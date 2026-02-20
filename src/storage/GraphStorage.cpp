#include "storage/GraphStorage.hpp"
#include "nodes/NodeGraphSerializer.hpp"
#include "dataframe/DataFrameSerializer.hpp"
#include <sqlite3.h>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace storage {

using json = nlohmann::json;

// =============================================================================
// Helper Functions
// =============================================================================

namespace {

/**
 * Get current UTC timestamp in ISO 8601 format
 */
std::string currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count() << 'Z';
    return oss.str();
}

/**
 * Convert vector of tags to JSON array string
 */
std::string tagsToJson(const std::vector<std::string>& tags) {
    json j = tags;
    return j.dump();
}

/**
 * Convert JSON array string to vector of tags
 */
std::vector<std::string> jsonToTags(const std::string& jsonStr) {
    if (jsonStr.empty()) {
        return {};
    }
    try {
        json j = json::parse(jsonStr);
        return j.get<std::vector<std::string>>();
    } catch (...) {
        return {};
    }
}

/**
 * RAII wrapper for SQLite prepared statements
 */
class Statement {
public:
    Statement(sqlite3* db, const std::string& sql) : m_stmt(nullptr) {
        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &m_stmt, nullptr) != SQLITE_OK) {
            throw std::runtime_error("Failed to prepare statement: " +
                                     std::string(sqlite3_errmsg(db)));
        }
    }

    ~Statement() {
        if (m_stmt) {
            sqlite3_finalize(m_stmt);
        }
    }

    Statement(const Statement&) = delete;
    Statement& operator=(const Statement&) = delete;

    sqlite3_stmt* get() { return m_stmt; }

    void bindText(int index, const std::string& value) {
        sqlite3_bind_text(m_stmt, index, value.c_str(), -1, SQLITE_TRANSIENT);
    }

    void bindInt64(int index, int64_t value) {
        sqlite3_bind_int64(m_stmt, index, value);
    }

    void bindNull(int index) {
        sqlite3_bind_null(m_stmt, index);
    }

    bool step() {
        int result = sqlite3_step(m_stmt);
        if (result == SQLITE_ROW) return true;
        if (result == SQLITE_DONE) return false;
        throw std::runtime_error("Step failed: " +
                                 std::string(sqlite3_errmsg(sqlite3_db_handle(m_stmt))));
    }

    std::string getText(int col) {
        const char* text = reinterpret_cast<const char*>(sqlite3_column_text(m_stmt, col));
        return text ? text : "";
    }

    int64_t getInt64(int col) {
        return sqlite3_column_int64(m_stmt, col);
    }

    bool isNull(int col) {
        return sqlite3_column_type(m_stmt, col) == SQLITE_NULL;
    }

    void reset() {
        sqlite3_reset(m_stmt);
        sqlite3_clear_bindings(m_stmt);
    }

private:
    sqlite3_stmt* m_stmt;
};

} // anonymous namespace

// =============================================================================
// GraphStorage::Impl
// =============================================================================

class GraphStorage::Impl {
public:
    explicit Impl(const std::string& dbPath) : m_dbPath(dbPath), m_db(nullptr) {
        if (sqlite3_open(dbPath.c_str(), &m_db) != SQLITE_OK) {
            throw std::runtime_error("Failed to open database: " +
                                     std::string(sqlite3_errmsg(m_db)));
        }

        // Enable foreign keys
        exec("PRAGMA foreign_keys = ON");

        // Create tables
        createTables();
    }

    ~Impl() {
        if (m_db) {
            sqlite3_close(m_db);
        }
    }

