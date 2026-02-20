#include <catch2/catch_test_macros.hpp>
#include "dataframe/DataFrame.hpp"
#include "dataframe/DataFrameJoiner.hpp"

using namespace dataframe;

// =============================================================================
// Helper functions
// =============================================================================

static std::shared_ptr<DataFrame> createEmployeesDF() {
    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("id");
    df->addStringColumn("name");
    df->addIntColumn("dept_id");

    df->addRow({"1", "Alice", "10"});
    df->addRow({"2", "Bob", "20"});
    df->addRow({"3", "Carol", "10"});
    df->addRow({"4", "Dave", "30"});  // No matching dept

    return df;
}

static std::shared_ptr<DataFrame> createDepartmentsDF() {
    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("dept_id");
    df->addStringColumn("dept_name");

    df->addRow({"10", "Engineering"});
    df->addRow({"20", "Sales"});
    // Note: dept 30 doesn't exist

    return df;
}

// =============================================================================
// Basic Inner Join Tests
// =============================================================================

TEST_CASE("InnerJoin basic - single key", "[DataFrameJoiner]") {
    auto employees = createEmployeesDF();
    auto departments = createDepartmentsDF();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "dept_id"}, {"right", "dept_id"}}
        })}
    };

    auto result = employees->innerJoin(departments, joinSpec);

    REQUIRE(result->rowCount() == 3);  // Dave excluded (dept 30 not found)
    REQUIRE(result->columnCount() == 4);  // dept_id, id, name, dept_name

    REQUIRE(result->hasColumn("dept_id"));
    REQUIRE(result->hasColumn("id"));
    REQUIRE(result->hasColumn("name"));
    REQUIRE(result->hasColumn("dept_name"));
}

TEST_CASE("InnerJoin with different key names", "[DataFrameJoiner]") {
    auto employees = createEmployeesDF();

    auto depts = std::make_shared<DataFrame>();
    depts->addIntColumn("d_id");  // Different name
    depts->addStringColumn("d_name");
    depts->addRow({"10", "Eng"});
    depts->addRow({"20", "Sales"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "dept_id"}, {"right", "d_id"}}
        })}
    };

    auto result = employees->innerJoin(depts, joinSpec);

    REQUIRE(result->rowCount() == 3);
    REQUIRE(result->hasColumn("dept_id"));  // Uses left key name
    REQUIRE(result->hasColumn("d_name"));
    REQUIRE_FALSE(result->hasColumn("d_id"));  // Right key not duplicated
}

// =============================================================================
// Composite Key Tests
// =============================================================================

