#pragma once

#include <functional>

#include "Implementation/Beast/Common.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
// IConnector is responsible for establishing a connection to the server, including
// handling proxy connections and TLS handshakes. The result is a stream that is
// ready for the WebSocket handshake.
class IConnector {
  public:
    virtual ~IConnector() = default;

    using OnConnectCallback =
        std::function<void(beast::error_code, tcp::resolver::results_type::endpoint_type)>;

    virtual void Connect(const ServerSettings& settings, OnConnectCallback&& callback) = 0;
};
}  // namespace WS