    void exec(const std::string& sql) {
        char* errMsg = nullptr;
        if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errMsg) != SQLITE_OK) {
            std::string error = errMsg ? errMsg : "Unknown error";
            sqlite3_free(errMsg);
            throw std::runtime_error("SQL error: " + error);
        }
    }

    void createTables() {
        exec(R"(
            CREATE TABLE IF NOT EXISTS graphs (
                slug TEXT PRIMARY KEY,
                name TEXT NOT NULL,
                description TEXT,
                author TEXT,
                tags TEXT,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            )
        )");

        exec(R"(
            CREATE TABLE IF NOT EXISTS graph_versions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                graph_slug TEXT NOT NULL,
                version_name TEXT,
                graph_json TEXT NOT NULL,
                created_at TEXT NOT NULL,
                FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE
            )
        )");

        exec("CREATE INDEX IF NOT EXISTS idx_versions_graph ON graph_versions(graph_slug)");
        exec("CREATE INDEX IF NOT EXISTS idx_versions_created ON graph_versions(graph_slug, created_at DESC)");

        // Execution persistence tables
        exec(R"(
            CREATE TABLE IF NOT EXISTS graph_executions (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                graph_slug TEXT NOT NULL,
                version_id INTEGER,
                session_id TEXT UNIQUE NOT NULL,
                created_at TEXT NOT NULL,
                duration_ms INTEGER,
                node_count INTEGER DEFAULT 0,
                FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE,
                FOREIGN KEY (version_id) REFERENCES graph_versions(id) ON DELETE SET NULL
            )
        )");

        exec(R"(
            CREATE TABLE IF NOT EXISTS execution_dataframes (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                execution_id INTEGER NOT NULL,
                node_id TEXT NOT NULL,
                port_name TEXT NOT NULL,
                row_count INTEGER,
                columns_json TEXT,
                schema_json TEXT,
                data_json TEXT,
                output_name TEXT,
                output_type TEXT,
                metadata_json TEXT,
                FOREIGN KEY (execution_id) REFERENCES graph_executions(id) ON DELETE CASCADE,
                UNIQUE(execution_id, node_id, port_name)
            )
        )");

        // Add output_name column if it doesn't exist (migration for existing DBs)
        try {
            exec("ALTER TABLE execution_dataframes ADD COLUMN output_name TEXT");
        } catch (...) {
            // Ignore error if column already exists
        }

        // Add output_type column if it doesn't exist (migration for existing DBs)
        try {
            exec("ALTER TABLE execution_dataframes ADD COLUMN output_type TEXT");
        } catch (...) {
            // Ignore error if column already exists
        }

        // Add metadata_json column if it doesn't exist (migration for existing DBs)
        try {
            exec("ALTER TABLE execution_dataframes ADD COLUMN metadata_json TEXT");
        } catch (...) {
            // Ignore error if column already exists
        }

        exec("CREATE INDEX IF NOT EXISTS idx_exec_graph ON graph_executions(graph_slug)");
        exec("CREATE INDEX IF NOT EXISTS idx_exec_session ON graph_executions(session_id)");
        exec("CREATE INDEX IF NOT EXISTS idx_exec_df ON execution_dataframes(execution_id)");

        // Test scenarios
        exec(R"(
            CREATE TABLE IF NOT EXISTS test_scenarios (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                graph_slug TEXT NOT NULL,
                name TEXT NOT NULL,
                description TEXT DEFAULT '',
                last_run_at TEXT,
                last_run_status TEXT,
                created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE
            )
        )");
        exec("CREATE INDEX IF NOT EXISTS idx_scenarios_graph ON test_scenarios(graph_slug)");

        exec(R"(
            CREATE TABLE IF NOT EXISTS test_scenario_inputs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                scenario_id INTEGER NOT NULL,
                identifier TEXT NOT NULL,
                value_json TEXT NOT NULL,
                FOREIGN KEY (scenario_id) REFERENCES test_scenarios(id) ON DELETE CASCADE,
                UNIQUE(scenario_id, identifier)
            )
        )");

        exec(R"(
            CREATE TABLE IF NOT EXISTS test_scenario_expected_outputs (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                scenario_id INTEGER NOT NULL,
                output_name TEXT NOT NULL,
                expected_json TEXT NOT NULL,
                FOREIGN KEY (scenario_id) REFERENCES test_scenarios(id) ON DELETE CASCADE,
                UNIQUE(scenario_id, output_name)
            )
        )");

        exec(R"(
            CREATE TABLE IF NOT EXISTS test_scenario_triggers (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                scenario_id INTEGER NOT NULL,
                trigger_type TEXT NOT NULL,
                identifier TEXT NOT NULL,
                data_json TEXT NOT NULL,
                FOREIGN KEY (scenario_id) REFERENCES test_scenarios(id) ON DELETE CASCADE,
                UNIQUE(scenario_id, trigger_type)
            )
        )");

        // Parameter overrides (viewer-only values, separate from graph)
        exec(R"(
            CREATE TABLE IF NOT EXISTS parameter_overrides (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                graph_slug TEXT NOT NULL,
                identifier TEXT NOT NULL,
                value_json TEXT NOT NULL,
                updated_at TEXT NOT NULL,
                FOREIGN KEY (graph_slug) REFERENCES graphs(slug) ON DELETE CASCADE,
                UNIQUE(graph_slug, identifier)
            )
        )");
        exec("CREATE INDEX IF NOT EXISTS idx_param_overrides_slug ON parameter_overrides(graph_slug)");

        // Graph links (auto-detected event navigation)
        exec(R"(
            CREATE TABLE IF NOT EXISTS graph_links (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                source_slug TEXT NOT NULL,
                target_slug TEXT NOT NULL,
                FOREIGN KEY (source_slug) REFERENCES graphs(slug) ON DELETE CASCADE,
                UNIQUE(source_slug, target_slug)
            )
        )");
        exec("CREATE INDEX IF NOT EXISTS idx_graph_links_source ON graph_links(source_slug)");
        exec("CREATE INDEX IF NOT EXISTS idx_graph_links_target ON graph_links(target_slug)");
    }

    // === Graph CRUD ===

    void createGraph(const GraphMetadata& metadata) {
        Statement stmt(m_db,
            "INSERT INTO graphs (slug, name, description, author, tags, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?, ?, ?)");

        std::string now = currentTimestamp();

        stmt.bindText(1, metadata.slug);
        stmt.bindText(2, metadata.name);
        stmt.bindText(3, metadata.description);
        stmt.bindText(4, metadata.author);
        stmt.bindText(5, tagsToJson(metadata.tags));
        stmt.bindText(6, now);
        stmt.bindText(7, now);

        stmt.step();
    }

    void updateGraph(const GraphMetadata& metadata) {
        Statement stmt(m_db,
            "UPDATE graphs SET name = ?, description = ?, author = ?, tags = ?, updated_at = ? "
            "WHERE slug = ?");

        stmt.bindText(1, metadata.name);
        stmt.bindText(2, metadata.description);
        stmt.bindText(3, metadata.author);
        stmt.bindText(4, tagsToJson(metadata.tags));
        stmt.bindText(5, currentTimestamp());
        stmt.bindText(6, metadata.slug);

        stmt.step();

        if (sqlite3_changes(m_db) == 0) {
            throw std::runtime_error("Graph not found: " + metadata.slug);
        }
    }

    void deleteGraph(const std::string& slug) {
        Statement stmt(m_db, "DELETE FROM graphs WHERE slug = ?");
        stmt.bindText(1, slug);
        stmt.step();
    }

    std::optional<GraphMetadata> getGraph(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT slug, name, description, author, tags, created_at, updated_at "
            "FROM graphs WHERE slug = ?");

        stmt.bindText(1, slug);

        if (!stmt.step()) {
            return std::nullopt;
        }

        return GraphMetadata{
            .slug = stmt.getText(0),
            .name = stmt.getText(1),
            .description = stmt.getText(2),
            .author = stmt.getText(3),
            .tags = jsonToTags(stmt.getText(4)),
            .createdAt = stmt.getText(5),
            .updatedAt = stmt.getText(6)
        };
    }

    std::vector<GraphMetadata> listGraphs() {
        Statement stmt(m_db,
            "SELECT slug, name, description, author, tags, created_at, updated_at "
            "FROM graphs ORDER BY updated_at DESC");

        std::vector<GraphMetadata> result;
        while (stmt.step()) {
            result.push_back({
                .slug = stmt.getText(0),
                .name = stmt.getText(1),
                .description = stmt.getText(2),
                .author = stmt.getText(3),
                .tags = jsonToTags(stmt.getText(4)),
                .createdAt = stmt.getText(5),
                .updatedAt = stmt.getText(6)
            });
        }
        return result;
    }

    bool graphExists(const std::string& slug) {
        Statement stmt(m_db, "SELECT 1 FROM graphs WHERE slug = ?");
        stmt.bindText(1, slug);
        return stmt.step();
    }

    // === Version Management ===

    int64_t saveVersion(const std::string& slug,
                        const nodes::NodeGraph& graph,
                        const std::optional<std::string>& versionName) {
        if (!graphExists(slug)) {
            throw std::runtime_error("Graph not found: " + slug);
        }

        std::string graphJson = nodes::NodeGraphSerializer::toString(graph, -1);

        Statement stmt(m_db,
            "INSERT INTO graph_versions (graph_slug, version_name, graph_json, created_at) "
            "VALUES (?, ?, ?, ?)");

        stmt.bindText(1, slug);
        if (versionName) {
            stmt.bindText(2, *versionName);
        } else {
            stmt.bindNull(2);
        }
        stmt.bindText(3, graphJson);
        stmt.bindText(4, currentTimestamp());

        stmt.step();

        // Update graph's updated_at
        Statement updateStmt(m_db, "UPDATE graphs SET updated_at = ? WHERE slug = ?");
        updateStmt.bindText(1, currentTimestamp());
        updateStmt.bindText(2, slug);
        updateStmt.step();

        return sqlite3_last_insert_rowid(m_db);
    }

    std::optional<GraphVersion> getVersion(int64_t versionId) {
        Statement stmt(m_db,
            "SELECT id, graph_slug, version_name, graph_json, created_at "
            "FROM graph_versions WHERE id = ?");

        stmt.bindInt64(1, versionId);

        if (!stmt.step()) {
            return std::nullopt;
        }

        return GraphVersion{
            .id = stmt.getInt64(0),
            .graphSlug = stmt.getText(1),
            .versionName = stmt.isNull(2) ? std::nullopt : std::optional<std::string>(stmt.getText(2)),
            .graphJson = stmt.getText(3),
            .createdAt = stmt.getText(4)
        };
    }

    std::optional<GraphVersion> getLatestVersion(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT id, graph_slug, version_name, graph_json, created_at "
            "FROM graph_versions WHERE graph_slug = ? "
            "ORDER BY created_at DESC LIMIT 1");

        stmt.bindText(1, slug);

        if (!stmt.step()) {
            return std::nullopt;
        }

        return GraphVersion{
            .id = stmt.getInt64(0),
            .graphSlug = stmt.getText(1),
            .versionName = stmt.isNull(2) ? std::nullopt : std::optional<std::string>(stmt.getText(2)),
            .graphJson = stmt.getText(3),
            .createdAt = stmt.getText(4)
        };
    }

    std::vector<GraphVersion> listVersions(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT id, graph_slug, version_name, graph_json, created_at "
            "FROM graph_versions WHERE graph_slug = ? "
            "ORDER BY created_at DESC");

        stmt.bindText(1, slug);

        std::vector<GraphVersion> result;
        while (stmt.step()) {
            result.push_back({
                .id = stmt.getInt64(0),
                .graphSlug = stmt.getText(1),
                .versionName = stmt.isNull(2) ? std::nullopt : std::optional<std::string>(stmt.getText(2)),
                .graphJson = stmt.getText(3),
                .createdAt = stmt.getText(4)
            });
        }
        return result;
    }

    void deleteVersion(int64_t versionId) {
        Statement stmt(m_db, "DELETE FROM graph_versions WHERE id = ?");
        stmt.bindInt64(1, versionId);
        stmt.step();
    }

    nodes::NodeGraph loadGraph(const std::string& slug) {
        auto version = getLatestVersion(slug);
        if (!version) {
            throw std::runtime_error("No version found for graph: " + slug);
        }
        return nodes::NodeGraphSerializer::fromString(version->graphJson);
    }

    nodes::NodeGraph loadVersion(int64_t versionId) {
        auto version = getVersion(versionId);
        if (!version) {
            throw std::runtime_error("Version not found: " + std::to_string(versionId));
        }
        return nodes::NodeGraphSerializer::fromString(version->graphJson);
    }

    const std::string& getDbPath() const { return m_dbPath; }

    // === Execution Persistence ===

    int64_t saveExecution(const std::string& slug,
                          const std::string& sessionId,
                          std::optional<int64_t> versionId,
                          int durationMs,
                          int nodeCount) {
        Statement stmt(m_db,
            "INSERT INTO graph_executions (graph_slug, version_id, session_id, created_at, duration_ms, node_count) "
            "VALUES (?, ?, ?, ?, ?, ?)");

        stmt.bindText(1, slug);
        if (versionId) {
            stmt.bindInt64(2, *versionId);
        } else {
            stmt.bindNull(2);
        }
        stmt.bindText(3, sessionId);
        stmt.bindText(4, currentTimestamp());
        stmt.bindInt64(5, durationMs);
        stmt.bindInt64(6, nodeCount);

        stmt.step();
        return sqlite3_last_insert_rowid(m_db);
    }

    void saveExecutionDataFrame(int64_t executionId,
                                const std::string& nodeId,
                                const std::string& portName,
                                const dataframe::DataFramePtr& df,
                                const std::string& outputName = "",
                                const std::string& outputType = "",
                                const std::string& metadataJson = "") {
        if (!df) return;

        // Serialize DataFrame with schema
        auto columnGetter = [&df](const std::string& name) { return df->getColumn(name); };
        json serialized = dataframe::DataFrameSerializer::toJsonWithSchema(
            df->rowCount(),
            df->getColumnNames(),
            columnGetter
        );

        Statement stmt(m_db,
            "INSERT OR REPLACE INTO execution_dataframes "
            "(execution_id, node_id, port_name, row_count, columns_json, schema_json, data_json, output_name, output_type, metadata_json) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

        stmt.bindInt64(1, executionId);
        stmt.bindText(2, nodeId);
        stmt.bindText(3, portName);
        stmt.bindInt64(4, static_cast<int64_t>(df->rowCount()));
        stmt.bindText(5, serialized["columns"].dump());
        stmt.bindText(6, serialized["schema"].dump());
        stmt.bindText(7, serialized["data"].dump());
        if (outputName.empty()) {
            stmt.bindNull(8);
        } else {
            stmt.bindText(8, outputName);
        }
        if (outputType.empty()) {
            stmt.bindNull(9);
        } else {
            stmt.bindText(9, outputType);
        }
        if (metadataJson.empty()) {
            stmt.bindNull(10);
        } else {
            stmt.bindText(10, metadataJson);
        }

        stmt.step();
    }

    dataframe::DataFramePtr loadExecutionDataFrame(int64_t executionId,
                                                    const std::string& nodeId,
                                                    const std::string& portName) {
        Statement stmt(m_db,
            "SELECT columns_json, schema_json, data_json FROM execution_dataframes "
            "WHERE execution_id = ? AND node_id = ? AND port_name = ?");

        stmt.bindInt64(1, executionId);
        stmt.bindText(2, nodeId);
        stmt.bindText(3, portName);

        if (!stmt.step()) {
            return nullptr;
        }

        // Reconstruct JSON and deserialize
        json j;
        j["columns"] = json::parse(stmt.getText(0));
        j["schema"] = json::parse(stmt.getText(1));
        j["data"] = json::parse(stmt.getText(2));

        return dataframe::DataFrameSerializer::fromJson(j);
    }

    std::map<std::string, std::map<std::string, dataframe::DataFramePtr>>
        loadExecutionDataFrames(int64_t executionId) {
        std::map<std::string, std::map<std::string, dataframe::DataFramePtr>> result;

        Statement stmt(m_db,
            "SELECT node_id, port_name, columns_json, schema_json, data_json "
            "FROM execution_dataframes WHERE execution_id = ?");

        stmt.bindInt64(1, executionId);

        while (stmt.step()) {
            std::string nodeId = stmt.getText(0);
            std::string portName = stmt.getText(1);

            json j;
            j["columns"] = json::parse(stmt.getText(2));
            j["schema"] = json::parse(stmt.getText(3));
            j["data"] = json::parse(stmt.getText(4));

            result[nodeId][portName] = dataframe::DataFrameSerializer::fromJson(j);
        }

        return result;
    }

    std::vector<ExecutionMetadata> listExecutions(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT e.id, e.graph_slug, e.version_id, e.session_id, e.created_at, e.duration_ms, e.node_count, "
            "       (SELECT COUNT(*) FROM execution_dataframes WHERE execution_id = e.id) as df_count "
            "FROM graph_executions e "
            "WHERE e.graph_slug = ? "
            "ORDER BY e.created_at DESC");

        stmt.bindText(1, slug);

        std::vector<ExecutionMetadata> result;
        while (stmt.step()) {
            result.push_back({
                .id = stmt.getInt64(0),
                .graphSlug = stmt.getText(1),
                .versionId = stmt.isNull(2) ? std::nullopt : std::optional<int64_t>(stmt.getInt64(2)),
                .sessionId = stmt.getText(3),
                .createdAt = stmt.getText(4),
                .durationMs = static_cast<int>(stmt.getInt64(5)),
                .nodeCount = static_cast<int>(stmt.getInt64(6)),
                .dataframeCount = static_cast<int>(stmt.getInt64(7))
            });
        }
        return result;
    }

    std::optional<ExecutionMetadata> getExecutionBySessionId(const std::string& sessionId) {
        Statement stmt(m_db,
            "SELECT e.id, e.graph_slug, e.version_id, e.session_id, e.created_at, e.duration_ms, e.node_count, "
            "       (SELECT COUNT(*) FROM execution_dataframes WHERE execution_id = e.id) as df_count "
            "FROM graph_executions e "
            "WHERE e.session_id = ?");

        stmt.bindText(1, sessionId);

        if (!stmt.step()) {
            return std::nullopt;
        }

        return ExecutionMetadata{
            .id = stmt.getInt64(0),
            .graphSlug = stmt.getText(1),
            .versionId = stmt.isNull(2) ? std::nullopt : std::optional<int64_t>(stmt.getInt64(2)),
            .sessionId = stmt.getText(3),
            .createdAt = stmt.getText(4),
            .durationMs = static_cast<int>(stmt.getInt64(5)),
            .nodeCount = static_cast<int>(stmt.getInt64(6)),
            .dataframeCount = static_cast<int>(stmt.getInt64(7))
        };
    }

    std::optional<ExecutionMetadata> getExecution(int64_t executionId) {
        Statement stmt(m_db,
            "SELECT e.id, e.graph_slug, e.version_id, e.session_id, e.created_at, e.duration_ms, e.node_count, "
            "       (SELECT COUNT(*) FROM execution_dataframes WHERE execution_id = e.id) as df_count "
            "FROM graph_executions e "
            "WHERE e.id = ?");

        stmt.bindInt64(1, executionId);

        if (!stmt.step()) {
            return std::nullopt;
        }

        return ExecutionMetadata{
            .id = stmt.getInt64(0),
            .graphSlug = stmt.getText(1),
            .versionId = stmt.isNull(2) ? std::nullopt : std::optional<int64_t>(stmt.getInt64(2)),
            .sessionId = stmt.getText(3),
            .createdAt = stmt.getText(4),
            .durationMs = static_cast<int>(stmt.getInt64(5)),
            .nodeCount = static_cast<int>(stmt.getInt64(6)),
            .dataframeCount = static_cast<int>(stmt.getInt64(7))
        };
    }

    void cleanupOldExecutions(const std::string& slug, size_t keepCount) {
        // Get IDs of executions to delete (all except the N most recent)
        Statement selectStmt(m_db,
            "SELECT id FROM graph_executions "
            "WHERE graph_slug = ? "
            "ORDER BY created_at DESC "
            "LIMIT -1 OFFSET ?");

        selectStmt.bindText(1, slug);
        selectStmt.bindInt64(2, static_cast<int64_t>(keepCount));

        std::vector<int64_t> toDelete;
        while (selectStmt.step()) {
            toDelete.push_back(selectStmt.getInt64(0));
        }

        // Delete them (cascade will delete dataframes too)
        for (int64_t id : toDelete) {
            Statement delStmt(m_db, "DELETE FROM graph_executions WHERE id = ?");
            delStmt.bindInt64(1, id);
            delStmt.step();
        }
    }

    std::map<std::string, std::map<std::string, DataFrameMetadata>>
        getExecutionCsvMetadata(int64_t executionId) {
        std::map<std::string, std::map<std::string, DataFrameMetadata>> result;

        Statement stmt(m_db,
            "SELECT id, node_id, port_name, row_count, columns_json, schema_json "
            "FROM execution_dataframes WHERE execution_id = ?");

        stmt.bindInt64(1, executionId);

        while (stmt.step()) {
            DataFrameMetadata meta;
            meta.id = stmt.getInt64(0);
            meta.executionId = executionId;
            meta.nodeId = stmt.getText(1);
            meta.portName = stmt.getText(2);
            meta.rowCount = static_cast<size_t>(stmt.getInt64(3));

            // Parse schema
            json schemaJson = json::parse(stmt.getText(5));
            for (const auto& col : schemaJson) {
                meta.schema.push_back({
                    .name = col["name"].get<std::string>(),
                    .type = col["type"].get<std::string>()
                });
            }

            result[meta.nodeId][meta.portName] = meta;
        }

        return result;
    }

    // Get named outputs for the latest execution of a graph
    std::vector<NamedOutputInfo> getNamedOutputs(const std::string& slug) {
        std::vector<NamedOutputInfo> result;

        // Get the latest execution for this graph
        Statement execStmt(m_db,
            "SELECT id, created_at FROM graph_executions "
            "WHERE graph_slug = ? ORDER BY created_at DESC LIMIT 1");
        execStmt.bindText(1, slug);

        if (!execStmt.step()) {
            return result; // No executions found
        }

        int64_t executionId = execStmt.getInt64(0);
        std::string executionCreatedAt = execStmt.getText(1);

        // Get all named outputs from this execution
        Statement stmt(m_db,
            "SELECT node_id, output_name, row_count, columns_json, output_type, metadata_json "
            "FROM execution_dataframes "
            "WHERE execution_id = ? AND output_name IS NOT NULL AND output_name != ''");
        stmt.bindInt64(1, executionId);

        while (stmt.step()) {
            NamedOutputInfo info;
            info.nodeId = stmt.getText(0);
            info.name = stmt.getText(1);
            info.rowCount = static_cast<size_t>(stmt.getInt64(2));
            info.executionId = executionId;
            info.createdAt = executionCreatedAt;

            // Parse columns
            json colsJson = json::parse(stmt.getText(3));
            for (const auto& col : colsJson) {
                info.columns.push_back(col.get<std::string>());
            }

            info.outputType = stmt.getText(4);
            info.metadataJson = stmt.getText(5);

            result.push_back(info);
        }

        return result;
    }

    // Load a named output DataFrame
    dataframe::DataFramePtr loadNamedOutput(const std::string& slug, const std::string& name) {
        // Get the latest execution for this graph
        Statement execStmt(m_db,
            "SELECT id FROM graph_executions "
            "WHERE graph_slug = ? ORDER BY created_at DESC LIMIT 1");
        execStmt.bindText(1, slug);

        if (!execStmt.step()) {
            return nullptr;
        }

        int64_t executionId = execStmt.getInt64(0);

        // Get the named output
        Statement stmt(m_db,
            "SELECT columns_json, schema_json, data_json FROM execution_dataframes "
            "WHERE execution_id = ? AND output_name = ?");
        stmt.bindInt64(1, executionId);
        stmt.bindText(2, name);

        if (!stmt.step()) {
            return nullptr;
        }

        // Reconstruct JSON and deserialize
        json j;
        j["columns"] = json::parse(stmt.getText(0));
        j["schema"] = json::parse(stmt.getText(1));
        j["data"] = json::parse(stmt.getText(2));

        return dataframe::DataFrameSerializer::fromJson(j);
    }

    // Get metadata for a named output
    std::optional<NamedOutputInfo> getNamedOutputInfo(const std::string& slug, const std::string& name) {
        // Get the latest execution for this graph
        Statement execStmt(m_db,
            "SELECT id, created_at FROM graph_executions "
            "WHERE graph_slug = ? ORDER BY created_at DESC LIMIT 1");
        execStmt.bindText(1, slug);

        if (!execStmt.step()) {
            return std::nullopt;
        }

        int64_t executionId = execStmt.getInt64(0);
        std::string executionCreatedAt = execStmt.getText(1);

        Statement stmt(m_db,
            "SELECT node_id, row_count, columns_json, output_type, metadata_json "
            "FROM execution_dataframes "
            "WHERE execution_id = ? AND output_name = ?");
        stmt.bindInt64(1, executionId);
        stmt.bindText(2, name);

        if (!stmt.step()) {
            return std::nullopt;
        }

        NamedOutputInfo info;
        info.nodeId = stmt.getText(0);
        info.name = name;
        info.rowCount = static_cast<size_t>(stmt.getInt64(1));
        info.executionId = executionId;
        info.createdAt = executionCreatedAt;

        json colsJson = json::parse(stmt.getText(2));
        for (const auto& col : colsJson) {
            info.columns.push_back(col.get<std::string>());
        }

        info.outputType = stmt.getText(3);
        info.metadataJson = stmt.getText(4);

        return info;
    }

    // === Test Scenarios ===

    int64_t createScenario(const std::string& graphSlug, const std::string& name, const std::string& description) {
        std::string now = currentTimestamp();
        Statement stmt(m_db,
            "INSERT INTO test_scenarios (graph_slug, name, description, created_at, updated_at) "
            "VALUES (?, ?, ?, ?, ?)");
        stmt.bindText(1, graphSlug);
        stmt.bindText(2, name);
        stmt.bindText(3, description);
        stmt.bindText(4, now);
        stmt.bindText(5, now);
        stmt.step();
        return sqlite3_last_insert_rowid(m_db);
    }

    std::optional<ScenarioInfo> getScenario(int64_t scenarioId) {
        Statement stmt(m_db,
            "SELECT id, graph_slug, name, description, last_run_at, last_run_status, created_at, updated_at "
            "FROM test_scenarios WHERE id = ?");
        stmt.bindInt64(1, scenarioId);
        if (!stmt.step()) return std::nullopt;
        ScenarioInfo info;
        info.id = stmt.getInt64(0);
        info.graphSlug = stmt.getText(1);
        info.name = stmt.getText(2);
        info.description = stmt.getText(3);
        info.lastRunAt = stmt.getText(4);
        info.lastRunStatus = stmt.getText(5);
        info.createdAt = stmt.getText(6);
        info.updatedAt = stmt.getText(7);
        return info;
    }

    std::vector<ScenarioInfo> listScenarios(const std::string& graphSlug) {
        Statement stmt(m_db,
            "SELECT id, graph_slug, name, description, last_run_at, last_run_status, created_at, updated_at "
            "FROM test_scenarios WHERE graph_slug = ? ORDER BY created_at ASC");
        stmt.bindText(1, graphSlug);
        std::vector<ScenarioInfo> result;
        while (stmt.step()) {
            ScenarioInfo info;
            info.id = stmt.getInt64(0);
            info.graphSlug = stmt.getText(1);
            info.name = stmt.getText(2);
            info.description = stmt.getText(3);
            info.lastRunAt = stmt.getText(4);
            info.lastRunStatus = stmt.getText(5);
            info.createdAt = stmt.getText(6);
            info.updatedAt = stmt.getText(7);
            result.push_back(std::move(info));
        }
        return result;
    }

    void updateScenario(int64_t scenarioId, const std::string& name, const std::string& description) {
        Statement stmt(m_db,
            "UPDATE test_scenarios SET name = ?, description = ?, updated_at = ? WHERE id = ?");
        stmt.bindText(1, name);
        stmt.bindText(2, description);
        stmt.bindText(3, currentTimestamp());
        stmt.bindInt64(4, scenarioId);
        stmt.step();
    }

    void updateScenarioRunStatus(int64_t scenarioId, const std::string& status) {
        Statement stmt(m_db,
            "UPDATE test_scenarios SET last_run_at = ?, last_run_status = ?, updated_at = ? WHERE id = ?");
        std::string now = currentTimestamp();
        stmt.bindText(1, now);
        stmt.bindText(2, status);
        stmt.bindText(3, now);
        stmt.bindInt64(4, scenarioId);
        stmt.step();
    }

    void deleteScenario(int64_t scenarioId) {
        Statement stmt(m_db, "DELETE FROM test_scenarios WHERE id = ?");
        stmt.bindInt64(1, scenarioId);
        stmt.step();
    }

    void setScenarioInputs(int64_t scenarioId, const std::vector<ScenarioInput>& inputs) {
        exec("BEGIN");
        {
            Statement del(m_db, "DELETE FROM test_scenario_inputs WHERE scenario_id = ?");
            del.bindInt64(1, scenarioId);
            del.step();
        }
        for (const auto& input : inputs) {
            Statement ins(m_db,
                "INSERT INTO test_scenario_inputs (scenario_id, identifier, value_json) VALUES (?, ?, ?)");
            ins.bindInt64(1, scenarioId);
            ins.bindText(2, input.identifier);
            ins.bindText(3, input.valueJson);
            ins.step();
        }
        exec("COMMIT");
    }

    std::vector<ScenarioInput> getScenarioInputs(int64_t scenarioId) {
        Statement stmt(m_db,
            "SELECT id, scenario_id, identifier, value_json FROM test_scenario_inputs WHERE scenario_id = ?");
        stmt.bindInt64(1, scenarioId);
        std::vector<ScenarioInput> result;
        while (stmt.step()) {
            ScenarioInput si;
            si.id = stmt.getInt64(0);
            si.scenarioId = stmt.getInt64(1);
            si.identifier = stmt.getText(2);
            si.valueJson = stmt.getText(3);
            result.push_back(std::move(si));
        }
        return result;
    }

    void setScenarioExpectedOutputs(int64_t scenarioId, const std::vector<ScenarioExpectedOutput>& outputs) {
        exec("BEGIN");
        {
            Statement del(m_db, "DELETE FROM test_scenario_expected_outputs WHERE scenario_id = ?");
            del.bindInt64(1, scenarioId);
            del.step();
        }
        for (const auto& out : outputs) {
            Statement ins(m_db,
                "INSERT INTO test_scenario_expected_outputs (scenario_id, output_name, expected_json) VALUES (?, ?, ?)");
            ins.bindInt64(1, scenarioId);
            ins.bindText(2, out.outputName);
            ins.bindText(3, out.expectedJson);
            ins.step();
        }
        exec("COMMIT");
    }

    std::vector<ScenarioExpectedOutput> getScenarioExpectedOutputs(int64_t scenarioId) {
        Statement stmt(m_db,
            "SELECT id, scenario_id, output_name, expected_json FROM test_scenario_expected_outputs WHERE scenario_id = ?");
        stmt.bindInt64(1, scenarioId);
        std::vector<ScenarioExpectedOutput> result;
        while (stmt.step()) {
            ScenarioExpectedOutput seo;
            seo.id = stmt.getInt64(0);
            seo.scenarioId = stmt.getInt64(1);
            seo.outputName = stmt.getText(2);
            seo.expectedJson = stmt.getText(3);
            result.push_back(std::move(seo));
        }
        return result;
    }

    void setScenarioTriggers(int64_t scenarioId, const std::vector<ScenarioTrigger>& triggers) {
        exec("BEGIN");
        {
            Statement del(m_db, "DELETE FROM test_scenario_triggers WHERE scenario_id = ?");
            del.bindInt64(1, scenarioId);
            del.step();
        }
        for (const auto& trig : triggers) {
            Statement ins(m_db,
                "INSERT INTO test_scenario_triggers (scenario_id, trigger_type, identifier, data_json) VALUES (?, ?, ?, ?)");
            ins.bindInt64(1, scenarioId);
            ins.bindText(2, trig.triggerType);
            ins.bindText(3, trig.identifier);
            ins.bindText(4, trig.dataJson);
            ins.step();
        }
        exec("COMMIT");
    }

    std::vector<ScenarioTrigger> getScenarioTriggers(int64_t scenarioId) {
        Statement stmt(m_db,
            "SELECT id, scenario_id, trigger_type, identifier, data_json FROM test_scenario_triggers WHERE scenario_id = ?");
        stmt.bindInt64(1, scenarioId);
        std::vector<ScenarioTrigger> result;
        while (stmt.step()) {
            ScenarioTrigger st;
            st.id = stmt.getInt64(0);
            st.scenarioId = stmt.getInt64(1);
            st.triggerType = stmt.getText(2);
            st.identifier = stmt.getText(3);
            st.dataJson = stmt.getText(4);
            result.push_back(std::move(st));
        }
        return result;
    }

    ScenarioDetails getScenarioDetails(int64_t scenarioId) {
        ScenarioDetails details;
        auto info = getScenario(scenarioId);
        if (!info) throw std::runtime_error("Scenario not found: " + std::to_string(scenarioId));
        details.info = *info;
        details.inputs = getScenarioInputs(scenarioId);
        details.expectedOutputs = getScenarioExpectedOutputs(scenarioId);
        details.triggers = getScenarioTriggers(scenarioId);
        return details;
    }

    // === Parameter Overrides ===

    std::map<std::string, std::string> getParameterOverrides(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT identifier, value_json FROM parameter_overrides WHERE graph_slug = ?");
        stmt.bindText(1, slug);

        std::map<std::string, std::string> result;
        while (stmt.step()) {
            result[stmt.getText(0)] = stmt.getText(1);
        }
        return result;
    }

    void setParameterOverride(const std::string& slug, const std::string& identifier, const std::string& valueJson) {
        Statement stmt(m_db,
            "INSERT OR REPLACE INTO parameter_overrides (graph_slug, identifier, value_json, updated_at) "
            "VALUES (?, ?, ?, ?)");
        stmt.bindText(1, slug);
        stmt.bindText(2, identifier);
        stmt.bindText(3, valueJson);
        stmt.bindText(4, currentTimestamp());
        stmt.step();
    }

    void deleteParameterOverride(const std::string& slug, const std::string& identifier) {
        Statement stmt(m_db,
            "DELETE FROM parameter_overrides WHERE graph_slug = ? AND identifier = ?");
        stmt.bindText(1, slug);
        stmt.bindText(2, identifier);
        stmt.step();
    }

    void clearParameterOverrides(const std::string& slug) {
        Statement stmt(m_db,
            "DELETE FROM parameter_overrides WHERE graph_slug = ?");
        stmt.bindText(1, slug);
        stmt.step();
    }

    // === Graph Links ===

    void replaceGraphLinks(const std::string& sourceSlug, const std::vector<std::string>& targetSlugs) {
        exec("BEGIN");
        {
            Statement del(m_db, "DELETE FROM graph_links WHERE source_slug = ?");
            del.bindText(1, sourceSlug);
            del.step();
        }
        for (const auto& target : targetSlugs) {
            Statement ins(m_db,
                "INSERT OR IGNORE INTO graph_links (source_slug, target_slug) VALUES (?, ?)");
            ins.bindText(1, sourceSlug);
            ins.bindText(2, target);
            ins.step();
        }
        exec("COMMIT");
    }

    std::vector<std::string> getOutgoingLinks(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT target_slug FROM graph_links WHERE source_slug = ? ORDER BY target_slug");
        stmt.bindText(1, slug);
        std::vector<std::string> result;
        while (stmt.step()) {
            result.push_back(stmt.getText(0));
        }
        return result;
    }

    std::vector<std::string> getIncomingLinks(const std::string& slug) {
        Statement stmt(m_db,
            "SELECT source_slug FROM graph_links WHERE target_slug = ? ORDER BY source_slug");
        stmt.bindText(1, slug);
        std::vector<std::string> result;
        while (stmt.step()) {
            result.push_back(stmt.getText(0));
        }
        return result;
    }

