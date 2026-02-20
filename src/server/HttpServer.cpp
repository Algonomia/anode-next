#include "server/HttpServer.hpp"
#include "server/HttpSession.hpp"
#include <iostream>

namespace dataframe {
namespace server {

HttpServer::HttpServer(net::io_context& ioc, const std::string& address, unsigned short port)
    : m_ioc(ioc)
    , m_acceptor(net::make_strand(ioc))
    , m_running(false)
{
    beast::error_code ec;

    auto endpoint = tcp::endpoint(net::ip::make_address(address), port);

    // Ouvrir l'acceptor
    m_acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        throw std::runtime_error("Failed to open acceptor: " + ec.message());
    }

    // Permettre la réutilisation de l'adresse
    m_acceptor.set_option(net::socket_base::reuse_address(true), ec);
    if (ec) {
        throw std::runtime_error("Failed to set reuse_address: " + ec.message());
    }

    // Lier à l'endpoint
    m_acceptor.bind(endpoint, ec);
    if (ec) {
        throw std::runtime_error("Failed to bind: " + ec.message());
    }

    // Commencer à écouter
    m_acceptor.listen(net::socket_base::max_listen_connections, ec);
    if (ec) {
        throw std::runtime_error("Failed to listen: " + ec.message());
    }

    std::cout << "Server listening on http://" << address << ":" << port << std::endl;
}

void HttpServer::run() {
    m_running = true;
    doAccept();
}

void HttpServer::stop() {
    m_running = false;
    m_acceptor.close();
}

void HttpServer::doAccept() {
    if (!m_running) return;

    m_acceptor.async_accept(
        net::make_strand(m_ioc),
        [this](beast::error_code ec, tcp::socket socket) {
            if (!ec) {
                std::make_shared<HttpSession>(std::move(socket))->run();
            }

            if (m_running) {
                doAccept();
            }
        });
}

} // namespace server
} // namespace dataframe
