#include "postgres/PostgresPool.hpp"
#include <sstream>
#include <stdexcept>
#include <iostream>

// Simple logging macros for postgres module
#define PG_LOG_INFO(msg) std::cerr << "[POSTGRES] INFO: " << msg << std::endl
#define PG_LOG_DEBUG(msg) std::cerr << "[POSTGRES] DEBUG: " << msg << std::endl
#define PG_LOG_ERROR(msg) std::cerr << "[POSTGRES] ERROR: " << msg << std::endl

namespace postgres {

PostgresPool& PostgresPool::instance() {
    static PostgresPool pool;
    return pool;
}

void PostgresPool::configure(const std::string& connectionString) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Si la string de connexion change, on ferme l'ancienne connexion
    if (m_connectionString != connectionString) {
        m_connection.reset();
    }

    m_connectionString = connectionString;
    m_configured = true;

    PG_LOG_INFO("PostgresPool configured with connection string");
}

bool PostgresPool::isConfigured() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_configured;
}

bool PostgresPool::isConnected() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connection && m_connection->is_open();
}

const std::string& PostgresPool::getConnectionString() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_connectionString;
}

void PostgresPool::ensureConnection() {
    if (!m_configured) {
        throw std::runtime_error("PostgresPool not configured. Call configure() first.");
    }

    if (!m_connection || !m_connection->is_open()) {
        PG_LOG_DEBUG("PostgresPool: Creating new connection...");
        m_connection = std::make_unique<pqxx::connection>(m_connectionString);

        if (!m_connection->is_open()) {
            throw std::runtime_error("Failed to open PostgreSQL connection");
        }

        PG_LOG_INFO("PostgresPool: Connection established");
    }
}

std::shared_ptr<dataframe::DataFrame> PostgresPool::executeQuery(const std::string& sql) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ensureConnection();

    PG_LOG_DEBUG("PostgresPool: Executing query:\n" + sql);

    try {
        pqxx::work txn(*m_connection);
        pqxx::result result = txn.exec(sql);
        txn.commit();

        PG_LOG_DEBUG("PostgresPool: Query returned " + std::to_string(result.size()) + " rows");

        return resultToDataFrame(result);
    }
    catch (const pqxx::sql_error& e) {
        PG_LOG_ERROR("PostgresPool: SQL error: " + std::string(e.what()));
        throw std::runtime_error("SQL error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        PG_LOG_ERROR("PostgresPool: Error executing query: " + std::string(e.what()));
        throw;
    }
}

size_t PostgresPool::executeCommand(const std::string& sql) {
    std::lock_guard<std::mutex> lock(m_mutex);

    ensureConnection();

    PG_LOG_DEBUG("PostgresPool: Executing command:\n" + sql);

    try {
        pqxx::work txn(*m_connection);
        pqxx::result result = txn.exec(sql);
        txn.commit();

        return result.affected_rows();
    }
    catch (const pqxx::sql_error& e) {
        PG_LOG_ERROR("PostgresPool: SQL error: " + std::string(e.what()));
        throw std::runtime_error("SQL error: " + std::string(e.what()));
    }
    catch (const std::exception& e) {
        PG_LOG_ERROR("PostgresPool: Error executing command: " + std::string(e.what()));
        throw;
    }
}

void PostgresPool::disconnect() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_connection) {
        PG_LOG_INFO("PostgresPool: Disconnecting...");
        m_connection.reset();
    }
}

void PostgresPool::reset() {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_connection.reset();
    m_connectionString.clear();
    m_configured = false;

    PG_LOG_INFO("PostgresPool: Reset complete");
}

dataframe::ColumnTypeOpt PostgresPool::oidToColumnType(pqxx::oid oid) {
    // OIDs PostgreSQL courants
    // https://www.postgresql.org/docs/current/datatype-oid.html
    switch (oid) {
        // Integer types
        case 20:   // int8
        case 21:   // int2
        case 23:   // int4
        case 26:   // oid
            return dataframe::ColumnTypeOpt::INT;

        // Float types
        case 700:  // float4
        case 701:  // float8
        case 1700: // numeric
            return dataframe::ColumnTypeOpt::DOUBLE;

        // String/text types (and everything else)
        case 25:   // text
        case 1042: // bpchar (char)
        case 1043: // varchar
        case 2950: // uuid
        default:
            return dataframe::ColumnTypeOpt::STRING;
    }
}

std::shared_ptr<dataframe::DataFrame> PostgresPool::resultToDataFrame(const pqxx::result& result) {
    auto df = std::make_shared<dataframe::DataFrame>();

    // Nombre de colonnes
    auto numCols = result.columns();

    if (result.empty()) {
        // Créer les colonnes vides avec les bons noms
        for (pqxx::row::size_type i = 0; i < numCols; ++i) {
            std::string colName = result.column_name(i);
            auto type = oidToColumnType(result.column_type(i));
            switch (type) {
                case dataframe::ColumnTypeOpt::INT:
                    df->addIntColumn(colName);
                    break;
                case dataframe::ColumnTypeOpt::DOUBLE:
                    df->addDoubleColumn(colName);
                    break;
                case dataframe::ColumnTypeOpt::STRING:
                default:
                    df->addStringColumn(colName);
                    break;
            }
        }
        return df;
    }

    // Créer les colonnes avec le bon type
    std::vector<dataframe::ColumnTypeOpt> columnTypes;
    for (pqxx::row::size_type i = 0; i < numCols; ++i) {
        std::string colName = result.column_name(i);
        auto type = oidToColumnType(result.column_type(i));
        columnTypes.push_back(type);

        switch (type) {
            case dataframe::ColumnTypeOpt::INT:
                df->addIntColumn(colName);
                break;
            case dataframe::ColumnTypeOpt::DOUBLE:
                df->addDoubleColumn(colName);
                break;
            case dataframe::ColumnTypeOpt::STRING:
            default:
                df->addStringColumn(colName);
                break;
        }
    }

    // Réserver l'espace pour les lignes
    size_t numRows = result.size();
    for (const auto& colName : df->getColumnNames()) {
        df->getColumn(colName)->reserve(numRows);
    }

    // Remplir les données
    for (const auto& row : result) {
        std::vector<std::string> values;
        values.reserve(static_cast<size_t>(row.size()));

        for (pqxx::row::size_type i = 0; i < row.size(); ++i) {
            if (row[i].is_null()) {
                // Pour les valeurs NULL, on utilise une valeur par défaut
                switch (columnTypes[static_cast<size_t>(i)]) {
                    case dataframe::ColumnTypeOpt::INT:
                        values.push_back("0");
                        break;
                    case dataframe::ColumnTypeOpt::DOUBLE:
                        values.push_back("0.0");
                        break;
                    default:
                        values.push_back("");
                        break;
                }
            } else {
                values.push_back(row[i].c_str());
            }
        }

        df->addRow(values);
    }

    return df;
}

} // namespace postgres
