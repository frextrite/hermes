#pragma once

#include <boost/asio/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>

namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
namespace ssl = boost::asio::ssl;        // from <boost/asio/ssl.hpp>
using tcp = boost::asio::ip::tcp;        // from <boost/asio/ip/tcp.hpp>

namespace WS {
static constexpr std::chrono::seconds ASYNC_TIMEOUT{ 5 };
static constexpr std::chrono::seconds PROXY_HANDSHAKE_TIMEOUT{ 10 };
}  // namespace WS

namespace WS {
// https://stackoverflow.com/a/53945549
// TODO: Use `static_assert(false, "message")` when we move to C++23
template <auto>
inline constexpr bool always_false = false;
}  // namespace WS

namespace WS {
static std::string GetDefaultUserAgent() {
    std::string os;
#if defined(_WIN32) || defined(WIN32)
    os = "Windows";
#elif defined(__APPLE__) || defined(__MACH__)
    os = "MacOS";
#elif defined(__linux__)
    os = "Linux";
#else
    os = "Unknown OS";
#endif
    static std::string user_agent = std::string(BOOST_BEAST_VERSION_STRING) +
                                    " (Hermes; " + os + ")";
    return user_agent;
}
}  // namespace WS
