#pragma once

#include "Implementation/Beast/Common.hpp"
#include "Implementation/Beast/Connector/IConnector.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
class DirectConnector : public IConnector, public std::enable_shared_from_this<DirectConnector> {
  public:
    explicit DirectConnector(net::io_context& ioc,
                             websocket::stream<ssl::stream<beast::tcp_stream>>& ws);

    void Connect(const ServerSettings& settings, OnConnectCallback&& callback) override;

  private:
    void OnResolve(beast::error_code ec, tcp::resolver::results_type results);
    void OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
    void InvokeCallback(beast::error_code ec,
                        tcp::resolver::results_type::endpoint_type endpoint = {});

  private:
    tcp::resolver resolver_;
    websocket::stream<ssl::stream<beast::tcp_stream>>& ws_;
    OnConnectCallback pending_connect_callback_;
};
}  // namespace WS
