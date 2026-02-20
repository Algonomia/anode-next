#include "server/HttpSession.hpp"
#include "server/RequestHandler.hpp"
#include "server/Logger.hpp"
#include "server/Profiler.hpp"
#include "nodes/NodeExecutor.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/NodeGraphSerializer.hpp"
#include "server/SessionManager.hpp"
#include "storage/GraphStorage.hpp"
#include <map>

namespace dataframe {
namespace server {

HttpSession::HttpSession(tcp::socket socket)
    : m_stream(std::move(socket))
{
}

void HttpSession::run() {
    net::dispatch(
        m_stream.get_executor(),
        beast::bind_front_handler(&HttpSession::doRead, shared_from_this()));
}

void HttpSession::doRead() {
    m_parser.emplace();
    m_parser->body_limit(50 * 1024 * 1024); // 50 MB
    m_stream.expires_after(std::chrono::seconds(30));

    http::async_read(
        m_stream,
        m_buffer,
        *m_parser,
        beast::bind_front_handler(&HttpSession::onRead, shared_from_this()));
}

void HttpSession::onRead(beast::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec == http::error::end_of_stream) {
        return doClose();
    }

    if (ec) {
        LOG_ERROR("Read error: " + ec.message());
        return;
    }

    auto response = handleRequest(m_parser->release());

