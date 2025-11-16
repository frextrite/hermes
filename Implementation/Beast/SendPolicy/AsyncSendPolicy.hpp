#pragma once

#include <queue>
#include <string>

#include "BeastSendPolicy.hpp"

namespace WS {
// Asynchronous send policy preserves original queuing behavior.
class AsyncSendPolicy : public ISendPolicy {
  public:
    explicit AsyncSendPolicy(ISendPolicyContext& context) : context_(context) {}

    // Send will always queue the message even if Open() has not yet been called on the Messenger
    bool Send(std::string&& message) override {
        // Queue work onto IO context to preserve thread safety
        context_.PostToIOContext(
            [this, msg = std::move(message)]() mutable { SendMessageInternal(std::move(msg)); });

        return true;  // Accepted for sending (may be dropped later if queue full)
    }

    void OnMessageWriteCompleted(MessageWriteStatus status) override {
        // Connection closed or failed; leave queue intact for potential reconnect
        if (status != MessageWriteStatus::Success) {
            write_in_progress_ = false;
            return;
        }

        assert(!message_queue_.empty());

        context_.RecordMessageSent(message_queue_.front().size());
        message_queue_.pop();
        context_.DecrementCurrentQueueSize();

        write_in_progress_ = false;

        TryWriteNext();
    }

    void OnConnected() override { TryWriteNext(); }

  private:
    void SendMessageInternal(std::string&& message) {
        if (context_.GetMaxSendQueueSize() > 0 &&
            message_queue_.size() >= context_.GetMaxSendQueueSize()) {
            return;  // Silently drop message if queue is full
        }

        message_queue_.push(std::move(message));
        context_.IncrementCurrentQueueSize();

        TryWriteNext();
    }

    void TryWriteNext() {
        if (message_queue_.empty() || write_in_progress_ || !context_.HasClient() ||
            !context_.IsClientConnected()) {
            return;
        }

        const auto& message = message_queue_.front();
        write_in_progress_ = true;

        if (!context_.ClientSend(message)) {
            write_in_progress_ = false;
            return;  // Send failed; will retry on next OnConnected or OnMessageWriteCompleted
        }
    }

  private:
    ISendPolicyContext& context_;
    bool write_in_progress_{ false };
    std::queue<std::string> message_queue_;
};
}  // namespace WS
