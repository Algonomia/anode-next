#include <catch2/catch_test_macros.hpp>
#include "postgres/PostgresPool.hpp"
#include "nodes/DynRequest.hpp"
#include "nodes/Types.hpp"
#include "dataframe/DataFrame.hpp"
#include <iostream>

using namespace postgres;
using namespace nodes;

// Note: Ces tests nécessitent une base PostgreSQL accessible
// Pour les exécuter, configurer la variable d'environnement POSTGRES_TEST_CONN
// Exemple: export POSTGRES_TEST_CONN="host=localhost port=5432 dbname=test user=postgres password=secret"

static std::string getTestConnectionString() {
    const char* connStr = std::getenv("POSTGRES_TEST_CONN");
    if (connStr) {
        return connStr;
    }
    // Connexion par défaut pour les tests locaux
    return "host=localhost port=5432 dbname=postgres user=postgres";
}

TEST_CASE("PostgresPool singleton", "[postgres]") {
    auto& pool1 = PostgresPool::instance();
    auto& pool2 = PostgresPool::instance();

    CHECK(&pool1 == &pool2);
}

TEST_CASE("PostgresPool configuration", "[postgres]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    CHECK_FALSE(pool.isConfigured());

    pool.configure("host=localhost port=5432 dbname=test");

    CHECK(pool.isConfigured());
    CHECK(pool.getConnectionString() == "host=localhost port=5432 dbname=test");

    pool.reset();
}

TEST_CASE("PostgresPool throws if not configured", "[postgres]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    CHECK_THROWS_AS(pool.executeQuery("SELECT 1"), std::runtime_error);
}

// Tests d'intégration - nécessitent une vraie connexion PostgreSQL
// Décommentez ces tests si vous avez une base de test disponible

/*
TEST_CASE("PostgresPool connection", "[postgres][integration]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    std::string connStr = getTestConnectionString();
    pool.configure(connStr);

    // Simple query test
    auto df = pool.executeQuery("SELECT 1 as value");

    REQUIRE(df != nullptr);
    CHECK(df->rowCount() == 1);
    CHECK(df->hasColumn("value"));

    pool.disconnect();
}

TEST_CASE("PostgresPool executeQuery returns DataFrame", "[postgres][integration]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    std::string connStr = getTestConnectionString();
    pool.configure(connStr);

    // Query with multiple columns and types
    auto df = pool.executeQuery(
        "SELECT 1 as int_col, 3.14 as double_col, 'hello' as text_col"
    );

    REQUIRE(df != nullptr);
    CHECK(df->rowCount() == 1);
    CHECK(df->columnCount() == 3);
    CHECK(df->hasColumn("int_col"));
    CHECK(df->hasColumn("double_col"));
    CHECK(df->hasColumn("text_col"));

    pool.disconnect();
}

TEST_CASE("PostgresPool handles multiple rows", "[postgres][integration]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    std::string connStr = getTestConnectionString();
    pool.configure(connStr);

    auto df = pool.executeQuery(
        "SELECT generate_series(1, 5) as id, 'row' || generate_series(1, 5) as name"
    );

    REQUIRE(df != nullptr);
    CHECK(df->rowCount() == 5);
    CHECK(df->hasColumn("id"));
    CHECK(df->hasColumn("name"));

    pool.disconnect();
}

TEST_CASE("PostgresPool handles empty result", "[postgres][integration]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    std::string connStr = getTestConnectionString();
    pool.configure(connStr);

    auto df = pool.executeQuery(
        "SELECT 1 as value WHERE false"
    );

    REQUIRE(df != nullptr);
    CHECK(df->rowCount() == 0);
    CHECK(df->hasColumn("value"));

    pool.disconnect();
}

TEST_CASE("PostgresPool handles SQL errors", "[postgres][integration]") {
    auto& pool = PostgresPool::instance();
    pool.reset();

    std::string connStr = getTestConnectionString();
    pool.configure(connStr);

    CHECK_THROWS_AS(
        pool.executeQuery("SELECT * FROM non_existent_table_xyz"),
        std::runtime_error
    );

    pool.disconnect();
}
*/