    // If SSE mode was activated, the connection is handled by SSE methods
    // Don't send the placeholder response
    if (!m_sseMode) {
        sendResponse(std::move(response));
    }
}

void HttpSession::sendResponse(http::response<http::string_body> response) {
    auto sp = std::make_shared<http::response<http::string_body>>(std::move(response));
    bool needEof = sp->need_eof();

    http::async_write(
        m_stream,
        *sp,
        [self = shared_from_this(), sp, needEof](beast::error_code ec, std::size_t bytes) {
            self->onWrite(needEof, ec, bytes);
        });
}

void HttpSession::onWrite(bool close, beast::error_code ec, std::size_t /*bytes_transferred*/) {
    if (ec) {
        LOG_ERROR("Write error: " + ec.message());
        return;
    }

    if (close) {
        return doClose();
    }

    doRead();
}

void HttpSession::doClose() {
    beast::error_code ec;
    m_stream.socket().shutdown(tcp::socket::shutdown_send, ec);
}

// Création d'une réponse JSON
http::response<http::string_body> makeJsonResponse(
    http::status status,
    const json& body,
    unsigned version,
    bool keepAlive,
    uint64_t requestId)
{
    http::response<http::string_body> res{status, version};
    res.set(http::field::server, "AnodeServer/1.0");
    res.set(http::field::content_type, "application/json");
    res.set(http::field::cache_control, "no-store");
    res.set(http::field::access_control_allow_origin, "*");
    res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
    res.set(http::field::access_control_allow_headers, "Content-Type");
    res.keep_alive(keepAlive);
    res.body() = body.dump();
    res.prepare_payload();

    // Log response with request ID correlation
    Logger::instance().logResponse(requestId, static_cast<int>(status), res.body(), res.body().size());

    return res;
}

http::response<http::string_body> HttpSession::handleRequest(
    http::request<http::string_body>&& req)
{
    auto& handler = RequestHandler::instance();
    auto& logger = Logger::instance();
    std::string target(req.target());
    std::string method(req.method_string());

    // Log request and get request ID for correlation
    uint64_t requestId = logger.logRequest(method, target, req.body());

    // CORS preflight
    if (req.method() == http::verb::options) {
        http::response<http::string_body> res{http::status::ok, req.version()};
        res.set(http::field::server, "AnodeServer/1.0");
        res.set(http::field::access_control_allow_origin, "*");
        res.set(http::field::access_control_allow_methods, "GET, POST, PUT, DELETE, OPTIONS");
        res.set(http::field::access_control_allow_headers, "Content-Type");
        res.keep_alive(req.keep_alive());
        res.prepare_payload();
        logger.logResponse(requestId, 200, "", 0);
        return res;
    }

    // Run request validators (authentication)
    {
        std::map<std::string, std::string> cookies;
        auto it = req.find(http::field::cookie);
        if (it != req.end()) {
            std::string cookieStr(it->value());
            size_t pos = 0;
            while (pos < cookieStr.size()) {
                size_t eq = cookieStr.find('=', pos);
                if (eq == std::string::npos) break;
                size_t semi = cookieStr.find(';', eq);
                if (semi == std::string::npos) semi = cookieStr.size();
                std::string key = cookieStr.substr(pos, eq - pos);
                std::string val = cookieStr.substr(eq + 1, semi - eq - 1);
                // Trim whitespace
                while (!key.empty() && key.front() == ' ') key.erase(key.begin());
                while (!key.empty() && key.back() == ' ') key.pop_back();
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                while (!val.empty() && val.back() == ' ') val.pop_back();
                if (!key.empty()) cookies[key] = val;
                pos = semi + 1;
            }
        }
        auto validationResult = handler.validateRequest(method, target, cookies);
        if (validationResult) {
            auto [code, respJson] = *validationResult;
            return makeJsonResponse(
                static_cast<http::status>(code), respJson,
                req.version(), req.keep_alive(), requestId);
        }
    }

    // Helper to parse URL path segments
    auto parseSlugFromPath = [](const std::string& path, const std::string& prefix) -> std::string {
        if (path.length() <= prefix.length()) return "";
        std::string remaining = path.substr(prefix.length());
        // Remove trailing parts (e.g., /versions, /execute)
        size_t slashPos = remaining.find('/');
        if (slashPos != std::string::npos) {
            return remaining.substr(0, slashPos);
        }
        return remaining;
    };

    try {
        // GET /api/health
        if (req.method() == http::verb::get && target == "/api/health") {
            return makeJsonResponse(
                http::status::ok,
                handler.handleHealth(),
                req.version(),
                req.keep_alive(),
                requestId);
        }

        // GET /api/dataset/info
        if (req.method() == http::verb::get && target == "/api/dataset/info") {
            if (!handler.isLoaded()) {
                return makeJsonResponse(
                    http::status::service_unavailable,
                    json{{"status", "error"}, {"message", "No dataset loaded"}},
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }
            return makeJsonResponse(
                http::status::ok,
                handler.handleDatasetInfo(),
                req.version(),
                req.keep_alive(),
                requestId);
        }

        // POST /api/dataset/query
        if (req.method() == http::verb::post && target == "/api/dataset/query") {
            if (!handler.isLoaded()) {
                return makeJsonResponse(
                    http::status::service_unavailable,
                    json{{"status", "error"}, {"message", "No dataset loaded"}},
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            json requestBody;
            try {
                requestBody = json::parse(req.body());
            } catch (const json::parse_error& e) {
                return makeJsonResponse(
                    http::status::bad_request,
                    json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            return makeJsonResponse(
                http::status::ok,
                handler.handleQuery(requestBody),
                req.version(),
                req.keep_alive(),
                requestId);
        }

        // ============================================================
        // Node Definitions API
        // ============================================================

        // GET /api/nodes - List all node definitions
        if (req.method() == http::verb::get && target == "/api/nodes") {
            return makeJsonResponse(
                http::status::ok,
                handler.handleListNodes(),
                req.version(),
                req.keep_alive(),
                requestId);
        }

        // ============================================================
        // Graph API endpoints
        // ============================================================

        // GET /api/graphs - List all graphs
        if (req.method() == http::verb::get && target == "/api/graphs") {
            return makeJsonResponse(
                http::status::ok,
                handler.handleListGraphs(),
                req.version(),
                req.keep_alive(),
                requestId);
        }

        // POST /api/graph - Create a new graph
        if (req.method() == http::verb::post && target == "/api/graph") {
            json requestBody;
            try {
                requestBody = json::parse(req.body());
            } catch (const json::parse_error& e) {
                return makeJsonResponse(
                    http::status::bad_request,
                    json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            json result = handler.handleCreateGraph(requestBody);
            http::status status = result.value("status", "") == "ok"
                ? http::status::created
                : http::status::bad_request;

            return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
        }

        // Routes with :slug parameter
        const std::string graphPrefix = "/api/graph/";
        if (target.rfind(graphPrefix, 0) == 0 && target.length() > graphPrefix.length()) {
            std::string slug = parseSlugFromPath(target, graphPrefix);

            if (slug.empty()) {
                return makeJsonResponse(
                    http::status::bad_request,
                    json{{"status", "error"}, {"message", "Missing graph slug"}},
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            // Check for sub-routes
            std::string subPath = target.substr(graphPrefix.length() + slug.length());

            // GET /api/graph/:slug/versions
            if (req.method() == http::verb::get && subPath == "/versions") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleGetGraphVersions(slug),
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            // POST /api/graph/:slug/execute-stream (SSE streaming execution)
            if (req.method() == http::verb::post && subPath == "/execute-stream") {
                // Handle SSE streaming - this will not return a normal response
                handleSseExecuteStream(slug, req.version(), req.keep_alive());
                // Return empty response as a placeholder (actual response sent via SSE)
                http::response<http::string_body> res{http::status::ok, req.version()};
                res.set(http::field::content_length, "0");
                return res;  // This won't be used - connection handled by SSE methods
            }

            // POST /api/graph/:slug/execute
            if (req.method() == http::verb::post && subPath == "/execute") {
                json requestBody = json::object();
                if (!req.body().empty()) {
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(
                            http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(),
                            req.keep_alive(),
                            requestId);
                    }
                }

                json result = handler.handleExecuteGraph(slug, requestBody);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::internal_server_error;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }

            // POST /api/graph/:slug/execute-dynamic
            if (req.method() == http::verb::post && subPath == "/execute-dynamic") {
                json requestBody = json::object();
                if (!req.body().empty()) {
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(
                            http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(),
                            req.keep_alive(),
                            requestId);
                    }
                }

                json result = handler.handleExecuteDynamic(slug, requestBody);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::internal_server_error;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }

            // GET /api/graph/:slug/dynamic-equations
            if (req.method() == http::verb::get && subPath == "/dynamic-equations") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleGetDynamicEquations(slug),
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            // POST /api/graph/:slug/apply-dynamic
            if (req.method() == http::verb::post && subPath == "/apply-dynamic") {
                json requestBody = json::object();
                if (!req.body().empty()) {
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(
                            http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(),
                            req.keep_alive(),
                            requestId);
                    }
                }

                json result = handler.handleApplyDynamic(slug, requestBody);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::internal_server_error;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }

            // GET /api/graph/:slug/executions - List past executions
            if (req.method() == http::verb::get && subPath == "/executions") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleListExecutions(slug),
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            // ============================================================
            // Parameter Overrides
            // ============================================================

            // GET /api/graph/:slug/parameters - List all parameter overrides
            if (req.method() == http::verb::get && subPath == "/parameters") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleGetParameters(slug),
                    req.version(), req.keep_alive(), requestId);
            }

            // PUT/DELETE /api/graph/:slug/parameters/:identifier
            if (subPath.rfind("/parameters/", 0) == 0 && subPath.length() > 12) {
                std::string identifier = subPath.substr(12);

                // PUT /api/graph/:slug/parameters/:identifier
                if (req.method() == http::verb::put) {
                    json requestBody = json::object();
                    if (!req.body().empty()) {
                        try {
                            requestBody = json::parse(req.body());
                        } catch (const json::parse_error& e) {
                            return makeJsonResponse(http::status::bad_request,
                                json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                                req.version(), req.keep_alive(), requestId);
                        }
                    }

                    json result = handler.handleSetParameter(slug, identifier, requestBody);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok : http::status::bad_request;
                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }

                // DELETE /api/graph/:slug/parameters/:identifier
                if (req.method() == http::verb::delete_) {
                    json result = handler.handleDeleteParameter(slug, identifier);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok : http::status::not_found;
                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }
            }

            // ============================================================
            // Test Scenarios
            // ============================================================

            // GET /api/graph/:slug/scenarios - List scenarios
            if (req.method() == http::verb::get && subPath == "/scenarios") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleListScenarios(slug),
                    req.version(), req.keep_alive(), requestId);
            }

            // POST /api/graph/:slug/scenarios - Create scenario
            if (req.method() == http::verb::post && subPath == "/scenarios") {
                json requestBody = json::object();
                if (!req.body().empty()) {
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(), req.keep_alive(), requestId);
                    }
                }
                json result = handler.handleCreateScenario(slug, requestBody);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::created : http::status::bad_request;
                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }

            // POST /api/graph/:slug/scenarios/run-all (must come BEFORE :id matching)
            if (req.method() == http::verb::post && subPath == "/scenarios/run-all") {
                json result = handler.handleRunAllScenarios(slug);
                return makeJsonResponse(http::status::ok, result, req.version(), req.keep_alive(), requestId);
            }

            // Routes with /scenarios/:id
            if (subPath.rfind("/scenarios/", 0) == 0 && subPath.length() > 11) {
                std::string scenarioRest = subPath.substr(11); // after "/scenarios/"
                size_t nextSlash = scenarioRest.find('/');
                std::string idStr = (nextSlash != std::string::npos)
                    ? scenarioRest.substr(0, nextSlash) : scenarioRest;
                std::string scenarioSubPath = (nextSlash != std::string::npos)
                    ? scenarioRest.substr(nextSlash) : "";

                int64_t scenarioId;
                try {
                    scenarioId = std::stoll(idStr);
                } catch (...) {
                    return makeJsonResponse(http::status::bad_request,
                        json{{"status", "error"}, {"message", "Invalid scenario ID"}},
                        req.version(), req.keep_alive(), requestId);
                }

                // POST /api/graph/:slug/scenarios/:id/run
                if (req.method() == http::verb::post && scenarioSubPath == "/run") {
                    json result = handler.handleRunScenario(slug, scenarioId);
                    return makeJsonResponse(http::status::ok, result, req.version(), req.keep_alive(), requestId);
                }

                // GET /api/graph/:slug/scenarios/:id
                if (req.method() == http::verb::get && scenarioSubPath.empty()) {
                    json result = handler.handleGetScenario(slug, scenarioId);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok : http::status::not_found;
                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }

                // PUT /api/graph/:slug/scenarios/:id
                if (req.method() == http::verb::put && scenarioSubPath.empty()) {
                    json requestBody = json::object();
                    if (!req.body().empty()) {
                        try {
                            requestBody = json::parse(req.body());
                        } catch (const json::parse_error& e) {
                            return makeJsonResponse(http::status::bad_request,
                                json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                                req.version(), req.keep_alive(), requestId);
                        }
                    }
                    json result = handler.handleUpdateScenario(slug, scenarioId, requestBody);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok : http::status::bad_request;
                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }

                // DELETE /api/graph/:slug/scenarios/:id
                if (req.method() == http::verb::delete_ && scenarioSubPath.empty()) {
                    json result = handler.handleDeleteScenario(scenarioId);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok : http::status::not_found;
                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }
            }

            // GET /api/graph/:slug/outputs - List named outputs
            if (req.method() == http::verb::get && subPath == "/outputs") {
                return makeJsonResponse(
                    http::status::ok,
                    handler.handleListOutputs(slug),
                    req.version(),
                    req.keep_alive(),
                    requestId);
            }

            // POST /api/graph/:slug/output/:name - Get a named output DataFrame
            if (subPath.rfind("/output/", 0) == 0 && subPath.length() > 8) {
                std::string outputName = subPath.substr(8);  // Skip "/output/"

                if (req.method() == http::verb::post) {
                    json requestBody = json::object();
                    if (!req.body().empty()) {
                        try {
                            requestBody = json::parse(req.body());
                        } catch (const json::parse_error& e) {
                            return makeJsonResponse(http::status::bad_request,
                                json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                                req.version(), req.keep_alive(), requestId);
                        }
                    }

                    json result = handler.handleGetOutput(slug, outputName, requestBody);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok
                        : http::status::not_found;

                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }
            }

            // No sub-path: GET, PUT, DELETE /api/graph/:slug
            if (subPath.empty()) {
                // GET /api/graph/:slug - Get a graph
                if (req.method() == http::verb::get) {
                    json result = handler.handleGetGraph(slug);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok
                        : http::status::not_found;

                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }

                // PUT /api/graph/:slug - Update a graph (save new version)
                if (req.method() == http::verb::put) {
                    json requestBody;
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(
                            http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(),
                            req.keep_alive(),
                            requestId);
                    }

                    json result = handler.handleUpdateGraph(slug, requestBody);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok
                        : http::status::bad_request;

                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }

                // DELETE /api/graph/:slug - Delete a graph
                if (req.method() == http::verb::delete_) {
                    json result = handler.handleDeleteGraph(slug);
                    http::status status = result.value("status", "") == "ok"
                        ? http::status::ok
                        : http::status::not_found;

                    return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
                }
            }
        }

        // ============================================================
        // Execution API
        // GET /api/execution/:id
        // POST /api/execution/:id/restore
        // ============================================================
        const std::string executionPrefix = "/api/execution/";
        if (target.rfind(executionPrefix, 0) == 0 && target.length() > executionPrefix.length()) {
            std::string remaining = target.substr(executionPrefix.length());

            // Parse execution ID
            size_t slashPos = remaining.find('/');
            std::string idStr = (slashPos != std::string::npos)
                ? remaining.substr(0, slashPos)
                : remaining;

            int64_t executionId;
            try {
                executionId = std::stoll(idStr);
            } catch (...) {
                return makeJsonResponse(http::status::bad_request,
                    json{{"status", "error"}, {"message", "Invalid execution ID"}},
                    req.version(), req.keep_alive(), requestId);
            }

            std::string subPath = (slashPos != std::string::npos)
                ? remaining.substr(slashPos)
                : "";

            // POST /api/execution/:id/restore
            if (req.method() == http::verb::post && subPath == "/restore") {
                json result = handler.handleRestoreExecution(executionId);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::not_found;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }

            // GET /api/execution/:id
            if (req.method() == http::verb::get && subPath.empty()) {
                json result = handler.handleGetExecution(executionId);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::not_found;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }
        }

        // ============================================================
        // Session DataFrame API
        // POST /api/session/{sessionId}/dataframe/{nodeId}/{portName}
        // ============================================================
        const std::string sessionPrefix = "/api/session/";
        if (target.rfind(sessionPrefix, 0) == 0 && target.length() > sessionPrefix.length()) {
            // Parse: /api/session/{sessionId}/dataframe/{nodeId}/{portName}
            std::string remaining = target.substr(sessionPrefix.length());

            // Find sessionId
            size_t pos1 = remaining.find('/');
            if (pos1 == std::string::npos) {
                return makeJsonResponse(http::status::bad_request,
                    json{{"status", "error"}, {"message", "Invalid session path"}},
                    req.version(), req.keep_alive(), requestId);
            }
            std::string sessionId = remaining.substr(0, pos1);
            remaining = remaining.substr(pos1 + 1);

            // Expect "dataframe/"
            if (remaining.rfind("dataframe/", 0) != 0) {
                return makeJsonResponse(http::status::bad_request,
                    json{{"status", "error"}, {"message", "Expected /dataframe/ in path"}},
                    req.version(), req.keep_alive(), requestId);
            }
            remaining = remaining.substr(10); // Skip "dataframe/"

            // Find nodeId
            size_t pos2 = remaining.find('/');
            if (pos2 == std::string::npos) {
                return makeJsonResponse(http::status::bad_request,
                    json{{"status", "error"}, {"message", "Missing port name in path"}},
                    req.version(), req.keep_alive(), requestId);
            }
            std::string nodeId = remaining.substr(0, pos2);
            std::string portName = remaining.substr(pos2 + 1);

            if (req.method() == http::verb::post) {
                json requestBody = json::object();
                if (!req.body().empty()) {
                    try {
                        requestBody = json::parse(req.body());
                    } catch (const json::parse_error& e) {
                        return makeJsonResponse(http::status::bad_request,
                            json{{"status", "error"}, {"message", "Invalid JSON: " + std::string(e.what())}},
                            req.version(), req.keep_alive(), requestId);
                    }
                }

                json result = handler.handleSessionDataFrame(sessionId, nodeId, portName, requestBody);
                http::status status = result.value("status", "") == "ok"
                    ? http::status::ok
                    : http::status::not_found;

                return makeJsonResponse(status, result, req.version(), req.keep_alive(), requestId);
            }
        }

        // Try plugin routes before returning 404
        {
            json body = json::object();
            if (!req.body().empty()) {
                try { body = json::parse(req.body()); } catch (...) {}
            }
            auto pluginResult = handler.tryPluginRoutes(method, target, body);
            if (pluginResult) {
                auto [code, respJson] = *pluginResult;
                return makeJsonResponse(
                    static_cast<http::status>(code), respJson,
                    req.version(), req.keep_alive(), requestId);
            }
        }

        // 404 Not Found
        return makeJsonResponse(
            http::status::not_found,
            json{{"status", "error"}, {"message", "Not found: " + target}},
            req.version(),
            req.keep_alive(),
            requestId);

    } catch (const std::exception& e) {
        return makeJsonResponse(
            http::status::internal_server_error,
            json{{"status", "error"}, {"message", e.what()}},
            req.version(),
            req.keep_alive(),
            requestId);
    }
}

