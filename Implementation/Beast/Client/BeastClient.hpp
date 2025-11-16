#pragma once

#include <boost/beast/http.hpp>
#include <memory>

#include "Implementation/Beast/Common.hpp"
#include "Implementation/Beast/Connector/IConnector.hpp"
#include "Implementation/Internal/ClientCallbackInterfaces.hpp"
#include "Include/WebSocketMessenger.hpp"

namespace WS {
class BeastClient : public std::enable_shared_from_this<BeastClient> {
  private:
    enum class ConnectionState {
        Ready,
        Connected,
        Disconnected,
    };

  public:
    explicit BeastClient(IWebSocketClientCallback& callback, IWriterOperator& writer_callback,
                         const ServerSettings& settings, net::io_context& ioc, ssl::context& ctx);
    ~BeastClient();

    bool Open();
    bool Send(std::string_view message);
    void Close();

    bool IsConnected() const;

  private:
    bool SetupWS();

    bool PrepareClose();
    void CloseInternal(beast::error_code ec);
    void CompleteClose();

    void OnConnect(beast::error_code ec, tcp::resolver::results_type::endpoint_type);
    void OnTlsHandshake(beast::error_code ec);
    void OnHandshake(beast::error_code ec);
    void OnRead(beast::error_code ec, std::size_t bytes_read);
    void OnWrite(beast::error_code ec, std::size_t);
    void OnClose(beast::error_code);

    void OnCloseInternal();

    void PerformRead();

    ErrorDetails GetLastErrorForReporting() const;

  private:
    std::atomic<ConnectionState> connection_state_{ ConnectionState::Ready };
    std::atomic<bool> should_stop_{ false };

    std::shared_ptr<IConnector> connector_;

    websocket::stream<ssl::stream<beast::tcp_stream>> ws_;
    beast::flat_buffer read_buffer_;
    std::optional<beast::error_code> last_error_;  // Last error encountered during operations

    IWebSocketClientCallback& callback_;
    IWriterOperator& writer_callback_;
    net::io_context& ioc_;
    ServerSettings server_settings_;
};
}  // namespace WS
