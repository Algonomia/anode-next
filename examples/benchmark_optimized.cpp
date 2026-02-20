#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameIO.hpp"
#include "dataframe/DataFrameJoiner.hpp"
#include "benchmark/BenchmarkReporter.hpp"
#include <iostream>
#include <chrono>
#include <iomanip>

using namespace dataframe;
using namespace std::chrono;

class BenchmarkTimer {
public:
    BenchmarkTimer(
        const std::string& name,
        BenchmarkReporter* reporter = nullptr,
        const std::string& category = "",
        size_t input_rows = 0
    ) : m_name(name),
        m_reporter(reporter),
        m_category(category),
        m_input_rows(input_rows),
        m_output_rows(0)
    {
        m_start = high_resolution_clock::now();
    }

    ~BenchmarkTimer() {
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - m_start).count();

        std::cout << std::left << std::setw(50) << m_name
                  << std::right << std::setw(10) << duration << " ms" << std::endl;

        if (m_reporter && !m_category.empty()) {
            m_reporter->addResult(m_category, m_name, duration, m_input_rows, m_output_rows);
        }
    }

    void setOutputRows(size_t rows) { m_output_rows = rows; }

private:
    std::string m_name;
    BenchmarkReporter* m_reporter;
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

int main() {
    std::cout << "\n";
    printHeader("DataFrame OPTIMIZED Benchmark - 500,000 rows CSV");

    BenchmarkReporter reporter("1.0.0-optimized");
    reporter.setSystemInfo("Linux", "g++ 13.3.0");

    std::shared_ptr<DataFrame> df;
    size_t datasetRows = 0;

    // ========================================================================
    // 1. Load CSV
    // ========================================================================
    {
        printHeader("1. CSV Loading (OPTIMIZED)");
        BenchmarkTimer timer("Load CSV (500,000 rows) - OPTIMIZED", &reporter, "IO");
        df = DataFrameIO::readCSV("../examples/customers-500000.csv");
        datasetRows = df->rowCount();
        timer.setOutputRows(datasetRows);
    }

    reporter.setDatasetInfo(df->rowCount(), df->columnCount(), "customers-500000.csv");

    std::cout << "\nDataFrame Info:" << std::endl;
    std::cout << "  Rows: " << df->rowCount() << std::endl;
    std::cout << "  Columns: " << df->columnCount() << std::endl;
    std::cout << "  String pool size: " << df->getStringPool()->size() << " unique strings" << std::endl;
    std::cout << "  String pool memory: " << (df->getStringPool()->memoryUsage() / 1024) << " KB" << std::endl;

    std::cout << "\nFirst 3 rows:" << std::endl;
    std::cout << df->toString(3) << std::endl;

    // ========================================================================
    // 2. Filter Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("2. Filter Operations (OPTIMIZED)");

        // Filter 1: Simple equality
        {
            BenchmarkTimer timer("filter_equality_simple_OPT", &reporter, "Filter", datasetRows);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}}
            });
            auto result = df->filter(filter);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }

        // Filter 2: Contains
        {
            BenchmarkTimer timer("filter_contains_OPT", &reporter, "Filter", datasetRows);
            json filter = json::array({
                {{"column", "City"}, {"operator", "contains"}, {"value", "Lake"}}
            });
            auto result = df->filter(filter);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }

        // Filter 3: Multi-condition
        {
            BenchmarkTimer timer("filter_multi_condition_OPT", &reporter, "Filter", datasetRows);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}},
                {{"column", "City"}, {"operator", "contains"}, {"value", "Lake"}}
            });
            auto result = df->filter(filter);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }

        // Filter 4: Email domain
        {
            BenchmarkTimer timer("filter_email_domain_OPT", &reporter, "Filter", datasetRows);
            json filter = json::array({
                {{"column", "Email"}, {"operator", "contains"}, {"value", "@espinoza"}}
            });
            auto result = df->filter(filter);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }
    }

    // ========================================================================
    // 3. OrderBy Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("3. OrderBy Operations (OPTIMIZED)");

        // Sort 1: Single column ASC
        {
            BenchmarkTimer timer("orderby_single_asc_OPT", &reporter, "OrderBy", datasetRows);
            json order = json::array({
                {{"column", "Country"}, {"order", "asc"}}
            });
            auto result = df->orderBy(order);
            timer.setOutputRows(result->rowCount());
        }

        // Sort 2: Single column DESC
        {
            BenchmarkTimer timer("orderby_single_desc_OPT", &reporter, "OrderBy", datasetRows);
            json order = json::array({
                {{"column", "City"}, {"order", "desc"}}
            });
            auto result = df->orderBy(order);
            timer.setOutputRows(result->rowCount());
        }

        // Sort 3: Multi-column (2 cols)
        {
            BenchmarkTimer timer("orderby_multi_2cols_OPT", &reporter, "OrderBy", datasetRows);
            json order = json::array({
                {{"column", "Country"}, {"order", "asc"}},
                {{"column", "City"}, {"order", "asc"}}
            });
            auto result = df->orderBy(order);
            timer.setOutputRows(result->rowCount());
        }

        // Sort 4: Multi-column complex (3 cols)
        {
            BenchmarkTimer timer("orderby_multi_3cols_OPT", &reporter, "OrderBy", datasetRows);
            json order = json::array({
                {{"column", "Country"}, {"order", "asc"}},
                {{"column", "Last Name"}, {"order", "desc"}},
                {{"column", "First Name"}, {"order", "asc"}}
            });
            auto result = df->orderBy(order);
            timer.setOutputRows(result->rowCount());
        }
    }

    // ========================================================================
    // 4. GroupBy Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("4. GroupBy Operations (OPTIMIZED)");

        // GroupBy 1: Count by country
        {
            BenchmarkTimer timer("groupby_count_country_OPT", &reporter, "GroupBy", datasetRows);
            json groupBy = {
                {"groupBy", json::array({"Country"})},
                {"aggregations", json::array({
                    {{"column", "Index"}, {"function", "count"}, {"alias", "customer_count"}}
                })}
            };
            auto result = df->groupBy(groupBy);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " groups" << std::endl;
            std::cout << "\nTop 5 countries by customer count:" << std::endl;

            // Sort by count descending
            json order = json::array({
                {{"column", "customer_count"}, {"order", "desc"}}
            });
            auto sorted = result->orderBy(order);
            std::cout << sorted->toString(5) << std::endl;
        }

        // GroupBy 2: Count by city
        {
            BenchmarkTimer timer("groupby_count_city_OPT", &reporter, "GroupBy", datasetRows);
            json groupBy = {
                {"groupBy", json::array({"City"})},
                {"aggregations", json::array({
                    {{"column", "Index"}, {"function", "count"}, {"alias", "count"}}
                })}
            };
            auto result = df->groupBy(groupBy);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " unique cities" << std::endl;
        }

        // GroupBy 3: Multi-column grouping
        {
            BenchmarkTimer timer("groupby_multi_country_city_OPT", &reporter, "GroupBy", datasetRows);
            json groupBy = {
                {"groupBy", json::array({"Country", "City"})},
                {"aggregations", json::array({
                    {{"column", "Index"}, {"function", "count"}, {"alias", "count"}}
                })}
            };
            auto result = df->groupBy(groupBy);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " unique combinations" << std::endl;
        }

        // GroupBy 4: Count by Company
        {
            BenchmarkTimer timer("groupby_count_company_OPT", &reporter, "GroupBy", datasetRows);
            json groupBy = {
                {"groupBy", json::array({"Company"})},
                {"aggregations", json::array({
                    {{"column", "Index"}, {"function", "count"}, {"alias", "employee_count"}}
                })}
            };
            auto result = df->groupBy(groupBy);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " unique companies" << std::endl;
        }
    }

    // ========================================================================
    // 5. Select Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("5. Select Operations (OPTIMIZED)");

        {
            BenchmarkTimer timer("select_3cols_OPT", &reporter, "Select", datasetRows);
            auto result = df->select({"First Name", "Last Name", "Email"});
            timer.setOutputRows(result->rowCount());
        }

        {
            BenchmarkTimer timer("select_6cols_OPT", &reporter, "Select", datasetRows);
            auto result = df->select({"Index", "First Name", "Last Name", "Company", "City", "Country"});
            timer.setOutputRows(result->rowCount());
        }
    }

    // ========================================================================
    // 6. Chained Operations (Complex Queries) (OPTIMIZED)
    // ========================================================================
    {
        printHeader("6. Chained Operations (OPTIMIZED)");

        // Chain 1: Filter + OrderBy
        {
            BenchmarkTimer timer("chain_filter_orderby_OPT", &reporter, "Chain", datasetRows);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}}
            });
            json order = json::array({
                {{"column", "City"}, {"order", "asc"}}
            });
            auto result = df->filter(filter)->orderBy(order);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }

        // Chain 2: Filter + OrderBy + Select
        {
            BenchmarkTimer timer("chain_filter_orderby_select_OPT", &reporter, "Chain", datasetRows);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}}
            });
            json order = json::array({
                {{"column", "Last Name"}, {"order", "asc"}}
            });
            auto result = df->filter(filter)
                            ->orderBy(order)
                            ->select({"First Name", "Last Name", "City"});
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows" << std::endl;
        }

        // Chain 3: Filter (multi) + GroupBy + OrderBy
        {
            BenchmarkTimer timer("chain_filter_groupby_orderby_OPT", &reporter, "Chain", datasetRows);
            json filter = json::array({
                {{"column", "City"}, {"operator", "contains"}, {"value", "Lake"}}
            });
            json groupBy = {
                {"groupBy", json::array({"Country"})},
                {"aggregations", json::array({
                    {{"column", "Index"}, {"function", "count"}, {"alias", "count"}}
                })}
            };
            json order = json::array({
                {{"column", "count"}, {"order", "desc"}}
            });
            auto result = df->filter(filter)->groupBy(groupBy)->orderBy(order);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " groups" << std::endl;
            std::cout << "\nCountries with cities containing 'Lake':" << std::endl;
            std::cout << result->toString(10) << std::endl;
        }
    }

    // ========================================================================
    // 7. Export Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("7. Export Operations (OPTIMIZED)");

        // Export to JSON (sample)
        {
            BenchmarkTimer timer("export_json_sample_OPT", &reporter, "Export", 51);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}}
            });
            auto sample = df->filter(filter);
            json exported = sample->toJson();
            timer.setOutputRows(sample->rowCount());
        }

        // Export to CSV
        {
            BenchmarkTimer timer("export_csv_full_OPT", &reporter, "Export", datasetRows);
            DataFrameIO::writeCSV(*df, "../examples/output_benchmark_opt.csv");
            timer.setOutputRows(datasetRows);
        }

        // Export filtered data to CSV
        {
            BenchmarkTimer timer("export_csv_filtered_OPT", &reporter, "Export", 51);
            json filter = json::array({
                {{"column", "Country"}, {"operator", "=="}, {"value", "Norway"}}
            });
            auto filtered = df->filter(filter);
            DataFrameIO::writeCSV(*filtered, "../examples/output_norway_opt.csv");
            timer.setOutputRows(filtered->rowCount());
        }
    }

    // ========================================================================
    // 8. Join Operations (OPTIMIZED)
    // ========================================================================
    {
        printHeader("8. Join Operations (OPTIMIZED)");

        // Create a lookup table for countries
        auto countryLookup = std::make_shared<DataFrame>();
        countryLookup->addStringColumn("Country");
        countryLookup->addStringColumn("Region");
        countryLookup->addIntColumn("CountryCode");

        // Add some countries with regions
        countryLookup->addRow({"Norway", "Scandinavia", "47"});
        countryLookup->addRow({"Sweden", "Scandinavia", "46"});
        countryLookup->addRow({"Denmark", "Scandinavia", "45"});
        countryLookup->addRow({"Finland", "Scandinavia", "358"});
        countryLookup->addRow({"Germany", "Western Europe", "49"});
        countryLookup->addRow({"France", "Western Europe", "33"});
        countryLookup->addRow({"Spain", "Southern Europe", "34"});
        countryLookup->addRow({"Italy", "Southern Europe", "39"});
        countryLookup->addRow({"United Kingdom", "Northern Europe", "44"});
        countryLookup->addRow({"Netherlands", "Western Europe", "31"});

        std::cout << "Lookup table: " << countryLookup->rowCount() << " countries" << std::endl;

        // Inner Join
        {
            BenchmarkTimer timer("innerJoin_country_lookup_OPT", &reporter, "Join", datasetRows);
            json joinSpec = {
                {"keys", json::array({
                    {{"left", "Country"}, {"right", "Country"}}
                })}
            };
            auto result = df->innerJoin(countryLookup, joinSpec);
            timer.setOutputRows(result->rowCount());
            std::cout << std::string(50, ' ') << "Result: " << result->rowCount() << " rows (matching countries)" << std::endl;
        }

        // Flex Join - ONLY single matches (should match innerJoin performance)
        {
            BenchmarkTimer timer("flexJoin_country_SingleOnly_OPT", &reporter, "Join", datasetRows);
            json joinSpec = {
                {"keys", json::array({
                    {{"left", "Country"}, {"right", "Country"}}
                })}
            };
            FlexJoinOptions options;
            options.noMatchMode = JoinMode::Skip;              // Ne rien écrire
            options.singleMatchMode = JoinMode::KeepAll;       // On garde tout
            options.multipleMatchMode = JoinMode::Skip;        // Ne rien écrire

            auto result = DataFrameJoiner::flexJoin(
                joinSpec,
                options,
                df->rowCount(),
                [&](const std::string& name) { return df->getColumn(name); },
                df->getColumnNames(),
                df->getStringPool(),
                countryLookup->rowCount(),
                [&](const std::string& name) { return countryLookup->getColumn(name); },
                countryLookup->getColumnNames(),
                countryLookup->getStringPool()
            );

            // On ne compte que singleMatch car c'est l'équivalent de innerJoin
            timer.setOutputRows(result.singleMatch->rowCount());
            std::cout << std::string(50, ' ')
                      << "singleMatch: " << result.singleMatch->rowCount() << " rows (comparable to innerJoin)" << std::endl;
        }

        // Flex Join - All modes
        {
            BenchmarkTimer timer("flexJoin_country_KeepAll_OPT", &reporter, "Join", datasetRows);
            json joinSpec = {
                {"keys", json::array({
                    {{"left", "Country"}, {"right", "Country"}}
                })}
            };
            FlexJoinOptions options;
            options.noMatchMode = JoinMode::KeepHeaderOnly;
            options.singleMatchMode = JoinMode::KeepAll;
            options.multipleMatchMode = JoinMode::KeepAll;

            auto result = DataFrameJoiner::flexJoin(
                joinSpec,
                options,
                df->rowCount(),
                [&](const std::string& name) { return df->getColumn(name); },
                df->getColumnNames(),
                df->getStringPool(),
                countryLookup->rowCount(),
                [&](const std::string& name) { return countryLookup->getColumn(name); },
                countryLookup->getColumnNames(),
                countryLookup->getStringPool()
            );

            size_t totalRows = result.noMatch->rowCount() + result.singleMatch->rowCount() + result.multipleMatch->rowCount();
            timer.setOutputRows(totalRows);
            std::cout << std::string(50, ' ')
                      << "noMatch: " << result.noMatch->rowCount()
                      << ", singleMatch: " << result.singleMatch->rowCount()
                      << ", multipleMatch: " << result.multipleMatch->rowCount() << std::endl;
        }

        // Flex Join - KeepLeftOnly mode (minimal memory)
        {
            BenchmarkTimer timer("flexJoin_country_KeepLeftOnly_OPT", &reporter, "Join", datasetRows);
            json joinSpec = {
                {"keys", json::array({
                    {{"left", "Country"}, {"right", "Country"}}
                })}
            };
            FlexJoinOptions options;
            options.noMatchMode = JoinMode::KeepLeftOnly;
            options.singleMatchMode = JoinMode::KeepLeftOnly;
            options.multipleMatchMode = JoinMode::KeepLeftOnly;

            auto result = DataFrameJoiner::flexJoin(
                joinSpec,
                options,
                df->rowCount(),
                [&](const std::string& name) { return df->getColumn(name); },
                df->getColumnNames(),
                df->getStringPool(),
                countryLookup->rowCount(),
                [&](const std::string& name) { return countryLookup->getColumn(name); },
                countryLookup->getColumnNames(),
                countryLookup->getStringPool()
            );

            size_t totalRows = result.noMatch->rowCount() + result.singleMatch->rowCount() + result.multipleMatch->rowCount();
            timer.setOutputRows(totalRows);
            std::cout << std::string(50, ' ')
                      << "noMatch: " << result.noMatch->rowCount()
                      << ", singleMatch: " << result.singleMatch->rowCount()
                      << ", multipleMatch: " << result.multipleMatch->rowCount() << std::endl;
        }

        // Create a larger lookup table for "many matches" scenario
        auto cityLookup = std::make_shared<DataFrame>();
        cityLookup->addStringColumn("City");
        cityLookup->addStringColumn("CityType");
        cityLookup->addIntColumn("Population");

        // Add cities with potential duplicates (cities that appear multiple times in main dataset)
        for (int i = 0; i < 100; i++) {
            cityLookup->addRow({"Lake City " + std::to_string(i), "Lake", std::to_string(10000 + i * 100)});
        }
        cityLookup->addRow({"Oslo", "Capital", "700000"});
        cityLookup->addRow({"Stockholm", "Capital", "950000"});
        cityLookup->addRow({"Copenhagen", "Capital", "600000"});

        // Flex Join with City - testing multiple match handling
        {
            BenchmarkTimer timer("flexJoin_city_KeepAll_OPT", &reporter, "Join", datasetRows);
            json joinSpec = {
                {"keys", json::array({
                    {{"left", "City"}, {"right", "City"}}
                })}
            };
            FlexJoinOptions options;
            options.noMatchMode = JoinMode::KeepHeaderOnly;
            options.singleMatchMode = JoinMode::KeepAll;
            options.multipleMatchMode = JoinMode::KeepAll;

            auto result = DataFrameJoiner::flexJoin(
                joinSpec,
                options,
                df->rowCount(),
                [&](const std::string& name) { return df->getColumn(name); },
                df->getColumnNames(),
                df->getStringPool(),
                cityLookup->rowCount(),
                [&](const std::string& name) { return cityLookup->getColumn(name); },
                cityLookup->getColumnNames(),
                cityLookup->getStringPool()
            );

            size_t totalRows = result.noMatch->rowCount() + result.singleMatch->rowCount() + result.multipleMatch->rowCount();
            timer.setOutputRows(totalRows);
            std::cout << std::string(50, ' ')
                      << "noMatch: " << result.noMatch->rowCount()
                      << ", singleMatch: " << result.singleMatch->rowCount()
                      << ", multipleMatch: " << result.multipleMatch->rowCount() << std::endl;
        }
    }

    // ========================================================================
    // Summary
    // ========================================================================
    {
        printHeader("Summary");

        std::cout << "Dataset Statistics:" << std::endl;
        std::cout << "  Total rows processed: " << df->rowCount() << std::endl;
        std::cout << "  Total columns: " << df->columnCount() << std::endl;
        std::cout << "  Unique strings in pool: " << df->getStringPool()->size() << std::endl;
        std::cout << "  String pool memory: " << (df->getStringPool()->memoryUsage() / 1024) << " KB" << std::endl;
        std::cout << "\nAll benchmarks completed successfully!" << std::endl;
    }

    printSeparator();
    std::cout << "\n";

    try {
        reporter.saveToFile("../examples/benchmark_optimized_report.json");
        reporter.printSummary();
    } catch (const std::exception& e) {
        std::cerr << "Error saving benchmark report: " << e.what() << std::endl;
    }

    return 0;
}