// =============================================================================
// SSE Streaming Methods for Real-time Graph Execution
// =============================================================================

void HttpSession::handleSseExecuteStream(const std::string& slug, unsigned /*version*/, bool /*keepAlive*/) {
    m_sseMode = true;
    auto& handler = RequestHandler::instance();

    // Disable timeout for streaming
    m_stream.expires_never();

    // Send SSE headers synchronously
    std::string headers =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/event-stream\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: keep-alive\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "\r\n";

    beast::error_code ec;
    net::write(m_stream.socket(), net::buffer(headers), ec);
    if (ec) {
        LOG_ERROR("SSE header write error: " + ec.message());
        return;
    }

    // Load graph
    auto* graphStorage = handler.getGraphStorage();
    if (!graphStorage) {
        sendSseEvent("error", R"({"message":"Graph storage not initialized"})");
        closeSseConnection();
        return;
    }

    nodes::NodeGraph graph;
    try {
        graph = graphStorage->loadGraph(slug);
    } catch (const std::exception& e) {
        sendSseEvent("error", "{\"message\":\"Failed to load graph: " + std::string(e.what()) + "\"}");
        closeSseConnection();
        return;
    }

    // Create session for storing results
    auto& sessionMgr = SessionManager::instance();
    std::string sessionId = sessionMgr.createSession();

    // Send start event
    json startEvent = {
        {"session_id", sessionId},
        {"node_count", graph.nodeCount()}
    };
    sendSseEvent("execution_start", startEvent.dump());

    // Create executor with callback for real-time events
    nodes::NodeExecutor executor(nodes::NodeRegistry::instance());

    // Track results for CSV storage
    std::unordered_map<std::string, std::unordered_map<std::string, nodes::Workload>> allResults;

    executor.setExecutionCallback([this, &sessionMgr, &sessionId, &allResults](const nodes::ExecutionEvent& evt) {
        json eventJson = evt.toJson();
        eventJson["session_id"] = sessionId;

        std::string eventType;
        switch (evt.status) {
            case nodes::ExecutionStatus::Started:
                eventType = "node_started";
                break;
            case nodes::ExecutionStatus::Completed:
                eventType = "node_completed";
                break;
            case nodes::ExecutionStatus::Failed:
                eventType = "node_failed";
                break;
        }

        sendSseEvent(eventType, eventJson.dump());
    });

    // Execute the graph
    try {
        allResults = executor.execute(graph);

        // Store all CSV results in session
        for (const auto& [nodeId, outputs] : allResults) {
            for (const auto& [portName, workload] : outputs) {
                if (workload.getType() == nodes::NodeType::Csv) {
                    auto df = workload.getCsv();
                    if (df) {
                        sessionMgr.storeDataFrame(sessionId, nodeId, portName, df);
                    }
                }
            }
        }

        // Send completion event
        json completeEvent = {
            {"session_id", sessionId},
            {"has_errors", executor.hasErrors()}
        };
        sendSseEvent("execution_complete", completeEvent.dump());

    } catch (const std::exception& e) {
        json errorEvent = {
            {"message", e.what()}
        };
        sendSseEvent("error", errorEvent.dump());
    }

    closeSseConnection();
}

void HttpSession::sendSseEvent(const std::string& eventType, const std::string& data) {
    if (!m_sseMode) return;

    std::string sseMessage = "event: " + eventType + "\ndata: " + data + "\n\n";

    beast::error_code ec;
    net::write(m_stream.socket(), net::buffer(sseMessage), ec);
    if (ec) {
        LOG_ERROR("SSE event write error: " + ec.message());
    }
}

void HttpSession::closeSseConnection() {
    m_sseMode = false;
    doClose();
}

} // namespace server
} // namespace dataframe
