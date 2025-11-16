#include "Implementation/Beast/Connector/ProxyConnector.hpp"

#include <boost/beast/core/detail/base64.hpp>

namespace WS {
ProxyConnector::ProxyConnector(net::io_context& ioc,
                               websocket::stream<ssl::stream<beast::tcp_stream>>& ws)
    : direct_connector_(std::make_shared<DirectConnector>(ioc, ws)), ws_(ws) {}

void ProxyConnector::Connect(const ServerSettings& settings, OnConnectCallback&& callback) {
    pending_connect_callback_ = std::move(callback);
    server_settings_ = settings;

    // Create modified settings for the direct connector to connect to proxy instead of target
    // TODO: Handle missing proxy_settings
    ServerSettings proxy_settings = settings;
    proxy_settings.host = settings.proxy_settings->host;
    proxy_settings.port = settings.proxy_settings->port;

    // Use DirectConnector to establish connection to proxy
    direct_connector_->Connect(
        proxy_settings,
        beast::bind_front_handler(&ProxyConnector::OnDirectConnect, shared_from_this()));
}

void ProxyConnector::OnDirectConnect(beast::error_code ec,
                                     tcp::resolver::results_type::endpoint_type endpoint) {
    if (ec) {
        InvokeCallback(ec, endpoint);
        return;
    }

    beast::get_lowest_layer(ws_).expires_after(PROXY_HANDSHAKE_TIMEOUT);

    auto& request = proxy_request_.request;
    request.clear();

    request.method(beast::http::verb::connect);
    request.target(server_settings_.host + ":" + std::to_string(server_settings_.port));
    request.version(11);
    request.set(beast::http::field::host, server_settings_.proxy_settings->host);
    request.set(beast::http::field::user_agent, GetDefaultUserAgent());
    request.set(beast::http::field::proxy_connection, "Keep-Alive");
    request.set(beast::http::field::connection, "Keep-Alive");

    if (!server_settings_.proxy_settings->username.empty()) {
        const std::string& encoded_auth{ GetEncodedProxyAuth() };
        request.set(beast::http::field::proxy_authorization, encoded_auth);
    }

    beast::http::async_write(
        beast::get_lowest_layer(ws_), proxy_request_.request,
        beast::bind_front_handler(&ProxyConnector::OnProxyRequest, shared_from_this()));
}

void ProxyConnector::OnProxyRequest(beast::error_code ec, std::size_t) {
    if (ec) {
        InvokeCallback(ec);
        return;
    }

    beast::http::async_read_header(
        beast::get_lowest_layer(ws_), proxy_response_.buffer, proxy_response_.parser,
        beast::bind_front_handler(&ProxyConnector::OnProxyResponse, shared_from_this()));
}

void ProxyConnector::OnProxyResponse(beast::error_code ec, std::size_t) {
    if (ec) {
        InvokeCallback(ec);
        return;
    }

    beast::http::response<beast::http::empty_body>& proxy_response{ proxy_response_.parser.get() };
    if (proxy_response.result() != beast::http::status::ok) {
        return InvokeCallback(boost::system::errc::make_error_code(beast::errc::protocol_error));
    }

    InvokeCallback({});
}
std::string ProxyConnector::GetEncodedProxyAuth() const {
    const std::string auth =
        server_settings_.proxy_settings->username + ":" + server_settings_.proxy_settings->password;
    std::vector<char> base64_encoded_bytes(beast::detail::base64::encoded_size(auth.length()));
    size_t encoded_size =
        beast::detail::base64::encode(base64_encoded_bytes.data(), auth.data(), auth.length());
    std::string encoded_auth = "Basic " + std::string(base64_encoded_bytes.data(), encoded_size);
    return encoded_auth;
}

void ProxyConnector::InvokeCallback(beast::error_code ec,
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
