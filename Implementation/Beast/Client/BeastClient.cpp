#include "BeastClient.hpp"

#include <boost/beast/core/detail/base64.hpp>
#include <boost/beast/http.hpp>

#include "Implementation/Beast/Connector/DirectConnector.hpp"
#include "Implementation/Beast/Connector/ProxyConnector.hpp"

namespace WS {
BeastClient::BeastClient(IWebSocketClientCallback& callback, IWriterOperator& writer_callback,
                         const ServerSettings& settings, net::io_context& ioc, ssl::context& ctx)
    : callback_(callback),
      writer_callback_(writer_callback),
      server_settings_(settings),
      ioc_(ioc),
      ws_(net::make_strand(ioc), ctx) {
    if (server_settings_.proxy_settings) {
        connector_ = std::make_shared<ProxyConnector>(ioc, ws_);
    } else {
        connector_ = std::make_shared<DirectConnector>(ioc, ws_);
    }
}

BeastClient::~BeastClient() { Close(); }

bool BeastClient::Open() {
    last_error_.reset();
    should_stop_.store(false);
    connection_state_.store(ConnectionState::Ready);

    if (!SetupWS()) {
        return false;
    }

    connector_->Connect(server_settings_,
                        beast::bind_front_handler(&BeastClient::OnConnect, shared_from_this()));

    return true;
}

bool BeastClient::Send(std::string_view message) {
    if (connection_state_ != ConnectionState::Connected) {
        return false;
    }

    ws_.async_write(net::buffer(message),
                    beast::bind_front_handler(&BeastClient::OnWrite, shared_from_this()));

    return true;
}

void BeastClient::Close() {
    if (!PrepareClose()) {
        return;
    }

    CompleteClose();
}

bool BeastClient::IsConnected() const { return connection_state_ == ConnectionState::Connected; }

bool BeastClient::SetupWS() {
    if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(),
                                  server_settings_.host.c_str())) {
        return false;
    }

    ws_.next_layer().set_verify_callback(ssl::host_name_verification(server_settings_.host));

    return true;
}

bool BeastClient::PrepareClose() {
    bool expected = false;
    return should_stop_.compare_exchange_strong(expected, true);
}

void BeastClient::CloseInternal(beast::error_code ec) {
    if (!PrepareClose()) {
        return;
    }

    last_error_ = ec;

    CompleteClose();
}

void BeastClient::CompleteClose() {
    if (ws_.is_open()) {
        ws_.async_close(websocket::close_code::normal,
                        beast::bind_front_handler(&BeastClient::OnClose, shared_from_this()));
    } else {
        net::post(ioc_,
                  beast::bind_front_handler(&BeastClient::OnCloseInternal, shared_from_this()));
    }
}

void BeastClient::OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type) {
    if (ec) {
        CloseInternal(ec);
        return;
    }

    beast::get_lowest_layer(ws_).expires_after(ASYNC_TIMEOUT);

    ws_.next_layer().async_handshake(
        ssl::stream_base::client,
        beast::bind_front_handler(&BeastClient::OnTlsHandshake, shared_from_this()));
}

void BeastClient::OnTlsHandshake(beast::error_code ec) {
    if (ec) {
        CloseInternal(ec);
        return;
    }

    beast::get_lowest_layer(ws_).expires_never();
    ws_.set_option(websocket::stream_base::timeout::suggested(beast::role_type::client));

    const std::string& host = server_settings_.host + ":" + std::to_string(server_settings_.port);
    const std::string& target = server_settings_.target;
    ws_.async_handshake(host, target,
                        beast::bind_front_handler(&BeastClient::OnHandshake, shared_from_this()));
}

void BeastClient::OnHandshake(beast::error_code ec) {
    if (ec) {
        CloseInternal(ec);
        return;
    }

    connection_state_.store(ConnectionState::Connected);

    callback_.OnConnected();

    PerformRead();
}

void BeastClient::OnRead(beast::error_code ec, std::size_t bytesRead) {
    assert(read_buffer_.size() == bytesRead);

    if (ec) {
        CloseInternal(ec);
        return;
    }

    std::string message{ beast::buffers_to_string(read_buffer_.data()) };
    read_buffer_.consume(bytesRead);

    callback_.OnMessageReceived(message);

    PerformRead();
}

void BeastClient::OnWrite(beast::error_code ec, std::size_t) {
    if (ec) {
        CloseInternal(ec);
    }

    MessageWriteStatus status = ec ? MessageWriteStatus::Failure : MessageWriteStatus::Success;
    writer_callback_.OnMessageWriteCompleted(status);
}

void BeastClient::OnClose(beast::error_code) { OnCloseInternal(); }

void BeastClient::OnCloseInternal() {
    ConnectionState connection_state = connection_state_.load();

    if (connection_state != ConnectionState::Disconnected) {
        connection_state_.store(ConnectionState::Disconnected);
        callback_.OnDisconnected(GetLastErrorForReporting());
    }
}

void BeastClient::PerformRead() {
    ws_.async_read(read_buffer_,
                   beast::bind_front_handler(&BeastClient::OnRead, shared_from_this()));
}

ErrorDetails BeastClient::GetLastErrorForReporting() const {
    ErrorDetails error;
    if (last_error_) {
        error.message = last_error_->message();
        error.code = last_error_->value();
    } else {
        error.message = "Either the connection closed successfully, or an unexpected error occurred";
    }
    return error;
}
}  // namespace WS