// ============ DynRequest Tests ============

TEST_CASE("DynRequest basic function call", "[dynrequest]") {
    DynRequest req;
    req.func("my_function");

    CHECK(req.getFunctionName() == "my_function");
    CHECK(req.getParameters().empty());

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM my_function()");
}

TEST_CASE("DynRequest scalar int param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addIntParam(42);

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(42)");
}

TEST_CASE("DynRequest scalar string param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addStringParam("hello");

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func('hello')");
}

TEST_CASE("DynRequest scalar string with apostrophe", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addStringParam("it's a test");

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func('it''s a test')");
}

TEST_CASE("DynRequest int array param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addIntArrayParam({1, 2, 3});

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(ARRAY[1,2,3]::INT[])");
}

TEST_CASE("DynRequest string array param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addStringArrayParam({"apple", "banana", "cherry"});

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(ARRAY['apple', 'banana', 'cherry']::TEXT[])");
}

TEST_CASE("DynRequest multiple params", "[dynrequest]") {
    DynRequest req;
    req.func("anode_identify_phase")
       .addIntArrayParam({10, 20, 30})
       .addStringArrayParam({"Planning", "Execution", "Review"});

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM anode_identify_phase(ARRAY[10,20,30]::INT[], ARRAY['Planning', 'Execution', 'Review']::TEXT[])");
}

TEST_CASE("DynRequest bool param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addBoolParam(true)
       .addBoolParam(false);

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(true, false)");
}

TEST_CASE("DynRequest null param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addNullParam();

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(NULL)");
}

TEST_CASE("DynRequest double array param", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addDoubleArrayParam({1.5, 2.5, 3.5});

    std::string sql = req.buildSQL();
    // Check that it contains the expected pattern (precision may vary)
    CHECK(sql.find("ARRAY[") != std::string::npos);
    CHECK(sql.find("::DOUBLE PRECISION[]") != std::string::npos);
}

TEST_CASE("DynRequest int array from scalar workload (broadcast)", "[dynrequest]") {
    // Create a simple CSV with 3 rows
    auto df = std::make_shared<dataframe::DataFrame>();
    df->addIntColumn("id");
    df->addRow({"1"});
    df->addRow({"2"});
    df->addRow({"3"});

    // Create a scalar workload
    Workload scalarWL(int64_t(42));

    DynRequest req;
    req.func("test_func")
       .addIntArrayFromWorkload(scalarWL, df);

    std::string sql = req.buildSQL();
    // The scalar 42 should be broadcast to all 3 rows
    CHECK(sql == "SELECT * FROM test_func(ARRAY[42,42,42]::INT[])");
}

TEST_CASE("DynRequest string array from field workload", "[dynrequest]") {
    // Create a CSV with a string column
    auto df = std::make_shared<dataframe::DataFrame>();
    df->addStringColumn("name");
    df->addRow({"Alice"});
    df->addRow({"Bob"});
    df->addRow({"Charlie"});

    // Create a field workload pointing to "name" column
    Workload fieldWL("name", NodeType::Field);

    DynRequest req;
    req.func("test_func")
       .addStringArrayFromWorkload(fieldWL, df);

    std::string sql = req.buildSQL();
    CHECK(sql == "SELECT * FROM test_func(ARRAY['Alice', 'Bob', 'Charlie']::TEXT[])");
}

TEST_CASE("DynRequest reset", "[dynrequest]") {
    DynRequest req;
    req.func("test_func")
       .addIntParam(42);

    CHECK(req.getFunctionName() == "test_func");
    CHECK(req.getParameters().size() == 1);

    req.reset();

    CHECK(req.getFunctionName().empty());
    CHECK(req.getParameters().empty());
}

TEST_CASE("DynRequest throws if no function name", "[dynrequest]") {
    DynRequest req;
    req.addIntParam(42);

    CHECK_THROWS_AS(req.buildSQL(), std::runtime_error);
}
