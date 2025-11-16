#include "Implementation/Beast/Client/BeastClient.hpp"
#include "Implementation/Beast/Factory/BeastClientFactory.hpp"
#include "Implementation/Beast/Messenger/BeastMessenger.hpp"
#include "Implementation/Beast/SendPolicy/AsyncSendPolicy.hpp"
#include "Implementation/Beast/SendPolicy/SyncSendPolicy.hpp"

namespace WS {
template <SendBehavior SendBehaviorT>
std::shared_ptr<IWebSocketMessenger> CreateWebSocketMessenger(IWebSocketMessengerCallback& callback,
                                                              const ConnectionConfig& config) {
    if constexpr (SendBehaviorT == SendBehavior::Sync) {
        return std::make_shared<BeastMessenger<SendBehaviorInternal::Sync, BeastClientFactory>>(
            callback, config);
    } else if constexpr (SendBehaviorT == SendBehavior::Async) {
        return std::make_shared<BeastMessenger<SendBehaviorInternal::Async, BeastClientFactory>>(
            callback, config);
    } else {
        static_assert(always_false<SendBehaviorT>,
                      "Unsupported SendBehavior specified for CreateWebSocketMessenger");
    }
}

template std::shared_ptr<IWebSocketMessenger> CreateWebSocketMessenger<SendBehavior::Sync>(
    IWebSocketMessengerCallback& callback, const ConnectionConfig& config);

template std::shared_ptr<IWebSocketMessenger> CreateWebSocketMessenger<SendBehavior::Async>(
    IWebSocketMessengerCallback& callback, const ConnectionConfig& config);
}  // namespace WS
