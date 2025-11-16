#include "Implementation/Beast/Connector/DirectConnector.hpp"

namespace WS {
DirectConnector::DirectConnector(net::io_context& ioc,
                                 websocket::stream<ssl::stream<beast::tcp_stream>>& ws)
    : resolver_(ioc), ws_(ws) {}

void DirectConnector::Connect(const ServerSettings& settings, OnConnectCallback&& callback) {
    pending_connect_callback_ = std::move(callback);

    resolver_.async_resolve(
        settings.host, std::to_string(settings.port),
        beast::bind_front_handler(&DirectConnector::OnResolve, shared_from_this()));
}

void DirectConnector::OnResolve(beast::error_code ec, tcp::resolver::results_type results) {
    if (ec) {
        InvokeCallback(ec);
        return;
    }

    beast::get_lowest_layer(ws_).expires_after(ASYNC_TIMEOUT);
    beast::get_lowest_layer(ws_).async_connect(
        results, beast::bind_front_handler(&DirectConnector::OnConnect, shared_from_this()));
}

void DirectConnector::OnConnect(beast::error_code ec,
                                tcp::resolver::results_type::endpoint_type endpoint) {
    InvokeCallback(ec, endpoint);
}

void DirectConnector::InvokeCallback(beast::error_code ec,
                                     tcp::resolver::results_type::endpoint_type endpoint) {
    // pending_connect_callback_ captures a shared reference to the caller,
    // and the caller holds a shared reference to this connector,
    // so we need to clear it before invoking the callback to avoid potential cycles
    auto callback = std::exchange(pending_connect_callback_, {});
    if (callback) {
        callback(ec, endpoint);
    }
}
}  // namespace WS
