#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace dataframe {

using json = nlohmann::json;
using namespace std::chrono;

/**
 * Classe pour enregistrer et comparer les résultats de benchmarks
 */
class BenchmarkReporter {
public:
    struct BenchmarkResult {
        std::string category;      // Ex: "Filter", "OrderBy", "GroupBy"
        std::string operation;     // Description détaillée
        long long duration_ms;     // Durée en millisecondes
        size_t input_rows;         // Nombre de lignes en entrée
        size_t output_rows;        // Nombre de lignes en sortie
        std::string details;       // Informations supplémentaires

        json toJson() const {
            return {
                {"category", category},
                {"operation", operation},
                {"duration_ms", duration_ms},
                {"input_rows", input_rows},
                {"output_rows", output_rows},
                {"details", details}
            };
        }
    };

    BenchmarkReporter(const std::string& version = "1.0.0");

    // Ajouter un résultat
    void addResult(
        const std::string& category,
        const std::string& operation,
        long long duration_ms,
        size_t input_rows = 0,
        size_t output_rows = 0,
        const std::string& details = ""
    );

    // Démarrer un timer
    void startTimer(const std::string& operation_name);

    // Arrêter le timer et enregistrer
    void stopTimer(
        const std::string& category,
        size_t input_rows = 0,
        size_t output_rows = 0,
        const std::string& details = ""
    );

    // Métadonnées
    void setDatasetInfo(size_t rows, size_t columns, const std::string& filename);
    void setSystemInfo(const std::string& os, const std::string& compiler);

    // Export
    void saveToFile(const std::string& filepath) const;
    json toJson() const;

    // Comparaison
    static void compareReports(
        const std::string& baseline_file,
        const std::string& current_file,
        const std::string& output_file
    );

    // Affichage
    void printSummary() const;

private:
    std::string m_version;
    std::string m_timestamp;
    std::vector<BenchmarkResult> m_results;

    // Dataset info
    size_t m_dataset_rows = 0;
    size_t m_dataset_columns = 0;
    std::string m_dataset_filename;

    // System info
    std::string m_os;
    std::string m_compiler;

    // Timer actuel
    std::string m_current_operation;
    time_point<high_resolution_clock> m_timer_start;

    std::string getCurrentTimestamp() const;
    long long calculateTotalDuration() const;
};

/**
 * Helper RAII pour mesurer automatiquement le temps d'une opération
 */
class ScopedBenchmark {
public:
    ScopedBenchmark(
        BenchmarkReporter& reporter,
        const std::string& category,
        const std::string& operation,
        size_t input_rows = 0
    ) : m_reporter(reporter),
        m_category(category),
        m_operation(operation),
        m_input_rows(input_rows),
        m_output_rows(0)
    {
        m_start = high_resolution_clock::now();
    }

    ~ScopedBenchmark() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - m_start).count();
        m_reporter.addResult(m_category, m_operation, duration, m_input_rows, m_output_rows);
    }

    void setOutputRows(size_t rows) { m_output_rows = rows; }
    void setDetails(const std::string& details) { m_details = details; }

private:
    BenchmarkReporter& m_reporter;
    std::string m_category;
    std::string m_operation;
    size_t m_input_rows;
    size_t m_output_rows;
    std::string m_details;
    time_point<high_resolution_clock> m_start;
};

} // namespace dataframe