TEST_CASE("InnerJoin composite keys", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("region");
    left->addIntColumn("product");
    left->addIntColumn("sales");
    left->addRow({"1", "100", "50"});
    left->addRow({"1", "200", "60"});
    left->addRow({"2", "100", "70"});
    left->addRow({"2", "200", "80"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("reg");
    right->addIntColumn("prod");
    right->addStringColumn("description");
    right->addRow({"1", "100", "Widget A"});
    right->addRow({"1", "200", "Widget B"});
    right->addRow({"2", "200", "Widget C"});  // Only this matches (2, 200)

    json joinSpec = {
        {"keys", json::array({
            {{"left", "region"}, {"right", "reg"}},
            {{"left", "product"}, {"right", "prod"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 3);  // (1,100), (1,200), (2,200)
    REQUIRE(result->hasColumn("region"));
    REQUIRE(result->hasColumn("product"));
    REQUIRE(result->hasColumn("sales"));
    REQUIRE(result->hasColumn("description"));
}

// =============================================================================
// Column Name Collision Tests
// =============================================================================

TEST_CASE("InnerJoin column name collision", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addStringColumn("name");
    left->addRow({"1", "Alice"});
    left->addRow({"2", "Bob"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("id");
    right->addStringColumn("name");  // Same column name as left!
    right->addRow({"1", "Alpha"});
    right->addRow({"2", "Beta"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 2);
    REQUIRE(result->hasColumn("id"));
    REQUIRE(result->hasColumn("name"));
    REQUIRE(result->hasColumn("name_right"));  // Collision resolved

    auto nameCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("name"));
    auto nameRightCol = std::dynamic_pointer_cast<StringColumn>(result->getColumn("name_right"));

    REQUIRE(nameCol->at(0) == "Alice");
    REQUIRE(nameRightCol->at(0) == "Alpha");
}

// =============================================================================
// String Key Tests
// =============================================================================

TEST_CASE("InnerJoin with string keys", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addStringColumn("code");
    left->addIntColumn("value");
    left->addRow({"AAA", "10"});
    left->addRow({"BBB", "20"});
    left->addRow({"CCC", "30"});

    auto right = std::make_shared<DataFrame>();
    right->addStringColumn("product_code");
    right->addStringColumn("description");
    right->addRow({"AAA", "Product A"});
    right->addRow({"BBB", "Product B"});
    right->addRow({"DDD", "Product D"});  // No match

    json joinSpec = {
        {"keys", json::array({
            {{"left", "code"}, {"right", "product_code"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 2);  // AAA, BBB
    REQUIRE(result->hasColumn("code"));
    REQUIRE(result->hasColumn("value"));
    REQUIRE(result->hasColumn("description"));
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("InnerJoin empty result", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addRow({"1"});
    left->addRow({"2"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("id");
    right->addRow({"3"});
    right->addRow({"4"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 0);
    REQUIRE(result->columnCount() == 1);  // Only id column
}

TEST_CASE("InnerJoin one-to-many", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addStringColumn("name");
    left->addRow({"1", "Parent"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("parent_id");
    right->addStringColumn("child_name");
    right->addRow({"1", "Child A"});
    right->addRow({"1", "Child B"});
    right->addRow({"1", "Child C"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "parent_id"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 3);  // One parent matches 3 children
}

TEST_CASE("InnerJoin many-to-many", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("group_id");
    left->addStringColumn("left_val");
    left->addRow({"1", "L1"});
    left->addRow({"1", "L2"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("group_id");
    right->addStringColumn("right_val");
    right->addRow({"1", "R1"});
    right->addRow({"1", "R2"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "group_id"}, {"right", "group_id"}}
        })}
    };

    auto result = left->innerJoin(right, joinSpec);

    REQUIRE(result->rowCount() == 4);  // 2 x 2 = 4 (cartesian for matching key)
}

// =============================================================================
// Error Handling Tests
// =============================================================================

TEST_CASE("InnerJoin missing keys throws", "[DataFrameJoiner]") {
    auto left = createEmployeesDF();
    auto right = createDepartmentsDF();

    json badSpec = {{"keys", json::array()}};

    REQUIRE_THROWS_AS(left->innerJoin(right, badSpec), std::invalid_argument);
}

TEST_CASE("InnerJoin missing column throws", "[DataFrameJoiner]") {
    auto left = createEmployeesDF();
    auto right = createDepartmentsDF();

    json badSpec = {
        {"keys", json::array({
            {{"left", "nonexistent"}, {"right", "dept_id"}}
        })}
    };

    REQUIRE_THROWS_AS(left->innerJoin(right, badSpec), std::out_of_range);
}

TEST_CASE("InnerJoin type mismatch throws", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addRow({"1"});

    auto right = std::make_shared<DataFrame>();
    right->addStringColumn("id");  // String vs Int
    right->addRow({"1"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    REQUIRE_THROWS_AS(left->innerJoin(right, joinSpec), std::invalid_argument);
}

// =============================================================================
// Shorthand Key Format Tests
// =============================================================================

TEST_CASE("InnerJoin shorthand key format", "[DataFrameJoiner]") {
    auto employees = createEmployeesDF();
    auto departments = createDepartmentsDF();

    // Shorthand: just the column name when same on both sides
    json joinSpec = {
        {"keys", json::array({"dept_id"})}
    };

    auto result = employees->innerJoin(departments, joinSpec);

    REQUIRE(result->rowCount() == 3);
    REQUIRE(result->hasColumn("dept_id"));
}

// =============================================================================
// Flex Join Tests
// =============================================================================

static std::shared_ptr<DataFrame> createLeftForFlexJoin() {
    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("id");
    df->addStringColumn("name");
    df->addRow({"1", "Alice"});   // Single match
    df->addRow({"2", "Bob"});     // Multiple matches
    df->addRow({"3", "Carol"});   // No match
    df->addRow({"4", "Dave"});    // Single match
    return df;
}

static std::shared_ptr<DataFrame> createRightForFlexJoin() {
    auto df = std::make_shared<DataFrame>();
    df->addIntColumn("left_id");
    df->addStringColumn("value");
    df->addRow({"1", "Val1"});     // Match for Alice
    df->addRow({"2", "Val2a"});    // First match for Bob
    df->addRow({"2", "Val2b"});    // Second match for Bob
    df->addRow({"4", "Val4"});     // Match for Dave
    // No match for id=3 (Carol)
    return df;
}

TEST_CASE("FlexJoin basic - separates no/single/multiple matches", "[DataFrameJoiner]") {
    auto left = createLeftForFlexJoin();
    auto right = createRightForFlexJoin();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "left_id"}}
        })}
    };

    FlexJoinOptions options;
    options.noMatchMode = JoinMode::KeepHeaderOnly;
    options.singleMatchMode = JoinMode::KeepAll;
    options.multipleMatchMode = JoinMode::KeepAll;

    auto result = DataFrameJoiner::flexJoin(
        joinSpec,
        options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    // No match: Carol (id=3)
    REQUIRE(result.noMatch->rowCount() == 1);
    auto noMatchIdCol = std::dynamic_pointer_cast<IntColumn>(result.noMatch->getColumn("id"));
    REQUIRE(noMatchIdCol->at(0) == 3);

    // Single match: Alice (id=1), Dave (id=4)
    REQUIRE(result.singleMatch->rowCount() == 2);

    // Multiple match: Bob (id=2) with 2 matches
    REQUIRE(result.multipleMatch->rowCount() == 2);
}

TEST_CASE("FlexJoin noMatch modes", "[DataFrameJoiner]") {
    auto left = createLeftForFlexJoin();
    auto right = createRightForFlexJoin();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "left_id"}}
        })}
    };

    SECTION("KeepHeaderOnly - keeps right columns with empty values") {
        FlexJoinOptions options;
        options.noMatchMode = JoinMode::KeepHeaderOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.noMatch->rowCount() == 1);
        REQUIRE(result.noMatch->hasColumn("value"));  // Right column present
        auto valueCol = std::dynamic_pointer_cast<StringColumn>(result.noMatch->getColumn("value"));
        REQUIRE(valueCol->at(0) == "");  // Empty value
    }

    SECTION("KeepLeftOnly - no right columns") {
        FlexJoinOptions options;
        options.noMatchMode = JoinMode::KeepLeftOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.noMatch->rowCount() == 1);
        REQUIRE_FALSE(result.noMatch->hasColumn("value"));  // No right column
        REQUIRE(result.noMatch->hasColumn("id"));
        REQUIRE(result.noMatch->hasColumn("name"));
    }
}

TEST_CASE("FlexJoin singleMatch modes", "[DataFrameJoiner]") {
    auto left = createLeftForFlexJoin();
    auto right = createRightForFlexJoin();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "left_id"}}
        })}
    };

    SECTION("KeepAll - includes right data") {
        FlexJoinOptions options;
        options.singleMatchMode = JoinMode::KeepAll;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.singleMatch->rowCount() == 2);
        REQUIRE(result.singleMatch->hasColumn("value"));
        auto valueCol = std::dynamic_pointer_cast<StringColumn>(result.singleMatch->getColumn("value"));
        // Alice (id=1) matches Val1, Dave (id=4) matches Val4
        bool hasVal1 = false, hasVal4 = false;
        for (size_t i = 0; i < result.singleMatch->rowCount(); i++) {
            if (valueCol->at(i) == "Val1") hasVal1 = true;
            if (valueCol->at(i) == "Val4") hasVal4 = true;
        }
        REQUIRE(hasVal1);
        REQUIRE(hasVal4);
    }

    SECTION("KeepHeaderOnly - right columns with empty values") {
        FlexJoinOptions options;
        options.singleMatchMode = JoinMode::KeepHeaderOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.singleMatch->rowCount() == 2);
        auto valueCol = std::dynamic_pointer_cast<StringColumn>(result.singleMatch->getColumn("value"));
        REQUIRE(valueCol->at(0) == "");
        REQUIRE(valueCol->at(1) == "");
    }

    SECTION("KeepLeftOnly - no right columns") {
        FlexJoinOptions options;
        options.singleMatchMode = JoinMode::KeepLeftOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.singleMatch->rowCount() == 2);
        REQUIRE_FALSE(result.singleMatch->hasColumn("value"));
    }
}

