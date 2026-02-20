/**
 * Benchmark comparing hosseinmoein/DataFrame performance
 * Same operations as benchmark_optimized.cpp for comparison
 */

#include <DataFrame/DataFrame.h>
#include <DataFrame/DataFrameStatsVisitors.h>

#include <iostream>
#include <chrono>
#include <iomanip>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <ctime>

using namespace hmdf;
using namespace std::chrono;

// DataFrame with unsigned long index
using MyDataFrame = StdDataFrame<unsigned long>;

// Simple benchmark result structure
struct BenchmarkResult {
    std::string name;
    std::string category;
    long long duration_ms;
    size_t input_rows;
    size_t output_rows;
};

// Global results collector
std::vector<BenchmarkResult> g_results;

class BenchmarkTimer {
public:
    BenchmarkTimer(const std::string& name, const std::string& category = "", size_t input_rows = 0)
        : m_name(name), m_category(category), m_input_rows(input_rows), m_output_rows(0)
    {
        m_start = high_resolution_clock::now();
    }

    ~BenchmarkTimer() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - m_start).count();

        std::cout << std::left << std::setw(50) << m_name
                  << std::right << std::setw(10) << duration << " ms" << std::endl;

        if (!m_category.empty()) {
            g_results.push_back({m_name, m_category, duration, m_input_rows, m_output_rows});
        }
    }

    void setOutputRows(size_t rows) { m_output_rows = rows; }

private:
    std::string m_name;
    std::string m_category;
    size_t m_input_rows;
    size_t m_output_rows;
    time_point<high_resolution_clock> m_start;
};

void printSeparator() {
    std::cout << std::string(65, '=') << std::endl;
}

void printHeader(const std::string& title) {
    printSeparator();
    std::cout << "  " << title << std::endl;
    printSeparator();
}

void printSummary(size_t rows, size_t cols, const std::string& filename) {
    // Calculate totals by category
    std::map<std::string, std::pair<long long, int>> byCategory;
    long long totalDuration = 0;
    int totalOps = 0;

    for (const auto& r : g_results) {
        byCategory[r.category].first += r.duration_ms;
        byCategory[r.category].second++;
        totalDuration += r.duration_ms;
        totalOps++;
    }

    // Get current timestamp
    auto now = std::time(nullptr);
    char timestamp[64];
    std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

    std::cout << "\n=== Benchmark Summary ===" << std::endl;
    std::cout << "Version: hosseinmoein/DataFrame" << std::endl;
    std::cout << "Timestamp: " << timestamp << std::endl;

    std::cout << "\nDataset:" << std::endl;
    std::cout << "  Rows: " << rows << std::endl;
    std::cout << "  Columns: " << cols << std::endl;
    std::cout << "  File: " << filename << std::endl;

    std::cout << "\nTotal Operations: " << totalOps << std::endl;
    std::cout << "Total Duration: " << totalDuration << " ms" << std::endl;

    std::cout << "\nBy Category:" << std::endl;
    for (const auto& [cat, data] : byCategory) {
        std::cout << "  " << std::left << std::setw(20) << cat
                  << std::right << std::setw(6) << data.first << " ms ("
                  << data.second << " operations)" << std::endl;
    }
}

