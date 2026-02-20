#include <catch2/catch_test_macros.hpp>
#include "dataframe/StringPool.hpp"

using namespace dataframe;

TEST_CASE("StringPool intern - new string", "[StringPool]") {
    StringPool pool;

    auto id1 = pool.intern("hello");
    auto id2 = pool.intern("world");

    REQUIRE(id1 == 0);
    REQUIRE(id2 == 1);
    REQUIRE(id1 != id2);
}

TEST_CASE("StringPool intern - duplicate returns same ID", "[StringPool]") {
    StringPool pool;

    auto id1 = pool.intern("hello");
    auto id2 = pool.intern("hello");

    REQUIRE(id1 == id2);
    REQUIRE(pool.size() == 1);
}

TEST_CASE("StringPool intern - different strings get different IDs", "[StringPool]") {
    StringPool pool;

    auto id1 = pool.intern("apple");
    auto id2 = pool.intern("banana");
    auto id3 = pool.intern("cherry");

    REQUIRE(id1 != id2);
    REQUIRE(id2 != id3);
    REQUIRE(id1 != id3);
    REQUIRE(pool.size() == 3);
}

TEST_CASE("StringPool getString - valid ID", "[StringPool]") {
    StringPool pool;

    auto id = pool.intern("test_string");

    REQUIRE(pool.getString(id) == "test_string");
}

TEST_CASE("StringPool getString - invalid ID returns empty", "[StringPool]") {
    StringPool pool;

    REQUIRE(pool.getString(999).empty());
    REQUIRE(pool.getString(StringPool::INVALID_ID).empty());
}

TEST_CASE("StringPool isValid", "[StringPool]") {
    StringPool pool;

    REQUIRE_FALSE(pool.isValid(0));

    auto id = pool.intern("test");

    REQUIRE(pool.isValid(id));
    REQUIRE_FALSE(pool.isValid(999));
    REQUIRE_FALSE(pool.isValid(StringPool::INVALID_ID));
}

TEST_CASE("StringPool size", "[StringPool]") {
    StringPool pool;

    REQUIRE(pool.size() == 0);

    pool.intern("one");
    REQUIRE(pool.size() == 1);

    pool.intern("two");
    REQUIRE(pool.size() == 2);

    pool.intern("one");  // Duplicate
    REQUIRE(pool.size() == 2);

    pool.intern("three");
    REQUIRE(pool.size() == 3);
}

TEST_CASE("StringPool reserve", "[StringPool]") {
    StringPool pool;

    REQUIRE_NOTHROW(pool.reserve(10000));

    // After reserve, adding many strings should not cause issues
    for (int i = 0; i < 5000; ++i) {
        pool.intern("string_" + std::to_string(i));
    }

    REQUIRE(pool.size() == 5000);
}

TEST_CASE("StringPool clear", "[StringPool]") {
    StringPool pool;

    pool.intern("one");
    pool.intern("two");
    pool.intern("three");

    REQUIRE(pool.size() == 3);

    pool.clear();

    REQUIRE(pool.size() == 0);
    REQUIRE(pool.getString(0).empty());
    REQUIRE_FALSE(pool.isValid(0));
}

TEST_CASE("StringPool memoryUsage", "[StringPool]") {
    StringPool pool;

    size_t initialUsage = pool.memoryUsage();

    pool.intern("a_reasonably_long_string_to_measure_memory");
    pool.intern("another_string_with_some_content");

    size_t afterUsage = pool.memoryUsage();

    REQUIRE(afterUsage > initialUsage);
}

TEST_CASE("StringPool empty string", "[StringPool]") {
    StringPool pool;

    auto id = pool.intern("");

    REQUIRE(pool.isValid(id));
    REQUIRE(pool.getString(id).empty());
    REQUIRE(pool.size() == 1);
}

TEST_CASE("StringPool special characters", "[StringPool]") {
    StringPool pool;

    auto id1 = pool.intern("hello\nworld");
    auto id2 = pool.intern("tab\there");
    auto id3 = pool.intern("unicode: \xC3\xA9\xC3\xA0\xC3\xBC");

    REQUIRE(pool.getString(id1) == "hello\nworld");
    REQUIRE(pool.getString(id2) == "tab\there");
    REQUIRE(pool.getString(id3) == "unicode: \xC3\xA9\xC3\xA0\xC3\xBC");
}
