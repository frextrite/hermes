#pragma once

#include <condition_variable>
#include <future>
#include <mutex>
#include <string>

#include "BeastSendPolicy.hpp"

namespace WS {
// Synchronous send policy: blocks caller thread until completion. Single in-flight send.
class SyncSendPolicy : public ISendPolicy {
  private:
    struct Payload {
        std::string message;
        std::promise<bool> write_promise;
    };

  public:
    explicit SyncSendPolicy(ISendPolicyContext& context) : context_(context) {}

    bool Send(std::string&& message) override {
        // Cannot perform synchronous send before Open() starts IO context
        if (!context_.IsReadyForSynchronousSend()) {
            return false;  // Messenger not ready
        }

        // Avoid deadlock if called from IO context thread
        if (context_.IsInContextThread()) {
            return false;  // Cannot perform blocking send from IO context thread
        }

        std::promise<bool> write_promise = std::promise<bool>();
        auto future = write_promise.get_future();

        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this] { return !active_send_; });

            // Prepare for send
            active_send_ = true;
            send_payload_.message = std::move(message);
            send_payload_.write_promise = std::move(write_promise);

            context_.IncrementCurrentQueueSize();

            // TODO: Evaluate if shared_from_this is needed here
            context_.PostToIOContext([this]() mutable { SendMessageInternal(); });
        }

        bool result = future.get();  // Block until completion
        return result;
    }

    void OnMessageWriteCompleted(MessageWriteStatus status) override {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!active_send_) {
            return;  // Spurious callback (should not happen)
        }

        if (status == MessageWriteStatus::Success) {
            context_.RecordMessageSent(send_payload_.message.size());
        }

        MarkWriteComplete(status == MessageWriteStatus::Success);
    }

  private:
    void SendMessageInternal() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!context_.IsClientConnected()) {
            MarkWriteComplete(false);
            return;
        }

        bool accepted = context_.ClientSend(send_payload_.message);
        if (!accepted) {
            MarkWriteComplete(false);
        }
        // Success path will be handled in OnMessageWriteCompleted
    }

    void MarkWriteComplete(bool status) {
        if (!active_send_) {
            return;  // Spurious callback (should not happen)
        }

        context_.DecrementCurrentQueueSize();
        active_send_ = false;
        SafeSetPromise(status);
        send_payload_.message.clear();
        cv_.notify_one();
    }

    void SafeSetPromise(bool value) {
        // Guard against multiple sets in edge cases
        try {
            send_payload_.write_promise.set_value(value);
        } catch (...) {
            // Ignore if already set
        }
    }

  private:
    ISendPolicyContext& context_;
    std::mutex mutex_;
    std::condition_variable cv_;
    bool active_send_{ false };
    Payload send_payload_;
};
}  // namespace WS
