#include <WebSocketMessenger.hpp>

#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

namespace {
class SyncEchoCallback : public WS::IWebSocketMessengerCallback {
  public:
    void OnMessageReceived(std::string_view message) override {
                {
                        std::lock_guard<std::mutex> lock(mutex_);
                        ++messages_received_;
                }
                messages_cv_.notify_all();
        std::cout << "[sync echo] " << message << std::endl;
    }

    void OnConnected() override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connected_ = true;
        }
        connection_cv_.notify_all();
        std::cout << "Connected to server" << std::endl;
    }

    void OnDisconnected(const WS::ErrorDetails& error) override {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            connected_ = false;
        }
        connection_cv_.notify_all();
        std::cout << "Disconnected: " << error.message << " (code: " << error.code << ")" << std::endl;
    }

    void SignalCriticalFailure() override {
        std::cout << "Critical failure threshold reached" << std::endl;
    }

    bool WaitForConnection(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return connection_cv_.wait_for(lock, timeout, [this] { return connected_; });
    }

    bool WaitForMessages(size_t expected, std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);

        if (messages_received_ >= expected) {
            return true;
        }

        return messages_cv_.wait_for(lock, timeout,
                                     [this, expected] { return messages_received_ >= expected; });
    }

  private:
    std::mutex mutex_;
    std::condition_variable connection_cv_;
    std::condition_variable messages_cv_;
    bool connected_{ false };
    size_t messages_received_{ 0 };
};
}  // namespace

int main() {
    SyncEchoCallback callback;

    WS::ConnectionConfig config{};
    config.server_settings.host = "echo.websocket.org";
    config.server_settings.port = 443;
    config.server_settings.target = "/";
    config.max_send_queue_size = 16;

    auto messenger = WS::CreateWebSocketMessenger<WS::SendBehavior::Sync>(callback, config);
    if (!messenger) {
        std::cerr << "Failed to create messenger" << std::endl;
        return 1;
    }

    if (!messenger->Open()) {
        std::cerr << "Failed to open messenger" << std::endl;
        return 1;
    }

    if (!callback.WaitForConnection(std::chrono::seconds(10))) {
        std::cerr << "Timed out waiting for connection" << std::endl;
        messenger->Close();
        return 1;
    }

    const bool delivered = messenger->Send("synchronous message sent after connection");
    std::cout << "Send completed with status: " << (delivered ? "success" : "failure") << std::endl;

    if (!callback.WaitForMessages(1, std::chrono::seconds(10))) {
        std::cerr << "Timed out waiting for echo" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(2));

    messenger->Close();
}
