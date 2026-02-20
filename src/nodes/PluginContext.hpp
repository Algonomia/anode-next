#pragma once

#include <string>
#include <map>

namespace boost::asio { class io_context; }
namespace storage { class GraphStorage; }
namespace dataframe::server { class RequestHandler; }

namespace nodes {

/**
 * Context passed to node plugins during initialization.
 * Provides access to core application resources.
 */
struct PluginContext {
    boost::asio::io_context* ioc = nullptr;
    storage::GraphStorage* storage = nullptr;
    dataframe::server::RequestHandler* handler = nullptr;
    std::string dbConnString;
    std::map<std::string, std::string> params;
};

} // namespace nodes