private:
    std::string m_dbPath;
    sqlite3* m_db;
};

// =============================================================================
// GraphStorage (pImpl forwarding)
// =============================================================================

GraphStorage::GraphStorage(const std::string& dbPath)
    : m_impl(std::make_unique<Impl>(dbPath)) {}

GraphStorage::~GraphStorage() = default;

GraphStorage::GraphStorage(GraphStorage&&) noexcept = default;
GraphStorage& GraphStorage::operator=(GraphStorage&&) noexcept = default;

void GraphStorage::createGraph(const GraphMetadata& metadata) {
    m_impl->createGraph(metadata);
}

void GraphStorage::updateGraph(const GraphMetadata& metadata) {
    m_impl->updateGraph(metadata);
}

void GraphStorage::deleteGraph(const std::string& slug) {
    m_impl->deleteGraph(slug);
}

std::optional<GraphMetadata> GraphStorage::getGraph(const std::string& slug) {
    return m_impl->getGraph(slug);
}

std::vector<GraphMetadata> GraphStorage::listGraphs() {
    return m_impl->listGraphs();
}

bool GraphStorage::graphExists(const std::string& slug) {
    return m_impl->graphExists(slug);
}

int64_t GraphStorage::saveVersion(const std::string& slug,
                                   const nodes::NodeGraph& graph,
                                   const std::optional<std::string>& versionName) {
    return m_impl->saveVersion(slug, graph, versionName);
}

