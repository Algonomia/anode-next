#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameIO.hpp"
#include <iostream>

using namespace dataframe;

int main() {
    std::cout << "=== DataFrame Example ===" << std::endl;

    // Créer un DataFrame
    auto df = std::make_shared<DataFrame>();

    // Ajouter des colonnes typées
    df->addStringColumn("name");
    df->addIntColumn("age");
    df->addDoubleColumn("salary");
    df->addStringColumn("city");

    // Remplir les données
    df->addRow({"Alice", "30", "50000.0", "Paris"});
    df->addRow({"Bob", "25", "45000.0", "Lyon"});
    df->addRow({"Charlie", "35", "60000.0", "Paris"});
    df->addRow({"Diana", "28", "52000.0", "Marseille"});
    df->addRow({"Eve", "32", "55000.0", "Lyon"});

    std::cout << "\n=== Original DataFrame ===" << std::endl;
    std::cout << df->toString() << std::endl;

    // ===== FILTER =====
    std::cout << "\n=== Filter: city == 'Paris' ===" << std::endl;
    auto filterContract = R"([{"column": "city", "operator": "==", "value": "Paris"}])"_json;
    auto filtered = df->filter(filterContract);
    std::cout << filtered->toString() << std::endl;

    // ===== ORDER BY =====
    std::cout << "\n=== Order By: age DESC ===" << std::endl;
    auto orderContract = R"([{"column": "age", "order": "desc"}])"_json;
    auto ordered = df->orderBy(orderContract);
    std::cout << ordered->toString() << std::endl;

    // ===== GROUP BY =====
    std::cout << "\n=== Group By: city with count and avg salary ===" << std::endl;
    auto groupContract = R"({
        "groupBy": ["city"],
        "aggregations": [
            {"column": "name", "function": "count", "alias": "count"},
            {"column": "salary", "function": "avg", "alias": "avg_salary"}
        ]
    })"_json;
    auto grouped = df->groupBy(groupContract);
    std::cout << grouped->toString() << std::endl;

    // ===== SELECT =====
    std::cout << "\n=== Select: name, salary ===" << std::endl;
    auto selected = df->select({"name", "salary"});
    std::cout << selected->toString() << std::endl;

    // ===== CHAINED OPERATIONS =====
    std::cout << "\n=== Chained: Filter age > 26, Order by salary DESC ===" << std::endl;
    auto filterAge = R"([{"column": "age", "operator": ">", "value": 26}])"_json;
    auto orderSalary = R"([{"column": "salary", "order": "desc"}])"_json;
    auto chained = df->filter(filterAge)->orderBy(orderSalary);
    std::cout << chained->toString() << std::endl;

    std::cout << "\n=== Example Complete ===" << std::endl;

    return 0;
}
