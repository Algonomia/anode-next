#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <pqxx/pqxx>
#include "dataframe/DataFrame.hpp"

namespace postgres {

/**
 * @brief Singleton pool de connexions PostgreSQL
 *
 * Gère une connexion PostgreSQL et fournit des méthodes pour
 * exécuter des requêtes et les convertir en DataFrame.
 */
class PostgresPool {
public:
    /**
     * @brief Accès au singleton
     */
    static PostgresPool& instance();

    /**
     * @brief Configure la connexion PostgreSQL
     * @param connectionString Format: "host=localhost port=5432 dbname=mydb user=user password=pass"
     */
    void configure(const std::string& connectionString);

    /**
     * @brief Vérifie si le pool est configuré
     */
    bool isConfigured() const;

    /**
     * @brief Vérifie si la connexion est active
     */
    bool isConnected() const;

    /**
     * @brief Retourne la string de connexion actuelle
     */
    const std::string& getConnectionString() const;

    /**
     * @brief Exécute une requête SQL et retourne le résultat comme DataFrame
     * @param sql La requête SQL à exécuter
     * @return DataFrame contenant les résultats
     * @throws std::runtime_error si la connexion échoue ou si la requête échoue
     */
    std::shared_ptr<dataframe::DataFrame> executeQuery(const std::string& sql);

    /**
     * @brief Exécute une requête SQL sans retour de données (INSERT, UPDATE, DELETE)
     * @param sql La requête SQL à exécuter
     * @return Nombre de lignes affectées
     * @throws std::runtime_error si la connexion échoue ou si la requête échoue
     */
    size_t executeCommand(const std::string& sql);

    /**
     * @brief Ferme la connexion courante
     */
    void disconnect();

    /**
     * @brief Réinitialise le pool (pour les tests)
     */
    void reset();

private:
    PostgresPool() = default;
    ~PostgresPool() = default;

    PostgresPool(const PostgresPool&) = delete;
    PostgresPool& operator=(const PostgresPool&) = delete;

    /**
     * @brief Assure qu'une connexion est établie
     */
    void ensureConnection();

    /**
     * @brief Convertit un résultat pqxx en DataFrame
     */
    std::shared_ptr<dataframe::DataFrame> resultToDataFrame(const pqxx::result& result);

    /**
     * @brief Détermine le type de colonne à partir du OID PostgreSQL
     */
    dataframe::ColumnTypeOpt oidToColumnType(pqxx::oid oid);

    std::string m_connectionString;
    std::unique_ptr<pqxx::connection> m_connection;
    mutable std::mutex m_mutex;
    bool m_configured = false;
};

} // namespace postgres