std::optional<GraphVersion> GraphStorage::getVersion(int64_t versionId) {
    return m_impl->getVersion(versionId);
}

std::optional<GraphVersion> GraphStorage::getLatestVersion(const std::string& slug) {
    return m_impl->getLatestVersion(slug);
}

std::vector<GraphVersion> GraphStorage::listVersions(const std::string& slug) {
    return m_impl->listVersions(slug);
}

void GraphStorage::deleteVersion(int64_t versionId) {
    m_impl->deleteVersion(versionId);
}

nodes::NodeGraph GraphStorage::loadGraph(const std::string& slug) {
    return m_impl->loadGraph(slug);
}

nodes::NodeGraph GraphStorage::loadVersion(int64_t versionId) {
    return m_impl->loadVersion(versionId);
}

const std::string& GraphStorage::getDbPath() const {
    return m_impl->getDbPath();
}

// === Execution Persistence ===

int64_t GraphStorage::saveExecution(const std::string& slug,
                                     const std::string& sessionId,
                                     std::optional<int64_t> versionId,
                                     int durationMs,
                                     int nodeCount) {
    return m_impl->saveExecution(slug, sessionId, versionId, durationMs, nodeCount);
}

void GraphStorage::saveExecutionDataFrame(int64_t executionId,
                                           const std::string& nodeId,
                                           const std::string& portName,
                                           const dataframe::DataFramePtr& df,
                                           const std::string& outputName,
                                           const std::string& outputType,
                                           const std::string& metadataJson) {
    m_impl->saveExecutionDataFrame(executionId, nodeId, portName, df, outputName, outputType, metadataJson);
}

