#include "server/HttpServer.hpp"
#include "server/RequestHandler.hpp"
#include "server/Logger.hpp"
#include "server/Profiler.hpp"
#include "postgres/PostgresPool.hpp"
#include "plugin_init.hpp"
#include <iostream>
#include <fstream>
#include <csignal>
#include <thread>
#include <map>

using namespace dataframe::server;

namespace {
    std::function<void()> shutdown_handler;
    void signal_handler(int) {
        if (shutdown_handler) shutdown_handler();
    }
}

int main(int argc, char* argv[]) {
    try {
        // Configuration par défaut
        std::string address = "0.0.0.0";
        unsigned short port = 8080;
        std::string datasetPath = "";
        std::string graphsDbPath = "../examples/graphs.db";
        std::string postgresConn = "";  // Connection string or path to config file
        std::string configFile = "";   // App parameters config file
        LogLevel logLevel = LogLevel::INFO;
        bool enableProfiler = true;

        // Arguments optionnels
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if ((arg == "-p" || arg == "--port") && i + 1 < argc) {
                port = static_cast<unsigned short>(std::stoi(argv[++i]));
            } else if ((arg == "-d" || arg == "--dataset") && i + 1 < argc) {
                datasetPath = argv[++i];
            } else if ((arg == "-a" || arg == "--address") && i + 1 < argc) {
                address = argv[++i];
            } else if ((arg == "-l" || arg == "--log-level") && i + 1 < argc) {
                std::string level = argv[++i];
                if (level == "debug") logLevel = LogLevel::DEBUG;
                else if (level == "info") logLevel = LogLevel::INFO;
                else if (level == "warn") logLevel = LogLevel::WARN;
                else if (level == "error") logLevel = LogLevel::ERROR;
            } else if (arg == "--no-profiler") {
                enableProfiler = false;
            } else if ((arg == "-g" || arg == "--graphs-db") && i + 1 < argc) {
                graphsDbPath = argv[++i];
            } else if ((arg == "--postgres") && i + 1 < argc) {
                postgresConn = argv[++i];
            } else if ((arg == "--config") && i + 1 < argc) {
                configFile = argv[++i];
            } else if (arg == "-h" || arg == "--help") {
                std::cout << "Usage: " << argv[0] << " [options]\n"
                          << "Options:\n"
                          << "  -p, --port PORT      Port to listen on (default: 8080)\n"
                          << "  -a, --address ADDR   Address to bind to (default: 0.0.0.0)\n"
                          << "  -d, --dataset PATH   Path to CSV dataset\n"
                          << "  -g, --graphs-db PATH Path to graphs SQLite database (default: ../examples/graphs.db)\n"
                          << "  --postgres CONN      PostgreSQL connection string or path to config file\n"
                          << "                       String: \"host=localhost port=5432 dbname=mydb user=postgres\"\n"
                          << "                       File: @/path/to/postgres.conf (one param per line)\n"
                          << "  --config FILE        App parameters file (key=value lines, @file syntax)\n"
                          << "  -l, --log-level LVL  Log level: debug, info, warn, error (default: info)\n"
                          << "  --no-profiler        Disable profiler\n"
                          << "  -h, --help           Show this help\n";
                return 0;
            }
        }

        // Configure Logger
        Logger::instance().setLevel(logLevel);

        // Configure Profiler
        Profiler::instance().setEnabled(enableProfiler);

        std::cout << "=== AnodeServer ===" << std::endl;
        std::cout << std::endl;

        // Register all node types (from plugins)
        registerNodePlugins();
        LOG_INFO("Node plugins registered");

        // Parse app parameters config file
        std::map<std::string, std::string> appParams;
        if (!configFile.empty()) {
            std::string path = configFile;
            if (path[0] == '@') path = path.substr(1);
            std::ifstream paramFile(path);
            if (!paramFile.is_open()) {
                std::cerr << "Error: Cannot open config file: " << path << std::endl;
                return 1;
            }
            auto trim = [](std::string s) {
                while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\r' || s.back() == '\n')) s.pop_back();
                while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(s.begin());
                return s;
            };
            std::string line;
            while (std::getline(paramFile, line)) {
                line = trim(line);
                if (line.empty() || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = trim(line.substr(0, eq));
                std::string val = trim(line.substr(eq + 1));
                appParams[key] = val;
            }
            LOG_INFO("Loaded " + std::to_string(appParams.size()) + " app parameters");
        }

        // Configurer PostgreSQL si spécifié
        if (!postgresConn.empty()) {
            std::string connString = postgresConn;

            // Si commence par @, c'est un chemin vers un fichier de config
            if (postgresConn[0] == '@') {
                std::string configPath = postgresConn.substr(1);
                std::ifstream configFile(configPath);
                if (!configFile.is_open()) {
                    std::cerr << "Error: Cannot open PostgreSQL config file: " << configPath << std::endl;
                    return 1;
                }

                // Lire le fichier ligne par ligne et construire la connection string
                connString.clear();
                std::string line;
                while (std::getline(configFile, line)) {
                    // Ignorer les lignes vides et les commentaires
                    if (line.empty() || line[0] == '#') continue;
                    if (!connString.empty()) connString += " ";
                    connString += line;
                }
            }

            postgres::PostgresPool::instance().configure(connString);
            LOG_INFO("PostgreSQL configured");
        }

        // Initialiser le stockage de graphes
        RequestHandler::instance().initGraphStorage(graphsDbPath);

        // Charger le dataset (optionnel)
        if (!datasetPath.empty()) {
            RequestHandler::instance().loadDataset(datasetPath);
            std::cout << std::endl;
        }

        // Créer le contexte IO
        net::io_context ioc{1};

        // Initialize plugins (caches, storage, route handlers)
        nodes::PluginContext pluginCtx;
        pluginCtx.ioc = &ioc;
        pluginCtx.storage = RequestHandler::instance().getGraphStorage();
        pluginCtx.handler = &RequestHandler::instance();
        pluginCtx.dbConnString = postgres::PostgresPool::instance().isConfigured()
            ? postgres::PostgresPool::instance().getConnectionString() : "";
        pluginCtx.params = std::move(appParams);
        initNodePlugins(pluginCtx);
        LOG_INFO("Node plugins initialized");

        // Créer et démarrer le serveur
        HttpServer server(ioc, address, port);
        server.run();

        // Start plugin listeners (e.g., PostgreSQL LISTEN/NOTIFY)
        startPluginListeners(pluginCtx);

        // Gérer le signal d'arrêt
        shutdown_handler = [&]() {
            LOG_INFO("Shutting down...");

            stopPluginListeners();
            shutdownNodePlugins();

            // Afficher les stats du profiler
            if (Profiler::instance().isEnabled()) {
                std::cout << Profiler::instance().formatStats() << std::endl;
            }

            server.stop();
            ioc.stop();
        };
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);

        std::cout << std::endl;
        std::cout << "Endpoints:" << std::endl;
        std::cout << "  GET  /api/health              - Health check" << std::endl;
        std::cout << "  GET  /api/dataset/info        - Dataset information" << std::endl;
        std::cout << "  POST /api/dataset/query       - Execute query pipeline" << std::endl;
        std::cout << std::endl;
        std::cout << "  GET  /api/nodes               - List node definitions" << std::endl;
        std::cout << std::endl;
        std::cout << "  GET  /api/graphs              - List all graphs" << std::endl;
        std::cout << "  POST /api/graph               - Create a new graph" << std::endl;
        std::cout << "  GET  /api/graph/:slug         - Get a graph" << std::endl;
        std::cout << "  PUT  /api/graph/:slug         - Update graph (new version)" << std::endl;
        std::cout << "  DELETE /api/graph/:slug       - Delete a graph" << std::endl;
        std::cout << "  GET  /api/graph/:slug/versions - List graph versions" << std::endl;
        std::cout << "  POST /api/graph/:slug/execute - Execute a graph" << std::endl;
        std::cout << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        std::cout << std::endl;

        // Lancer la boucle d'événements
        ioc.run();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
