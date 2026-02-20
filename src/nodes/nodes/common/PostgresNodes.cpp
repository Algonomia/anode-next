#include "PostgresNodes.hpp"
#include "nodes/NodeBuilder.hpp"
#include "nodes/NodeRegistry.hpp"
#include "nodes/DynRequest.hpp"
#include "postgres/PostgresPool.hpp"
#include <sstream>

namespace nodes {

void registerPostgresNodes() {
    registerPostgresConfigNode();
    registerPostgresQueryNode();
    registerPostgresFuncNode();
}

void registerPostgresConfigNode() {
    NodeBuilder("postgres_config", "database")
        .output("connection", Type::String)
        .entryPoint()
        .onCompile([](NodeContext& ctx) {
            // Récupérer les propriétés de configuration
            auto hostProp = ctx.getInputWorkload("_host");
            auto portProp = ctx.getInputWorkload("_port");
            auto databaseProp = ctx.getInputWorkload("_database");
            auto userProp = ctx.getInputWorkload("_user");
            auto passwordProp = ctx.getInputWorkload("_password");

            // Check if any property is explicitly set
            bool hasExplicitConfig = !hostProp.isNull() || !portProp.isNull() ||
                                     !databaseProp.isNull() || !userProp.isNull() ||
                                     !passwordProp.isNull();

            // If already configured at server level and no explicit properties, just output current config
            auto& pool = postgres::PostgresPool::instance();
            if (pool.isConfigured() && !hasExplicitConfig) {
                // Return existing connection info
                ctx.setOutput("connection", std::string("(configured at server level)"));
                return;
            }

            // Valeurs par défaut
            std::string host = hostProp.isNull() ? "localhost" : hostProp.getString();
            std::string port = portProp.isNull() ? "5432" : portProp.getString();
            std::string database = databaseProp.isNull() ? "postgres" : databaseProp.getString();
            std::string user = userProp.isNull() ? "postgres" : userProp.getString();
            std::string password = passwordProp.isNull() ? "" : passwordProp.getString();

            // Construire la string de connexion
            std::ostringstream connStr;
            connStr << "host=" << host
                    << " port=" << port
                    << " dbname=" << database
                    << " user=" << user;

            if (!password.empty()) {
                connStr << " password=" << password;
            }

            std::string connectionString = connStr.str();

            // Configurer le pool de connexions
            pool.configure(connectionString);

            // Retourner la string de connexion (sans le mot de passe pour la sécurité)
            std::ostringstream safeConnStr;
            safeConnStr << "host=" << host
                        << " port=" << port
                        << " dbname=" << database
                        << " user=" << user;

            ctx.setOutput("connection", safeConnStr.str());
        })
        .buildAndRegister();
}

void registerPostgresQueryNode() {
    NodeBuilder("postgres_query", "database")
        .input("query", Type::String)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Récupérer la requête
            auto queryWL = ctx.getInputWorkload("query");

            // Si pas de query en input, essayer la propriété _query
            std::string sql;
            if (queryWL.isNull()) {
                auto queryProp = ctx.getInputWorkload("_query");
                if (queryProp.isNull()) {
                    ctx.setError("No query provided");
                    return;
                }
                sql = queryProp.getString();
            } else {
                sql = queryWL.getString();
            }

            if (sql.empty()) {
                ctx.setError("Empty query");
                return;
            }

            try {
                // Exécuter la requête
                auto& pool = postgres::PostgresPool::instance();

                if (!pool.isConfigured()) {
                    ctx.setError("PostgreSQL not configured. Add a postgres_config node first.");
                    return;
                }

                auto result = pool.executeQuery(sql);
                ctx.setOutput("csv", result);
            }
            catch (const std::exception& e) {
                ctx.setError(std::string("PostgreSQL error: ") + e.what());
            }
        })
        .buildAndRegister();
}

void registerPostgresFuncNode() {
    NodeBuilder("postgres_func", "database")
        .inputOptional("csv", Type::Csv)
        .input("function", Type::String)
        .output("csv", Type::Csv)
        .onCompile([](NodeContext& ctx) {
            // Récupérer le nom de la fonction
            auto funcWL = ctx.getInputWorkload("function");
            std::string funcName;

            if (funcWL.isNull()) {
                auto funcProp = ctx.getInputWorkload("_function");
                if (funcProp.isNull()) {
                    ctx.setError("No function name provided");
                    return;
                }
                funcName = funcProp.getString();
            } else {
                funcName = funcWL.getString();
            }

            try {
                auto& pool = postgres::PostgresPool::instance();

                if (!pool.isConfigured()) {
                    ctx.setError("PostgreSQL not configured. Add a postgres_config node first.");
                    return;
                }

                // Construire la requête avec DynRequest
                DynRequest req;
                req.func(funcName);

                // Ajouter les paramètres depuis les propriétés _param_X
                // Les propriétés sont nommées _int_0, _string_0, _intarray_0, etc.
                for (int i = 0; i < 20; ++i) {
                    std::string idx = std::to_string(i);

                    // Paramètres scalaires
                    auto intProp = ctx.getInputWorkload("_int_" + idx);
                    if (!intProp.isNull()) {
                        req.addIntParam(intProp.getInt());
                        continue;
                    }

                    auto strProp = ctx.getInputWorkload("_string_" + idx);
                    if (!strProp.isNull()) {
                        req.addStringParam(strProp.getString());
                        continue;
                    }

                    auto dblProp = ctx.getInputWorkload("_double_" + idx);
                    if (!dblProp.isNull()) {
                        req.addDoubleParam(dblProp.getDouble());
                        continue;
                    }

                    // Si aucun paramètre trouvé pour cet index, on arrête
                    break;
                }

                std::string sql = req.buildSQL();
                auto result = pool.executeQuery(sql);
                ctx.setOutput("csv", result);
            }
            catch (const std::exception& e) {
                ctx.setError(std::string("PostgreSQL error: ") + e.what());
            }
        })
        .buildAndRegister();
}

} // namespace nodes
