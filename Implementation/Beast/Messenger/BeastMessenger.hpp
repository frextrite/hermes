#pragma once

#include <cassert>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>

#include "Implementation/Beast/Common.hpp"
#include "Implementation/Beast/Factory/BeastClientFactory.hpp"
#include "Implementation/Beast/SendPolicy/AsyncSendPolicy.hpp"
#include "Implementation/Beast/SendPolicy/BeastSendPolicy.hpp"
#include "Implementation/Beast/SendPolicy/SyncSendPolicy.hpp"
#include "Implementation/Internal/ClientCallbackInterfaces.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
template <SendBehaviorInternal SendBehaviorT, typename ClientFactoryT>
class BeastMessenger : public IWebSocketMessenger,
                       public IWebSocketClientCallback,
                       public IWriterOperator,
                       public ISendPolicyContext {
  private:
    using WebSocketClientT = typename ClientFactoryT::WebSocketClientT;

    struct ConnectionStatsInternal {
        std::atomic<size_t> total_messages_sent{ 0 };
        std::atomic<size_t> total_messages_received{ 0 };
        std::atomic<size_t> total_bytes_sent{ 0 };
        std::atomic<size_t> total_bytes_received{ 0 };
        std::atomic<size_t> current_send_queue_size{ 0 };
    };

  public:
    BeastMessenger(IWebSocketMessengerCallback& callback, const ConnectionConfig& config,
                   std::shared_ptr<ClientFactoryT> factory = nullptr,
                   std::shared_ptr<ISendPolicyFactory> send_policy_factory = nullptr)
        : messenger_callback_(callback),
          connection_config_(config),
          client_factory_(std::move(factory)),
          work_guard_(),
          ioc_(),
          ctx_(ssl::context::tlsv13_client),
          context_thread_(),
          client_(),
          reconnect_attempts_(0) {
        if (!client_factory_) {
            client_factory_ = std::make_shared<ClientFactoryT>();
        }
        InitializeSendPolicy(send_policy_factory);
    }

    ~BeastMessenger() { Close(); }

    // IWebSocketMessenger
    bool Open() override {
        if (!OpenInternal()) {
            Close();
            return false;
        }
        return true;
    }

    bool Send(std::string&& message) override {
        if (stop_requested_ || !send_policy_) {
            return false;
        }
        return send_policy_->Send(std::move(message));
    }

    void Close() override {
        stop_requested_ = true;

        net::post(ioc_, [this]() { CloseInternal(); });

        if (work_guard_) {
            work_guard_->reset();  // signal work guard there is no more work to do
            work_guard_.reset();
        }

        if (context_thread_.joinable() && std::this_thread::get_id() != context_thread_.get_id()) {
            context_thread_.join();
        }
    }

    ConnectionStats GetConnectionStats() const override {
        ConnectionStats stats;
        stats.total_messages_sent = stats_.total_messages_sent.load();
        stats.total_messages_received = stats_.total_messages_received.load();
        stats.total_bytes_sent = stats_.total_bytes_sent.load();
        stats.total_bytes_received = stats_.total_bytes_received.load();
        stats.current_send_queue_size = stats_.current_send_queue_size.load();
        return stats;
    }

    bool ScheduleReconnect(std::optional<ServerSettings> settings) override {
        if (stop_requested_) {
            return false;
        }

        bool expected = true;
        if (!std::atomic_compare_exchange_strong(&pending_critical_failure_handling_, &expected,
                                                 false)) {
            return false;
        }

        net::post(ioc_,
                  [this, settings = std::move(settings)] { StartReconnectInternal(settings); });
        return true;
    }

    // IWebSocketClientCallbackV2
    void OnMessageReceived(std::string_view message) override {
        stats_.total_messages_received++;
        stats_.total_bytes_received += message.size();
        messenger_callback_.OnMessageReceived(message);
    }

    void OnConnected() override {
        messenger_callback_.OnConnected();
        reconnect_attempts_ = 0;
        if (send_policy_) {
            send_policy_->OnConnected();
        }
    }

    void OnDisconnected(const ErrorDetails& error) override {
        messenger_callback_.OnDisconnected(error);
        WaitAndReconnect();
    }

    // IWriterOperator
    void OnMessageWriteCompleted(MessageWriteStatus status) override {
        if (send_policy_) {
            send_policy_->OnMessageWriteCompleted(status);
        }
    }

    // ISendPolicyContext
    bool IsClientConnected() const override { return client_ && client_->IsConnected(); }
    bool HasClient() const override { return static_cast<bool>(client_); }
    bool IsReadyForSynchronousSend() const override {
        return context_thread_.joinable() && HasClient();
    }
    bool IsInContextThread() const override {
        return std::this_thread::get_id() == context_thread_.get_id();
    }
    size_t GetMaxSendQueueSize() const override { return connection_config_.max_send_queue_size; }
    void PostToIOContext(std::function<void()> fn) override { net::post(ioc_, std::move(fn)); }
    bool ClientSend(const std::string& message) override {
        return client_ ? client_->Send(message) : false;
    }
    void IncrementCurrentQueueSize() override { stats_.current_send_queue_size++; }
    void DecrementCurrentQueueSize() override { stats_.current_send_queue_size--; }
    void RecordMessageSent(size_t message_size_bytes) override {
        stats_.total_messages_sent++;
        stats_.total_bytes_sent += message_size_bytes;
    }

  private:
    void InitializeSendPolicy(const std::shared_ptr<ISendPolicyFactory>& factory) {
        if constexpr (SendBehaviorT == SendBehaviorInternal::Custom) {
            if (!factory) {
                throw std::invalid_argument(
                    "Custom send policy factory must be provided for SendBehaviorInternal::Custom");
            }

            send_policy_ = factory->Create(*this);
        } else {
            if (factory) {
                throw std::invalid_argument(
                    "Custom send policy factory provided for non-custom SendBehaviorT");
            }

            if constexpr (SendBehaviorT == SendBehaviorInternal::Sync) {
                send_policy_ = std::make_shared<SyncSendPolicy>(*this);
            } else if constexpr (SendBehaviorT == SendBehaviorInternal::Async) {
                send_policy_ = std::make_shared<AsyncSendPolicy>(*this);
            } else {
                static_assert(always_false<SendBehaviorT>,
                              "Unsupported SendBehaviorT specified for BeastMessenger");
            }
        }
    }

    bool OpenInternal() {
        if (!InitializeTlsContext()) {
            return false;
        }

        work_guard_ =
            std::make_unique<boost::asio::executor_work_guard<net::io_context::executor_type>>(
                net::make_work_guard(ioc_));
        reconnect_timer_ = std::make_unique<boost::asio::steady_timer>(ioc_);
        context_thread_ = std::thread([this]() { RunIOContext(); });

        if (!CreateAndOpenClient()) {
            return false;
        }

        return true;
    }

    bool InitializeTlsContext() {
        boost::system::error_code ec;
        ctx_.set_verify_mode(ssl::verify_peer, ec);
        if (ec) {
            return false;
        }

#if _WIN32 || _WIN64
        if (::SSL_CTX_load_verify_store(ctx_.native_handle(), "org.openssl.winstore://") == 0) {
            return false;
        }
#else
        // TODO: Make sure this works on non-Debian systems that have CA certs injected as env
        // variable
        ctx_.set_default_verify_paths(ec);
        if (ec) {
            return false;
        }
#endif

        return true;
    }

    bool CreateAndOpenClient() {
        if (!client_factory_) {
            return false;
        }

        last_reconnect_attempt_ = std::chrono::steady_clock::now();

        client_ = client_factory_->CreateClient(*this, *this, connection_config_.server_settings,
                                                ioc_, ctx_);

        if (!client_->Open()) {
            return false;
        }

        return true;
    }

    void RunIOContext() { ioc_.run(); }

    void CloseInternal() {
        if (client_) {
            client_->Close();
            client_.reset();
        }

        if (reconnect_timer_) {
            reconnect_timer_->cancel();
        }
    }

    void StartReconnectInternal(std::optional<ServerSettings> settings) {
        if (settings.has_value()) {
            connection_config_.server_settings = *settings;
        }

        CloseInternal();

        reconnect_attempts_ = 0;
        HandleReconnect();
    }

    void HandleReconnect() {
        if (stop_requested_) {
            return;
        }

        reconnect_attempts_++;

        if (IsCriticalFailureThresholdBreached()) {
            pending_critical_failure_handling_ = true;
            messenger_callback_.SignalCriticalFailure();
            return;
        }

        if (CreateAndOpenClient()) {
            return;
        }

        WaitAndReconnect();
    }

    void WaitAndReconnect() {
        if (stop_requested_) {
            return;
        }

        // short-circuit the last wait
        if (IsCriticalFailureThresholdBreached()) {
            HandleReconnect();
            return;
        }

        auto wait_duration = std::chrono::steady_clock::now() - last_reconnect_attempt_;
        if (wait_duration < ReconnectDelay) {
            wait_duration = ReconnectDelay - wait_duration;
        } else {
            wait_duration = std::chrono::seconds(1);
        }

        reconnect_timer_->expires_after(wait_duration);
        reconnect_timer_->async_wait(beast::bind_front_handler(&BeastMessenger::OnReconnect, this));
    }

    void OnReconnect(const boost::system::error_code& ec) {
        if (ec == boost::asio::error::operation_aborted) {
            return;
        }

        HandleReconnect();
    }

    bool IsCriticalFailureThresholdBreached() const {
        return reconnect_attempts_ > connection_config_.critical_failure_threshold;
    }

  private:
    std::atomic<bool> stop_requested_{ false };
    std::atomic<bool> pending_critical_failure_handling_{ false };

    std::unique_ptr<boost::asio::executor_work_guard<net::io_context::executor_type>> work_guard_;
    std::unique_ptr<boost::asio::steady_timer> reconnect_timer_;
    net::io_context ioc_;
    ssl::context ctx_;
    std::thread context_thread_;

    ConnectionStatsInternal stats_;

    IWebSocketMessengerCallback& messenger_callback_;
    ConnectionConfig connection_config_;
    std::shared_ptr<ISendPolicy> send_policy_;
    std::shared_ptr<ClientFactoryT> client_factory_;
    std::shared_ptr<WebSocketClientT> client_;

    int reconnect_attempts_;
    std::chrono::steady_clock::time_point last_reconnect_attempt_;
    static constexpr std::chrono::seconds ReconnectDelay{ 5 };
};
}  // namespace WS