dataframe::DataFramePtr GraphStorage::loadExecutionDataFrame(int64_t executionId,
                                                              const std::string& nodeId,
                                                              const std::string& portName) {
    return m_impl->loadExecutionDataFrame(executionId, nodeId, portName);
}

std::map<std::string, std::map<std::string, dataframe::DataFramePtr>>
    GraphStorage::loadExecutionDataFrames(int64_t executionId) {
    return m_impl->loadExecutionDataFrames(executionId);
}

std::vector<ExecutionMetadata> GraphStorage::listExecutions(const std::string& slug) {
    return m_impl->listExecutions(slug);
}

std::optional<ExecutionMetadata> GraphStorage::getExecutionBySessionId(const std::string& sessionId) {
    return m_impl->getExecutionBySessionId(sessionId);
}

std::optional<ExecutionMetadata> GraphStorage::getExecution(int64_t executionId) {
    return m_impl->getExecution(executionId);
}

void GraphStorage::cleanupOldExecutions(const std::string& slug, size_t keepCount) {
    m_impl->cleanupOldExecutions(slug, keepCount);
}

std::map<std::string, std::map<std::string, DataFrameMetadata>>
    GraphStorage::getExecutionCsvMetadata(int64_t executionId) {
    return m_impl->getExecutionCsvMetadata(executionId);
}

