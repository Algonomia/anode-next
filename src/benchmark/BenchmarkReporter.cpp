#include "BenchmarkReporter.hpp"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>

namespace dataframe {

BenchmarkReporter::BenchmarkReporter(const std::string& version)
    : m_version(version), m_timestamp(getCurrentTimestamp()) {
}

void BenchmarkReporter::addResult(
    const std::string& category,
    const std::string& operation,
    long long duration_ms,
    size_t input_rows,
    size_t output_rows,
    const std::string& details
) {
    BenchmarkResult result;
    result.category = category;
    result.operation = operation;
    result.duration_ms = duration_ms;
    result.input_rows = input_rows;
    result.output_rows = output_rows;
    result.details = details;

    m_results.push_back(result);
}

void BenchmarkReporter::startTimer(const std::string& operation_name) {
    m_current_operation = operation_name;
    m_timer_start = high_resolution_clock::now();
}

void BenchmarkReporter::stopTimer(
    const std::string& category,
    size_t input_rows,
    size_t output_rows,
    const std::string& details
) {
    auto end = high_resolution_clock::now();
    auto duration = duration_cast<milliseconds>(end - m_timer_start).count();

    addResult(category, m_current_operation, duration, input_rows, output_rows, details);
}

void BenchmarkReporter::setDatasetInfo(
    size_t rows,
    size_t columns,
    const std::string& filename
) {
    m_dataset_rows = rows;
    m_dataset_columns = columns;
    m_dataset_filename = filename;
}

void BenchmarkReporter::setSystemInfo(
    const std::string& os,
    const std::string& compiler
) {
    m_os = os;
    m_compiler = compiler;
}

json BenchmarkReporter::toJson() const {
    json results_array = json::array();
    for (const auto& result : m_results) {
        results_array.push_back(result.toJson());
    }

    // Calculer les statistiques par catégorie
    json category_stats = json::object();
    std::map<std::string, std::vector<long long>> category_durations;

    for (const auto& result : m_results) {
        category_durations[result.category].push_back(result.duration_ms);
    }

    for (const auto& [category, durations] : category_durations) {
        long long total = std::accumulate(durations.begin(), durations.end(), 0LL);
        long long avg = durations.empty() ? 0 : total / durations.size();
        long long min = durations.empty() ? 0 : *std::min_element(durations.begin(), durations.end());
        long long max = durations.empty() ? 0 : *std::max_element(durations.begin(), durations.end());

        category_stats[category] = {
            {"total_ms", total},
            {"average_ms", avg},
            {"min_ms", min},
            {"max_ms", max},
            {"count", durations.size()}
        };
    }

    json report = {
        {"version", m_version},
        {"timestamp", m_timestamp},
        {"dataset", {
            {"rows", m_dataset_rows},
            {"columns", m_dataset_columns},
            {"filename", m_dataset_filename}
        }},
        {"system", {
            {"os", m_os},
            {"compiler", m_compiler}
        }},
        {"results", results_array},
        {"statistics", {
            {"total_duration_ms", calculateTotalDuration()},
            {"total_operations", m_results.size()},
            {"by_category", category_stats}
        }}
    };

    return report;
}

void BenchmarkReporter::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot create benchmark report file: " + filepath);
    }

    json report = toJson();
    file << report.dump(2);
    file.close();

    std::cout << "Benchmark report saved to: " << filepath << std::endl;
}

