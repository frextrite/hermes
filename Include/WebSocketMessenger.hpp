#pragma once

#include <memory>
#include <optional>
#include <string>

namespace WS {
//
// Data for Messenger / Client
//

struct ProxySettings {
    std::string host;
    uint16_t port;
    // Optional credentials for proxy authentication
    std::string username;
    std::string password;
};

struct ServerSettings {
    std::string host;
    uint16_t port{ 443 };
    // Target includes the path and optional query parameters for the WebSocket connection.
    // For example, target containing an authentication token may look like "/ws?auth_token=secret"
    std::string target;
    std::optional<ProxySettings> proxy_settings;
};

struct ConnectionConfig {
    ServerSettings server_settings;
    const bool enable_tls{ true };  // non-secure is not supported
    int critical_failure_threshold{ 5 };
    size_t max_send_queue_size{ 1024 };
};

enum class SendBehavior {
    Sync,
    Async,
};

enum class MessageWriteStatus {
    Success,
    Failure,
    Timeout,
};

struct ErrorDetails {
    std::string message;
    int code{ 0 };  // Error code, if applicable
};

struct ConnectionStats {
    size_t total_messages_sent{ 0 };
    size_t total_messages_received{ 0 };
    size_t total_bytes_sent{ 0 };
    size_t total_bytes_received{ 0 };
    size_t current_send_queue_size{ 0 };
};

//
// Interfaces
//

class IWebSocketMessengerCallback {
  public:
    virtual ~IWebSocketMessengerCallback() = default;

    virtual void OnMessageReceived(std::string_view message) = 0;

    virtual void OnConnected() = 0;
    virtual void OnDisconnected(const ErrorDetails& error) = 0;

    // This callback function is called when the messenger has reached the critical failure
    // threshold and will not attempt to reconnect anymore. Call `ScheduleReconnect` on the
    // messenger with potentially updated settings to attempt a reconnection. See
    // `ScheduleReconnect` for more information.
    //
    // Callers should note that this is called from within the messenger's internal IO context
    // thread, and should avoid blocking operations in the implementation of this method.
    virtual void SignalCriticalFailure() = 0;
};

class IWebSocketMessenger {
  public:
    virtual ~IWebSocketMessenger() = default;

    // Schedules the connection to the server.
    virtual bool Open() = 0;

    // Sends a message asynchronously. Returns true if the message was accepted for sending.
    // Even if the send queue is full, the function will return true and the message will be
    // dropped silently.
    virtual bool Send(std::string&& message) = 0;

    // Closes the connection and stops the messenger. This is a blocking call and will return
    // only after all internal resources are cleaned up.
    virtual void Close() = 0;

    // Gets the current connection statistics.
    virtual ConnectionStats GetConnectionStats() const = 0;

    // Schedule a reconnect attempt if and only if the messenger has already signalled a critical
    // failure via `SignalCriticalFailure` callback.
    //
    // If `settings` is provided, the messenger will use the new settings for the reconnect attempt.
    //
    // Notes on correctness of the function:
    // This function MUST be called ONLY ONCE, IF AND ONLY IF the messenger has signalled a
    // critical failure via `SignalCriticalFailure` callback. Subsequent calls after the first one
    // and calls made when the messenger has not signalled a critical failure will be ignored.
    virtual bool ScheduleReconnect(std::optional<ServerSettings> settings) = 0;
};

template <SendBehavior SendBehaviorT>
std::shared_ptr<IWebSocketMessenger> CreateWebSocketMessenger(IWebSocketMessengerCallback& callback,
                                                              const ConnectionConfig& config);
}  // namespace WS