TEST_CASE("FlexJoin multipleMatch modes", "[DataFrameJoiner]") {
    auto left = createLeftForFlexJoin();
    auto right = createRightForFlexJoin();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "left_id"}}
        })}
    };

    SECTION("KeepAll - one row per match") {
        FlexJoinOptions options;
        options.multipleMatchMode = JoinMode::KeepAll;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        // Bob (id=2) has 2 matches -> 2 rows
        REQUIRE(result.multipleMatch->rowCount() == 2);
        auto valueCol = std::dynamic_pointer_cast<StringColumn>(result.multipleMatch->getColumn("value"));
        bool hasVal2a = false, hasVal2b = false;
        for (size_t i = 0; i < result.multipleMatch->rowCount(); i++) {
            if (valueCol->at(i) == "Val2a") hasVal2a = true;
            if (valueCol->at(i) == "Val2b") hasVal2b = true;
        }
        REQUIRE(hasVal2a);
        REQUIRE(hasVal2b);
    }

    SECTION("KeepHeaderOnly - one row with empty right columns") {
        FlexJoinOptions options;
        options.multipleMatchMode = JoinMode::KeepHeaderOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        // Only 1 row for Bob (not expanded)
        REQUIRE(result.multipleMatch->rowCount() == 1);
        REQUIRE(result.multipleMatch->hasColumn("value"));
        auto valueCol = std::dynamic_pointer_cast<StringColumn>(result.multipleMatch->getColumn("value"));
        REQUIRE(valueCol->at(0) == "");
    }

    SECTION("KeepLeftOnly - one row without right columns") {
        FlexJoinOptions options;
        options.multipleMatchMode = JoinMode::KeepLeftOnly;

        auto result = DataFrameJoiner::flexJoin(
            joinSpec, options,
            left->rowCount(),
            [&](const std::string& name) { return left->getColumn(name); },
            left->getColumnNames(),
            left->getStringPool(),
            right->rowCount(),
            [&](const std::string& name) { return right->getColumn(name); },
            right->getColumnNames(),
            right->getStringPool()
        );

        REQUIRE(result.multipleMatch->rowCount() == 1);
        REQUIRE_FALSE(result.multipleMatch->hasColumn("value"));
    }
}