int main() {
    std::cout << "\n";
    printHeader("hosseinmoein/DataFrame Benchmark - 500,000 rows CSV");

    MyDataFrame df;
    size_t datasetRows = 0;

    // ========================================================================
    // 1. Load CSV
    // ========================================================================
    {
        printHeader("1. CSV Loading (hosseinmoein/DataFrame)");
        BenchmarkTimer timer("Load CSV (500,000 rows) - HMDF", "IO");

        // Define schema for the CSV file
        ReadParams params;
        params.skip_first_line = true;
        params.schema = {
            { DF_INDEX_COL_NAME, file_dtypes::ULONG, 500000, 0 },
            { "Customer Id", file_dtypes::STRING, 500000, 1 },
            { "First Name", file_dtypes::STRING, 500000, 2 },
            { "Last Name", file_dtypes::STRING, 500000, 3 },
            { "Company", file_dtypes::STRING, 500000, 4 },
            { "City", file_dtypes::STRING, 500000, 5 },
            { "Country", file_dtypes::STRING, 500000, 6 },
            { "Phone 1", file_dtypes::STRING, 500000, 7 },
            { "Phone 2", file_dtypes::STRING, 500000, 8 },
            { "Email", file_dtypes::STRING, 500000, 9 },
            { "Subscription Date", file_dtypes::STRING, 500000, 10 },
            { "Website", file_dtypes::STRING, 500000, 11 }
        };

        df.read("../examples/customers-500000.csv", io_format::csv2, params);
        datasetRows = df.get_index().size();
        timer.setOutputRows(datasetRows);
    }

    std::cout << "\nDataFrame Info:" << std::endl;
    std::cout << "  Rows: " << df.get_index().size() << std::endl;

    // ========================================================================
    // 2. Filter Operations
    // ========================================================================
    {
        printHeader("2. Filter Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("filter_equality_simple_HMDF", "Filter", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val == "Norway";
            };
            auto result = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Country", filter_fn);
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " rows" << std::endl;
        }

        {
            BenchmarkTimer timer("filter_contains_HMDF", "Filter", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val.find("Lake") != std::string::npos;
            };
            auto result = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("City", filter_fn);
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " rows" << std::endl;
        }

        {
            BenchmarkTimer timer("filter_multi_condition_HMDF", "Filter", datasetRows);
            auto filter_fn = [](const unsigned long&,
                               const std::string& country,
                               const std::string& city) -> bool {
                return country == "Norway" && city.find("Lake") != std::string::npos;
            };
            auto result = df.get_data_by_sel<std::string, std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Country", "City", filter_fn);
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " rows" << std::endl;
        }

        {
            BenchmarkTimer timer("filter_email_domain_HMDF", "Filter", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val.find("@espinoza") != std::string::npos;
            };
            auto result = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Email", filter_fn);
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " rows" << std::endl;
        }
    }

    // ========================================================================
    // 3. OrderBy Operations
    // ========================================================================
    {
        printHeader("3. OrderBy Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("orderby_single_asc_HMDF", "OrderBy", datasetRows);
            auto result = df;
            result.sort<std::string, unsigned long, std::string, std::string,
                       std::string, std::string, std::string, std::string,
                       std::string, std::string, std::string, std::string>(
                "Country", sort_spec::ascen);
            timer.setOutputRows(result.get_index().size());
        }

        {
            BenchmarkTimer timer("orderby_single_desc_HMDF", "OrderBy", datasetRows);
            auto result = df;
            result.sort<std::string, unsigned long, std::string, std::string,
                       std::string, std::string, std::string, std::string,
                       std::string, std::string, std::string, std::string>(
                "City", sort_spec::desce);
            timer.setOutputRows(result.get_index().size());
        }

        {
            BenchmarkTimer timer("orderby_multi_2cols_HMDF", "OrderBy", datasetRows);
            auto result = df;
            result.sort<std::string, std::string, unsigned long, std::string,
                       std::string, std::string, std::string, std::string,
                       std::string, std::string, std::string, std::string>(
                "Country", sort_spec::ascen,
                "City", sort_spec::ascen);
            timer.setOutputRows(result.get_index().size());
        }

        {
            BenchmarkTimer timer("orderby_multi_3cols_HMDF", "OrderBy", datasetRows);
            auto result = df;
            result.sort<std::string, std::string, std::string, unsigned long,
                       std::string, std::string, std::string, std::string,
                       std::string, std::string, std::string, std::string>(
                "Country", sort_spec::ascen,
                "Last Name", sort_spec::desce,
                "First Name", sort_spec::ascen);
            timer.setOutputRows(result.get_index().size());
        }
    }

    // ========================================================================
    // 4. GroupBy Operations
    // ========================================================================
    {
        printHeader("4. GroupBy Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("groupby_count_country_HMDF", "GroupBy", datasetRows);
            auto result = df.groupby1<std::string>(
                "Country",
                LastVisitor<unsigned long, unsigned long>(),
                std::make_tuple("Customer Id", "customer_count", CountVisitor<std::string>())
            );
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " groups" << std::endl;
        }

        {
            BenchmarkTimer timer("groupby_count_city_HMDF", "GroupBy", datasetRows);
            auto result = df.groupby1<std::string>(
                "City",
                LastVisitor<unsigned long, unsigned long>(),
                std::make_tuple("Customer Id", "count", CountVisitor<std::string>())
            );
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " unique cities" << std::endl;
        }

        {
            BenchmarkTimer timer("groupby_multi_country_city_HMDF", "GroupBy", datasetRows);
            auto result = df.groupby2<std::string, std::string>(
                "Country", "City",
                LastVisitor<unsigned long, unsigned long>(),
                std::make_tuple("Customer Id", "count", CountVisitor<std::string>())
            );
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " unique combinations" << std::endl;
        }

        {
            BenchmarkTimer timer("groupby_count_company_HMDF", "GroupBy", datasetRows);
            auto result = df.groupby1<std::string>(
                "Company",
                LastVisitor<unsigned long, unsigned long>(),
                std::make_tuple("Customer Id", "employee_count", CountVisitor<std::string>())
            );
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " unique companies" << std::endl;
        }
    }

    // ========================================================================
    // 5. Select Operations
    // ========================================================================
    {
        printHeader("5. Select Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("select_3cols_HMDF", "Select", datasetRows);
            const auto& first_name = df.get_column<std::string>("First Name");
            const auto& last_name = df.get_column<std::string>("Last Name");
            const auto& email = df.get_column<std::string>("Email");

            MyDataFrame result;
            std::vector<unsigned long> idx(df.get_index().begin(), df.get_index().end());
            result.load_data(std::move(idx),
                std::make_pair("First Name", std::vector<std::string>(first_name.begin(), first_name.end())),
                std::make_pair("Last Name", std::vector<std::string>(last_name.begin(), last_name.end())),
                std::make_pair("Email", std::vector<std::string>(email.begin(), email.end()))
            );
            timer.setOutputRows(result.get_index().size());
        }

        {
            BenchmarkTimer timer("select_6cols_HMDF", "Select", datasetRows);
            const auto& customer_id = df.get_column<std::string>("Customer Id");
            const auto& first_name = df.get_column<std::string>("First Name");
            const auto& last_name = df.get_column<std::string>("Last Name");
            const auto& company = df.get_column<std::string>("Company");
            const auto& city = df.get_column<std::string>("City");
            const auto& country = df.get_column<std::string>("Country");

            MyDataFrame result;
            std::vector<unsigned long> idx(df.get_index().begin(), df.get_index().end());
            result.load_data(std::move(idx),
                std::make_pair("Customer Id", std::vector<std::string>(customer_id.begin(), customer_id.end())),
                std::make_pair("First Name", std::vector<std::string>(first_name.begin(), first_name.end())),
                std::make_pair("Last Name", std::vector<std::string>(last_name.begin(), last_name.end())),
                std::make_pair("Company", std::vector<std::string>(company.begin(), company.end())),
                std::make_pair("City", std::vector<std::string>(city.begin(), city.end())),
                std::make_pair("Country", std::vector<std::string>(country.begin(), country.end()))
            );
            timer.setOutputRows(result.get_index().size());
        }
    }

    // ========================================================================
    // 6. Chained Operations
    // ========================================================================
    {
        printHeader("6. Chained Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("chain_filter_orderby_HMDF", "Chain", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val == "Norway";
            };
            auto filtered = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Country", filter_fn);
            filtered.sort<std::string, unsigned long, std::string, std::string,
                         std::string, std::string, std::string, std::string,
                         std::string, std::string, std::string, std::string>(
                "City", sort_spec::ascen);
            timer.setOutputRows(filtered.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << filtered.get_index().size() << " rows" << std::endl;
        }

        {
            BenchmarkTimer timer("chain_filter_orderby_select_HMDF", "Chain", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val == "Norway";
            };
            auto filtered = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Country", filter_fn);
            filtered.sort<std::string, unsigned long, std::string, std::string,
                         std::string, std::string, std::string, std::string,
                         std::string, std::string, std::string, std::string>(
                "Last Name", sort_spec::ascen);

            const auto& first_name = filtered.get_column<std::string>("First Name");
            const auto& last_name = filtered.get_column<std::string>("Last Name");
            const auto& city = filtered.get_column<std::string>("City");

            MyDataFrame result;
            std::vector<unsigned long> idx(filtered.get_index().begin(), filtered.get_index().end());
            result.load_data(std::move(idx),
                std::make_pair("First Name", std::vector<std::string>(first_name.begin(), first_name.end())),
                std::make_pair("Last Name", std::vector<std::string>(last_name.begin(), last_name.end())),
                std::make_pair("City", std::vector<std::string>(city.begin(), city.end()))
            );
            timer.setOutputRows(result.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << result.get_index().size() << " rows" << std::endl;
        }

        {
            BenchmarkTimer timer("chain_filter_groupby_orderby_HMDF", "Chain", datasetRows);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val.find("Lake") != std::string::npos;
            };
            auto filtered = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("City", filter_fn);
            auto grouped = filtered.groupby1<std::string>(
                "Country",
                LastVisitor<unsigned long, unsigned long>(),
                std::make_tuple("Customer Id", "count", CountVisitor<std::string>())
            );
            grouped.sort<size_t, unsigned long, std::string>(
                "count", sort_spec::desce);
            timer.setOutputRows(grouped.get_index().size());
            std::cout << std::string(50, ' ') << "Result: " << grouped.get_index().size() << " groups" << std::endl;
        }
    }

    // ========================================================================
    // 7. Export Operations
    // ========================================================================
    {
        printHeader("7. Export Operations (hosseinmoein/DataFrame)");

        {
            BenchmarkTimer timer("export_csv_filtered_HMDF", "Export", 51);
            auto filter_fn = [](const unsigned long&, const std::string& val) -> bool {
                return val == "Norway";
            };
            auto filtered = df.get_data_by_sel<std::string, decltype(filter_fn),
                unsigned long, std::string, std::string, std::string, std::string,
                std::string, std::string, std::string, std::string, std::string,
                std::string, std::string>("Country", filter_fn);
            std::ofstream outfile("../examples/output_norway_hmdf.csv");
            filtered.write<std::ostream, unsigned long, std::string, std::string,
                          std::string, std::string, std::string, std::string,
                          std::string, std::string, std::string, std::string,
                          std::string>(outfile, io_format::csv2);
            timer.setOutputRows(filtered.get_index().size());
        }
    }

    // ========================================================================
    // Summary
    // ========================================================================
    printSeparator();
    printSummary(datasetRows, 12, "customers-500000.csv");

    std::cout << "\n";

    return 0;
}