// === Named Outputs ===

std::vector<NamedOutputInfo> GraphStorage::getNamedOutputs(const std::string& slug) {
    return m_impl->getNamedOutputs(slug);
}

dataframe::DataFramePtr GraphStorage::loadNamedOutput(const std::string& slug, const std::string& name) {
    return m_impl->loadNamedOutput(slug, name);
}

std::optional<NamedOutputInfo> GraphStorage::getNamedOutputInfo(const std::string& slug, const std::string& name) {
    return m_impl->getNamedOutputInfo(slug, name);
}

// === Test Scenarios ===

int64_t GraphStorage::createScenario(const std::string& graphSlug, const std::string& name, const std::string& description) {
    return m_impl->createScenario(graphSlug, name, description);
}

std::optional<ScenarioInfo> GraphStorage::getScenario(int64_t scenarioId) {
    return m_impl->getScenario(scenarioId);
}

std::vector<ScenarioInfo> GraphStorage::listScenarios(const std::string& graphSlug) {
    return m_impl->listScenarios(graphSlug);
}

void GraphStorage::updateScenario(int64_t scenarioId, const std::string& name, const std::string& description) {
    m_impl->updateScenario(scenarioId, name, description);
}

void GraphStorage::updateScenarioRunStatus(int64_t scenarioId, const std::string& status) {
    m_impl->updateScenarioRunStatus(scenarioId, status);
}

