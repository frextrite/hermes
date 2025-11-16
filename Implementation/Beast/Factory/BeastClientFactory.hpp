#pragma once

#include "Implementation/Beast/Client/BeastClient.hpp"

namespace WS {
class BeastClientFactory {
  public:
    using WebSocketClientT = BeastClient;
  public:
    explicit BeastClientFactory() = default;

    std::shared_ptr<WebSocketClientT> CreateClient(IWebSocketClientCallback& callback,
                                                   IWriterOperator& writer_callback,
                                                   const ServerSettings& settings, net::io_context& ioc,
                                                   ssl::context& ctx) {
        return std::make_shared<WebSocketClientT>(callback, writer_callback, settings, ioc, ctx);
    }
};
}  // namespace WS