TEST_CASE("FlexJoin with composite keys", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("region");
    left->addIntColumn("product");
    left->addStringColumn("name");
    left->addRow({"1", "100", "R1P100"});
    left->addRow({"1", "200", "R1P200"});
    left->addRow({"2", "100", "R2P100"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("reg");
    right->addIntColumn("prod");
    right->addStringColumn("desc");
    right->addRow({"1", "100", "Match1"});
    right->addRow({"1", "200", "Match2"});
    // No match for (2, 100)

    json joinSpec = {
        {"keys", json::array({
            {{"left", "region"}, {"right", "reg"}},
            {{"left", "product"}, {"right", "prod"}}
        })}
    };

    FlexJoinOptions options;
    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    REQUIRE(result.singleMatch->rowCount() == 2);  // R1P100, R1P200
    REQUIRE(result.noMatch->rowCount() == 1);      // R2P100
    REQUIRE(result.multipleMatch->rowCount() == 0);
}

TEST_CASE("FlexJoin empty results", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addRow({"1"});
    left->addRow({"2"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("id");
    right->addStringColumn("value");
    right->addRow({"3", "Val3"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    FlexJoinOptions options;
    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    REQUIRE(result.noMatch->rowCount() == 2);      // All left rows are no-match
    REQUIRE(result.singleMatch->rowCount() == 0);
    REQUIRE(result.multipleMatch->rowCount() == 0);
}

TEST_CASE("FlexJoin all single matches", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addRow({"1"});
    left->addRow({"2"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("id");
    right->addStringColumn("value");
    right->addRow({"1", "Val1"});
    right->addRow({"2", "Val2"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    FlexJoinOptions options;
    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    REQUIRE(result.noMatch->rowCount() == 0);
    REQUIRE(result.singleMatch->rowCount() == 2);  // All single matches
    REQUIRE(result.multipleMatch->rowCount() == 0);
}

TEST_CASE("FlexJoin all modes KeepLeftOnly", "[DataFrameJoiner]") {
    auto left = createLeftForFlexJoin();
    auto right = createRightForFlexJoin();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "left_id"}}
        })}
    };

    FlexJoinOptions options;
    options.noMatchMode = JoinMode::KeepLeftOnly;
    options.singleMatchMode = JoinMode::KeepLeftOnly;
    options.multipleMatchMode = JoinMode::KeepLeftOnly;

    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    // Same distribution as other tests:
    // - Alice (id=1): single match
    // - Bob (id=2): multiple matches (2)
    // - Carol (id=3): no match
    // - Dave (id=4): single match
    REQUIRE(result.noMatch->rowCount() == 1);       // Carol
    REQUIRE(result.singleMatch->rowCount() == 2);   // Alice, Dave
    REQUIRE(result.multipleMatch->rowCount() == 1); // Bob (1 row, not expanded)

    // Verify no right columns in any output
    REQUIRE_FALSE(result.noMatch->hasColumn("value"));
    REQUIRE_FALSE(result.singleMatch->hasColumn("value"));
    REQUIRE_FALSE(result.multipleMatch->hasColumn("value"));
}

TEST_CASE("FlexJoin with STRING keys - all modes KeepLeftOnly", "[DataFrameJoiner]") {
    // Simule le scénario du benchmark: clés STRING (Country)
    auto left = std::make_shared<DataFrame>();
    left->addStringColumn("country");
    left->addStringColumn("name");
    left->addRow({"Norway", "Alice"});
    left->addRow({"Sweden", "Bob"});
    left->addRow({"Unknown", "Carol"});  // No match
    left->addRow({"Norway", "Dave"});    // Same country as Alice

    auto right = std::make_shared<DataFrame>();
    right->addStringColumn("country");
    right->addStringColumn("region");
    right->addRow({"Norway", "Scandinavia"});
    right->addRow({"Sweden", "Scandinavia"});
    // No "Unknown" country

    json joinSpec = {
        {"keys", json::array({
            {{"left", "country"}, {"right", "country"}}
        })}
    };

    FlexJoinOptions options;
    options.noMatchMode = JoinMode::KeepLeftOnly;
    options.singleMatchMode = JoinMode::KeepLeftOnly;
    options.multipleMatchMode = JoinMode::KeepLeftOnly;

    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    // - Norway (Alice, Dave): 2 rows -> single match each (country appears once in right)
    // - Sweden (Bob): 1 row -> single match
    // - Unknown (Carol): 1 row -> no match
    REQUIRE(result.noMatch->rowCount() == 1);       // Carol (Unknown)
    REQUIRE(result.singleMatch->rowCount() == 3);   // Alice, Bob, Dave
    REQUIRE(result.multipleMatch->rowCount() == 0);

    // Verify no right columns
    REQUIRE_FALSE(result.noMatch->hasColumn("region"));
    REQUIRE_FALSE(result.singleMatch->hasColumn("region"));
}

TEST_CASE("FlexJoin column name collision", "[DataFrameJoiner]") {
    auto left = std::make_shared<DataFrame>();
    left->addIntColumn("id");
    left->addStringColumn("value");  // Same name as right
    left->addRow({"1", "LeftVal"});

    auto right = std::make_shared<DataFrame>();
    right->addIntColumn("id");
    right->addStringColumn("value");  // Collision!
    right->addRow({"1", "RightVal"});

    json joinSpec = {
        {"keys", json::array({
            {{"left", "id"}, {"right", "id"}}
        })}
    };

    FlexJoinOptions options;
    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        left->rowCount(),
        [&](const std::string& name) { return left->getColumn(name); },
        left->getColumnNames(),
        left->getStringPool(),
        right->rowCount(),
        [&](const std::string& name) { return right->getColumn(name); },
        right->getColumnNames(),
        right->getStringPool()
    );

    REQUIRE(result.singleMatch->rowCount() == 1);
    REQUIRE(result.singleMatch->hasColumn("value"));
    REQUIRE(result.singleMatch->hasColumn("value_right"));

    auto leftVal = std::dynamic_pointer_cast<StringColumn>(result.singleMatch->getColumn("value"));
    auto rightVal = std::dynamic_pointer_cast<StringColumn>(result.singleMatch->getColumn("value_right"));
    REQUIRE(leftVal->at(0) == "LeftVal");
    REQUIRE(rightVal->at(0) == "RightVal");
}

TEST_CASE("FlexJoin Skip mode - only single matches", "[DataFrameJoiner]") {
    auto employees = createEmployeesDF();
    auto departments = createDepartmentsDF();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "dept_id"}, {"right", "dept_id"}}
        })}
    };

    FlexJoinOptions options;
    options.noMatchMode = JoinMode::Skip;       // Skip noMatch
    options.singleMatchMode = JoinMode::KeepAll;
    options.multipleMatchMode = JoinMode::Skip; // Skip multipleMatch

    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        employees->rowCount(),
        [&](const std::string& name) { return employees->getColumn(name); },
        employees->getColumnNames(),
        employees->getStringPool(),
        departments->rowCount(),
        [&](const std::string& name) { return departments->getColumn(name); },
        departments->getColumnNames(),
        departments->getStringPool()
    );

    // noMatch should be empty (Skip mode)
    REQUIRE(result.noMatch->rowCount() == 0);
    REQUIRE(result.noMatch->columnCount() == 0);  // No columns either

    // singleMatch should have the matching rows
    REQUIRE(result.singleMatch->rowCount() == 3);  // Alice, Bob, Carol
    REQUIRE(result.singleMatch->hasColumn("id"));
    REQUIRE(result.singleMatch->hasColumn("dept_name"));

    // multipleMatch should be empty (Skip mode)
    REQUIRE(result.multipleMatch->rowCount() == 0);
    REQUIRE(result.multipleMatch->columnCount() == 0);
}

TEST_CASE("FlexJoin Skip mode - all skipped", "[DataFrameJoiner]") {
    auto employees = createEmployeesDF();
    auto departments = createDepartmentsDF();

    json joinSpec = {
        {"keys", json::array({
            {{"left", "dept_id"}, {"right", "dept_id"}}
        })}
    };

    FlexJoinOptions options;
    options.noMatchMode = JoinMode::Skip;
    options.singleMatchMode = JoinMode::Skip;
    options.multipleMatchMode = JoinMode::Skip;

    auto result = DataFrameJoiner::flexJoin(
        joinSpec, options,
        employees->rowCount(),
        [&](const std::string& name) { return employees->getColumn(name); },
        employees->getColumnNames(),
        employees->getStringPool(),
        departments->rowCount(),
        [&](const std::string& name) { return departments->getColumn(name); },
        departments->getColumnNames(),
        departments->getStringPool()
    );

    // All outputs should be empty
    REQUIRE(result.noMatch->rowCount() == 0);
    REQUIRE(result.singleMatch->rowCount() == 0);
    REQUIRE(result.multipleMatch->rowCount() == 0);
}