void GraphStorage::deleteScenario(int64_t scenarioId) {
    m_impl->deleteScenario(scenarioId);
}

void GraphStorage::setScenarioInputs(int64_t scenarioId, const std::vector<ScenarioInput>& inputs) {
    m_impl->setScenarioInputs(scenarioId, inputs);
}

std::vector<ScenarioInput> GraphStorage::getScenarioInputs(int64_t scenarioId) {
    return m_impl->getScenarioInputs(scenarioId);
}

void GraphStorage::setScenarioExpectedOutputs(int64_t scenarioId, const std::vector<ScenarioExpectedOutput>& outputs) {
    m_impl->setScenarioExpectedOutputs(scenarioId, outputs);
}

std::vector<ScenarioExpectedOutput> GraphStorage::getScenarioExpectedOutputs(int64_t scenarioId) {
    return m_impl->getScenarioExpectedOutputs(scenarioId);
}

void GraphStorage::setScenarioTriggers(int64_t scenarioId, const std::vector<ScenarioTrigger>& triggers) {
    m_impl->setScenarioTriggers(scenarioId, triggers);
}

std::vector<ScenarioTrigger> GraphStorage::getScenarioTriggers(int64_t scenarioId) {
    return m_impl->getScenarioTriggers(scenarioId);
}

