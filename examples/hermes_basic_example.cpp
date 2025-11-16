#include <WebSocketMessenger.hpp>

#include <chrono>
#include <iostream>
#include <thread>

namespace {
class EchoCallback : public WS::IWebSocketMessengerCallback {
  public:
    void OnMessageReceived(std::string_view message) override {
        std::cout << "[echo] " << message << std::endl;
    }

    void OnConnected() override {
        std::cout << "Connected to server" << std::endl;
    }

    void OnDisconnected(const WS::ErrorDetails& error) override {
        std::cout << "Disconnected: " << error.message << " (code: " << error.code << ")" << std::endl;
    }

    void SignalCriticalFailure() override {
        std::cout << "Critical failure threshold reached" << std::endl;
    }
};
}  // namespace

int main(int charc, char** argv) {
    EchoCallback callback;

    WS::ConnectionConfig config{};
    config.server_settings.host = "echo.websocket.org";
    config.server_settings.port = 443;
    config.server_settings.target = "/";
    config.max_send_queue_size = 16;

    auto messenger = WS::CreateWebSocketMessenger<WS::SendBehavior::Async>(callback, config);
    if (!messenger) {
        std::cerr << "Failed to create messenger" << std::endl;
        return 1;
    }

    if (!messenger->Open()) {
        std::cerr << "Failed to open messenger" << std::endl;
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(1));

    if (!messenger->Send("sphinx of black quartz, judge my vow")) {
        std::cerr << "Send call was rejected" << std::endl;
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    messenger->Close();
}