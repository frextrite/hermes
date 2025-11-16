#pragma once

#include <functional>
#include <memory>
#include <string>

#include "Implementation/Internal/ClientCallbackInterfaces.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
enum class SendBehaviorInternal {
    Sync,
    Async,
    Custom,
};

class ISendPolicyContext {
  public:
    virtual ~ISendPolicyContext() = default;

    virtual bool IsClientConnected() const = 0;
    virtual bool HasClient() const = 0;
    virtual bool IsReadyForSynchronousSend() const = 0;
    virtual bool IsInContextThread() const = 0;
    virtual size_t GetMaxSendQueueSize() const = 0;
    virtual void PostToIOContext(std::function<void()> fn) = 0;
    virtual bool ClientSend(const std::string& message) = 0;
    virtual void IncrementCurrentQueueSize() = 0;
    virtual void DecrementCurrentQueueSize() = 0;
    virtual void RecordMessageSent(size_t message_size_bytes) = 0;
};

class ISendPolicy {
  public:
    virtual ~ISendPolicy() = default;

    virtual bool Send(std::string&& message) = 0;  // Depending on policy may block.
    virtual void OnMessageWriteCompleted(MessageWriteStatus status) = 0;
    virtual void OnConnected() {}
};

class ISendPolicyFactory {
  public:
    virtual ~ISendPolicyFactory() = default;

    virtual std::shared_ptr<ISendPolicy> Create(ISendPolicyContext& context) = 0;
};
}  // namespace WS
