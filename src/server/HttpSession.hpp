#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>
#include <functional>
#include <optional>

namespace dataframe {
namespace server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

/**
 * Session HTTP - gère une connexion client
 */
class HttpSession : public std::enable_shared_from_this<HttpSession> {
public:
    explicit HttpSession(tcp::socket socket);

    void run();

private:
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void sendResponse(http::response<http::string_body> response);
    void onWrite(bool close, beast::error_code ec, std::size_t bytes_transferred);
    void doClose();

    // Traitement des requêtes
    http::response<http::string_body> handleRequest(
        http::request<http::string_body>&& req);

    // SSE streaming for graph execution
    void handleSseExecuteStream(const std::string& slug, unsigned version, bool keepAlive);
    void sendSseEvent(const std::string& eventType, const std::string& data);
    void closeSseConnection();

    beast::tcp_stream m_stream;
    beast::flat_buffer m_buffer;
    std::optional<http::request_parser<http::string_body>> m_parser;
    bool m_sseMode = false;  // True when handling SSE stream
};

} // namespace server
} // namespace dataframe