void BenchmarkReporter::compareReports(
    const std::string& baseline_file,
    const std::string& current_file,
    const std::string& output_file
) {
    // Charger les deux rapports
    std::ifstream baseline_f(baseline_file);
    std::ifstream current_f(current_file);

    if (!baseline_f.is_open() || !current_f.is_open()) {
        throw std::runtime_error("Cannot open benchmark files for comparison");
    }

    json baseline, current;
    baseline_f >> baseline;
    current_f >> current;

    baseline_f.close();
    current_f.close();

    // Créer le rapport de comparaison
    json comparison = {
        {"baseline", {
            {"file", baseline_file},
            {"version", baseline["version"]},
            {"timestamp", baseline["timestamp"]}
        }},
        {"current", {
            {"file", current_file},
            {"version", current["version"]},
            {"timestamp", current["timestamp"]}
        }},
        {"comparisons", json::array()}
    };

    // Comparer les résultats par opération
    auto baseline_results = baseline["results"];
    auto current_results = current["results"];

    // Créer un map pour accès rapide
    std::map<std::string, long long> baseline_map;
    for (const auto& result : baseline_results) {
        std::string key = result["category"].get<std::string>() + "::" + result["operation"].get<std::string>();
        baseline_map[key] = result["duration_ms"];
    }

    for (const auto& result : current_results) {
        std::string category = result["category"];
        std::string operation = result["operation"];
        std::string key = category + "::" + operation;
        long long current_duration = result["duration_ms"];

        if (baseline_map.find(key) != baseline_map.end()) {
            long long baseline_duration = baseline_map[key];
            long long diff = current_duration - baseline_duration;
            double percent_change = baseline_duration == 0 ? 0.0 :
                (static_cast<double>(diff) / baseline_duration) * 100.0;

            std::string status;
            if (percent_change < -5.0) {
                status = "improved";
            } else if (percent_change > 5.0) {
                status = "regressed";
            } else {
                status = "stable";
            }

            comparison["comparisons"].push_back({
                {"category", category},
                {"operation", operation},
                {"baseline_ms", baseline_duration},
                {"current_ms", current_duration},
                {"diff_ms", diff},
                {"percent_change", percent_change},
                {"status", status}
            });
        }
    }

    // Statistiques globales
    int improved = 0, regressed = 0, stable = 0;
    for (const auto& comp : comparison["comparisons"]) {
        std::string status = comp["status"];
        if (status == "improved") improved++;
        else if (status == "regressed") regressed++;
        else stable++;
    }

    comparison["summary"] = {
        {"total_operations", comparison["comparisons"].size()},
        {"improved", improved},
        {"regressed", regressed},
        {"stable", stable}
    };

    // Sauvegarder
    std::ofstream output(output_file);
    if (!output.is_open()) {
        throw std::runtime_error("Cannot create comparison report: " + output_file);
    }

    output << comparison.dump(2);
    output.close();

    std::cout << "\n=== Benchmark Comparison ===" << std::endl;
    std::cout << "Baseline: " << baseline["version"] << " (" << baseline["timestamp"] << ")" << std::endl;
    std::cout << "Current:  " << current["version"] << " (" << current["timestamp"] << ")" << std::endl;
    std::cout << "\nResults:" << std::endl;
    std::cout << "  Improved:  " << improved << std::endl;
    std::cout << "  Regressed: " << regressed << std::endl;
    std::cout << "  Stable:    " << stable << std::endl;
    std::cout << "\nComparison saved to: " << output_file << std::endl;
}

void BenchmarkReporter::printSummary() const {
    std::cout << "\n=== Benchmark Summary ===" << std::endl;
    std::cout << "Version: " << m_version << std::endl;
    std::cout << "Timestamp: " << m_timestamp << std::endl;
    std::cout << "\nDataset:" << std::endl;
    std::cout << "  Rows: " << m_dataset_rows << std::endl;
    std::cout << "  Columns: " << m_dataset_columns << std::endl;
    std::cout << "  File: " << m_dataset_filename << std::endl;

    std::cout << "\nTotal Operations: " << m_results.size() << std::endl;
    std::cout << "Total Duration: " << calculateTotalDuration() << " ms" << std::endl;

    // Afficher par catégorie
    std::map<std::string, std::vector<long long>> category_durations;
    for (const auto& result : m_results) {
        category_durations[result.category].push_back(result.duration_ms);
    }

    std::cout << "\nBy Category:" << std::endl;
    for (const auto& [category, durations] : category_durations) {
        long long total = std::accumulate(durations.begin(), durations.end(), 0LL);
        std::cout << "  " << std::left << std::setw(20) << category
                  << std::right << std::setw(6) << total << " ms"
                  << " (" << durations.size() << " operations)" << std::endl;
    }
}

std::string BenchmarkReporter::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

long long BenchmarkReporter::calculateTotalDuration() const {
    long long total = 0;
    for (const auto& result : m_results) {
        total += result.duration_ms;
    }
    return total;
}

} // namespace dataframe