ScenarioDetails GraphStorage::getScenarioDetails(int64_t scenarioId) {
    return m_impl->getScenarioDetails(scenarioId);
}

// === Parameter Overrides ===

std::map<std::string, std::string> GraphStorage::getParameterOverrides(const std::string& slug) {
    return m_impl->getParameterOverrides(slug);
}

void GraphStorage::setParameterOverride(const std::string& slug, const std::string& identifier, const std::string& valueJson) {
    m_impl->setParameterOverride(slug, identifier, valueJson);
}

void GraphStorage::deleteParameterOverride(const std::string& slug, const std::string& identifier) {
    m_impl->deleteParameterOverride(slug, identifier);
}

void GraphStorage::clearParameterOverrides(const std::string& slug) {
    m_impl->clearParameterOverrides(slug);
}

// === Graph Links ===

void GraphStorage::replaceGraphLinks(const std::string& sourceSlug, const std::vector<std::string>& targetSlugs) {
    m_impl->replaceGraphLinks(sourceSlug, targetSlugs);
}

std::vector<std::string> GraphStorage::getOutgoingLinks(const std::string& slug) {
    return m_impl->getOutgoingLinks(slug);
}

std::vector<std::string> GraphStorage::getIncomingLinks(const std::string& slug) {
    return m_impl->getIncomingLinks(slug);
}

} // namespace storage
