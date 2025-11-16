#pragma once

#include <boost/beast/http.hpp>

#include "Implementation/Beast/Common.hpp"
#include "Implementation/Beast/Connector/DirectConnector.hpp"
#include "Implementation/Beast/Connector/IConnector.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
class ProxyConnector : public IConnector, public std::enable_shared_from_this<ProxyConnector> {
  private:
    struct ProxyRequest {
        beast::http::request<beast::http::empty_body> request;
    };

    struct ProxyResponse {
        beast::flat_buffer buffer;
        beast::http::response_parser<beast::http::empty_body> parser;
    };

  public:
    explicit ProxyConnector(net::io_context& ioc,
                            websocket::stream<ssl::stream<beast::tcp_stream>>& ws);

    void Connect(const ServerSettings& settings, OnConnectCallback&& callback) override;

  private:
    void OnDirectConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type endpoint);
    void OnProxyRequest(beast::error_code ec, std::size_t);
    void OnProxyResponse(beast::error_code ec, std::size_t);
    void InvokeCallback(beast::error_code ec,
                        tcp::resolver::results_type::endpoint_type endpoint = {});

    std::string GetEncodedProxyAuth() const;

  private:
    std::shared_ptr<DirectConnector> direct_connector_;
    websocket::stream<ssl::stream<beast::tcp_stream>>& ws_;
    ServerSettings server_settings_;
    OnConnectCallback pending_connect_callback_;
    ProxyRequest proxy_request_;
    ProxyResponse proxy_response_;
};
}  // namespace WS
