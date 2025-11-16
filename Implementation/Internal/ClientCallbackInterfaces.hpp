#pragma once

#include <string_view>

#include "Include/WebSocketMessenger.hpp"

namespace WS {
class IWebSocketClientCallback {
  public:
    virtual ~IWebSocketClientCallback() = default;

    virtual void OnMessageReceived(std::string_view message) = 0;
    virtual void OnConnected() = 0;
    virtual void OnDisconnected(const ErrorDetails& error) = 0;
};

class IWriterOperator {
  public:
    virtual ~IWriterOperator() = default;

    virtual void OnMessageWriteCompleted(MessageWriteStatus status) = 0;
};
}  // namespace WS
