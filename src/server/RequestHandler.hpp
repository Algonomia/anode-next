#pragma once

#include "dataframe/DataFrame.hpp"
#include "storage/GraphStorage.hpp"
#include <nlohmann/json.hpp>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <chrono>
#include <vector>

namespace dataframe {
namespace server {

using json = nlohmann::json;

/// Route handler result: {HTTP status code, JSON body}
using RouteResult = std::pair<unsigned, json>;
/// Plugin route handler: returns nullopt if route not matched
using RouteHandler = std::function<
    std::optional<RouteResult>(const std::string& method,
                               const std::string& target,
                               const json& body)>;
/// Request validator: returns a RouteResult (e.g. 401) to block, or nullopt to allow
using RequestValidator = std::function<
    std::optional<RouteResult>(const std::string& method,
                               const std::string& target,
                               const std::map<std::string, std::string>& cookies)>;

/**
 * Gestionnaire de requêtes - traite la logique métier
 */
class RequestHandler {
public:
    static RequestHandler& instance();

    // Initialisation avec le dataset
    void loadDataset(const std::string& csvPath);
    bool isLoaded() const { return m_dataset != nullptr; }

    // Initialisation du stockage de graphes
    void initGraphStorage(const std::string& dbPath);
    bool hasGraphStorage() const { return m_graphStorage != nullptr; }
    storage::GraphStorage* getGraphStorage() { return m_graphStorage.get(); }

    // Handlers pour les endpoints dataset
    json handleHealth();
    json handleDatasetInfo();
    json handleQuery(const json& request);

    // Handlers pour les endpoints nodes
    json handleListNodes();

    // Handlers pour les endpoints graph
    json handleListGraphs();
    json handleGetGraph(const std::string& slug);
    json handleGetGraphVersions(const std::string& slug);
    json handleCreateGraph(const json& request);
    json handleUpdateGraph(const std::string& slug, const json& request);
    json handleDeleteGraph(const std::string& slug);
    json handleExecuteGraph(const std::string& slug, const json& request,
                            const nodes::CsvOverrides& csvOverrides = {});
    json handleExecuteDynamic(const std::string& slug, const json& request);
    json handleApplyDynamic(const std::string& slug, const json& request);
    json handleGetDynamicEquations(const std::string& slug);

    // Handler pour les endpoints session (DataFrame visualization)
    json handleSessionDataFrame(const std::string& sessionId,
                                const std::string& nodeId,
                                const std::string& portName,
                                const json& request);

    // Handlers pour les endpoints execution (persistence)
    json handleListExecutions(const std::string& slug);
    json handleGetExecution(int64_t executionId);
    json handleRestoreExecution(int64_t executionId);

    // Handlers pour les endpoints outputs (named outputs)
    json handleListOutputs(const std::string& slug);
    json handleGetOutput(const std::string& slug, const std::string& name, const json& request);

    // Handlers pour les endpoints parameter overrides (viewer parameters)
    json handleGetParameters(const std::string& slug);
    json handleSetParameter(const std::string& slug, const std::string& identifier, const json& request);
    json handleDeleteParameter(const std::string& slug, const std::string& identifier);

    // Plugin route extension
    void registerRouteHandler(RouteHandler handler);
    std::optional<RouteResult> tryPluginRoutes(
        const std::string& method, const std::string& target,
        const json& body) const;

    // Request validation (authentication, authorization)
    void registerRequestValidator(RequestValidator validator);
    std::optional<RouteResult> validateRequest(
        const std::string& method, const std::string& target,
        const std::map<std::string, std::string>& cookies) const;

    // Handlers pour les endpoints test scenarios
    json handleListScenarios(const std::string& slug);
    json handleCreateScenario(const std::string& slug, const json& request);
    json handleGetScenario(const std::string& slug, int64_t scenarioId);
    json handleUpdateScenario(const std::string& slug, int64_t scenarioId, const json& request);
    json handleDeleteScenario(int64_t scenarioId);
    json handleRunScenario(const std::string& slug, int64_t scenarioId);
    json handleRunAllScenarios(const std::string& slug);

private:
    RequestHandler() = default;
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler& operator=(const RequestHandler&) = delete;

    // Exécute une opération sur le DataFrame
    std::shared_ptr<DataFrame> executeOperation(
        std::shared_ptr<DataFrame> df,
        const std::string& type,
        const json& params);

    // Detect event links from timeline_output nodes and save them
    void detectAndSaveLinks(const std::string& slug, const nodes::NodeGraph& nodeGraph);

    std::shared_ptr<DataFrame> m_dataset;
    std::string m_datasetPath;
    size_t m_originalRows = 0;

    // Stockage de graphes
    std::unique_ptr<storage::GraphStorage> m_graphStorage;

    // Plugin route handlers
    std::vector<RouteHandler> m_pluginRouteHandlers;

    // Request validators (authentication)
    std::vector<RequestValidator> m_requestValidators;
};

} // namespace server
} // namespace dataframe
