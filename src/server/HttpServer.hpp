#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <string>
#include <functional>

namespace dataframe {
namespace server {

namespace beast = boost::beast;
namespace http = beast::http;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

// Forward declaration
class HttpSession;

/**
 * Serveur HTTP basé sur Boost.Beast
 */
class HttpServer {
public:
    HttpServer(net::io_context& ioc, const std::string& address, unsigned short port);

    void run();
    void stop();

private:
    void doAccept();

    net::io_context& m_ioc;
    tcp::acceptor m_acceptor;
    bool m_running;
};

} // namespace server
} // namespace dataframe
