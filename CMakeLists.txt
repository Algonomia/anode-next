cmake_minimum_required(VERSION 3.14)
project(anodeServer VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options de compilation
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -O2")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
#set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/src)

# Find nlohmann/json
find_package(nlohmann_json 3.2.0 QUIET)

if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann/json not found, fetching from GitHub...")
    include(FetchContent)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# hosseinmoein/DataFrame library
message(STATUS "Fetching hosseinmoein/DataFrame from GitHub...")
include(FetchContent)
FetchContent_Declare(
    hmdf
    GIT_REPOSITORY https://github.com/hosseinmoein/DataFrame.git
    GIT_TAG master
)
set(HMDF_TESTING OFF CACHE BOOL "" FORCE)
set(HMDF_EXAMPLES OFF CACHE BOOL "" FORCE)
set(HMDF_BENCHMARKS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(hmdf)

# Boost (pour le serveur HTTP)
find_package(Boost 1.70 REQUIRED COMPONENTS system)

# SQLite3 (pour le storage des graphes)
find_package(SQLite3 QUIET)

# libpqxx (pour PostgreSQL)
find_package(PkgConfig REQUIRED)
pkg_check_modules(PQXX REQUIRED libpqxx)

# libpq (raw C API, needed by some plugins)
pkg_check_modules(LIBPQ QUIET libpq)

if(NOT SQLite3_FOUND)
    message(STATUS "SQLite3 not found, downloading amalgamation...")
    include(FetchContent)
    FetchContent_Declare(
        sqlite3_amalgamation
        URL https://www.sqlite.org/2024/sqlite-amalgamation-3460000.zip
        URL_HASH SHA256=712a7d09d2a22652fb06a49af516e051979a3984adb067da86760e60ed51a7f5
    )
    FetchContent_MakeAvailable(sqlite3_amalgamation)

    # Build SQLite as a static library
    add_library(sqlite3_lib STATIC
        ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c
    )
    target_include_directories(sqlite3_lib PUBLIC
        ${sqlite3_amalgamation_SOURCE_DIR}
    )
    target_compile_definitions(sqlite3_lib PRIVATE
        SQLITE_ENABLE_FTS5
        SQLITE_ENABLE_JSON1
    )

    # Create alias to match find_package target name
    add_library(SQLite::SQLite3 ALIAS sqlite3_lib)
endif()

# DataFrame library
add_library(dataframe
    src/dataframe/DataFrame.cpp
    src/dataframe/DataFrameFilter.cpp
    src/dataframe/DataFrameSorter.cpp
    src/dataframe/DataFrameAggregator.cpp
    src/dataframe/DataFrameJoiner.cpp
    src/dataframe/DataFrameSerializer.cpp
    src/dataframe/DataFrameIO.cpp
)

# Benchmark library
add_library(benchmark_lib
    src/benchmark/BenchmarkReporter.cpp
)

target_include_directories(dataframe PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(dataframe PUBLIC
    nlohmann_json::nlohmann_json
)

target_include_directories(benchmark_lib PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(benchmark_lib PUBLIC
    nlohmann_json::nlohmann_json
)

# PostgreSQL library (must be before nodes)
add_library(postgres
    src/postgres/PostgresPool.cpp
)

target_include_directories(postgres PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PQXX_INCLUDE_DIRS}
    ${LIBPQ_INCLUDE_DIRS}
)

target_link_libraries(postgres PUBLIC
    dataframe
    ${PQXX_LIBRARIES}
    ${LIBPQ_LIBRARIES}
)

target_compile_options(postgres PUBLIC ${PQXX_CFLAGS_OTHER} ${LIBPQ_CFLAGS_OTHER})

# Nodes library (core framework only)
add_library(nodes
    src/nodes/Types.cpp
    src/nodes/NodeContext.cpp
    src/nodes/NodeDefinition.cpp
    src/nodes/NodeBuilder.cpp
    src/nodes/NodeRegistry.cpp
    src/nodes/NodeExecutor.cpp
    src/nodes/NodeGraphSerializer.cpp
    src/nodes/DateTimeUtil.cpp
    src/nodes/DynRequest.cpp
    src/nodes/EquationParser.cpp
    src/nodes/LabelRegistry.cpp
)

target_include_directories(nodes PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${PQXX_INCLUDE_DIRS}
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(nodes PUBLIC
    dataframe
    postgres
)

# ============================================================
# Auto-discover node plugins (src/nodes/nodes/*/CMakeLists.txt)
# ============================================================
set(PLUGIN_INCLUDES "")
set(PLUGIN_SERVER_INCLUDES "")
set(PLUGIN_REGISTER_CALLS "")
set(PLUGIN_INIT_CALLS "")
set(PLUGIN_START_LISTENER_CALLS "")
set(PLUGIN_SHUTDOWN_CALLS "")
set(PLUGIN_STOP_LISTENER_CALLS "")

file(GLOB _plugin_cmakes "${PROJECT_SOURCE_DIR}/src/nodes/nodes/*/CMakeLists.txt")

foreach(_pcmake ${_plugin_cmakes})
    get_filename_component(_pdir "${_pcmake}" DIRECTORY)
    get_filename_component(_pname "${_pdir}" NAME)

    add_subdirectory("${_pdir}")
    target_link_libraries(nodes PUBLIC "${_pname}_nodes")

    # Plugins with register.hpp contribute to plugin init
    if(EXISTS "${_pdir}/register.hpp")
        message(STATUS "Node plugin: ${_pname} (with init)")
        string(APPEND PLUGIN_INCLUDES
            "#include \"nodes/nodes/${_pname}/register.hpp\"\n")
        string(APPEND PLUGIN_REGISTER_CALLS
            "    ${_pname}::registerNodes();\n")
        string(APPEND PLUGIN_INIT_CALLS
            "    ${_pname}::init(ctx);\n")
        string(APPEND PLUGIN_SHUTDOWN_CALLS
            "    ${_pname}::shutdown();\n")
    else()
        message(STATUS "Node plugin: ${_pname} (sources only)")
    endif()

    # Plugins with server plugin metadata contribute to server-side init
    string(TOUPPER ${_pname} _pname_upper)
    if(${_pname_upper}_SERVER_PLUGIN_HEADER)
        string(APPEND PLUGIN_SERVER_INCLUDES
            "#include \"${${_pname_upper}_SERVER_PLUGIN_HEADER}\"\n")
        string(APPEND PLUGIN_START_LISTENER_CALLS
            "    ${${_pname_upper}_SERVER_PLUGIN_START}(ctx);\n")
        string(APPEND PLUGIN_STOP_LISTENER_CALLS
            "    ${${_pname_upper}_SERVER_PLUGIN_STOP}();\n")
    endif()
endforeach()

# Generate plugin init header
configure_file(
    ${PROJECT_SOURCE_DIR}/src/nodes/plugin_init.hpp.in
    ${CMAKE_BINARY_DIR}/generated/plugin_init.hpp
    @ONLY
)

# Storage library (SQLite persistence for graphs)
add_library(storage
    src/storage/GraphStorage.cpp
)

target_include_directories(storage PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(storage PUBLIC
    nodes
    nlohmann_json::nlohmann_json
    SQLite::SQLite3
)

# Server library
add_library(server
    src/server/HttpServer.cpp
    src/server/HttpSession.cpp
    src/server/RequestHandler.cpp
    src/server/SessionManager.cpp
    src/server/Logger.cpp
    src/server/Profiler.cpp
)

target_include_directories(server PUBLIC
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(server PUBLIC
    dataframe
    nodes
    storage
    postgres
    Boost::system
)

# Add server sources from plugins (e.g., PostgresListener)
foreach(_pcmake ${_plugin_cmakes})
    get_filename_component(_pdir "${_pcmake}" DIRECTORY)
    get_filename_component(_pname "${_pdir}" NAME)
    string(TOUPPER ${_pname} _pname_upper)
    if(${_pname_upper}_SERVER_SOURCES)
        target_sources(server PRIVATE ${${_pname_upper}_SERVER_SOURCES})
        if(${_pname_upper}_SERVER_INCLUDE_DIRS)
            target_include_directories(server PRIVATE ${${_pname_upper}_SERVER_INCLUDE_DIRS})
        endif()
    endif()
endforeach()

# Main server executable
add_executable(anodeServer main.cpp)

target_link_libraries(anodeServer PRIVATE
    server
)

# Example executable
add_executable(dataframe_example
    examples/dataframe_example.cpp
)

target_link_libraries(dataframe_example PRIVATE
    dataframe
)

# Benchmark executable
add_executable(benchmark
    examples/benchmark_optimized.cpp
)

target_link_libraries(benchmark PRIVATE
    dataframe
    benchmark_lib
)

# hosseinmoein/DataFrame benchmark executable
add_executable(benchmark_hosseinmoein
    examples/benchmark_hosseinmoein.cpp
)

target_link_libraries(benchmark_hosseinmoein PRIVATE
    DataFrame::DataFrame
)

# Fetch Catch2 for testing
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.0
)
FetchContent_MakeAvailable(Catch2)

# Tests executable
add_executable(dataframe_tests
    tests/StringPoolTest.cpp
    tests/ColumnTest.cpp
    tests/DataFrameTest.cpp
    tests/DataFrameFilterTest.cpp
    tests/DataFrameSorterTest.cpp
    tests/DataFrameAggregatorTest.cpp
    tests/DataFrameJoinerTest.cpp
    tests/DataFrameSerializerTest.cpp
    tests/DataFrameIOTest.cpp
)

target_link_libraries(dataframe_tests PRIVATE
    dataframe
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(dataframe_tests)

# Node tests executable
set(NODE_TEST_SOURCES
    tests/NodeTypesTest.cpp
    tests/NodeContextTest.cpp
    tests/NodeBuilderTest.cpp
    tests/NodeExecutorTest.cpp
    tests/TestNodesTest.cpp
    tests/NodeGraphSerializerTest.cpp
    tests/SelectNodesTest.cpp
    tests/DynamicNodesTest.cpp
    tests/EquationParserTest.cpp
    tests/LabelNodesTest.cpp
)

# Auto-discover test files from active plugins only
foreach(_pcmake ${_plugin_cmakes})
    get_filename_component(_pdir "${_pcmake}" DIRECTORY)
    if(EXISTS "${_pdir}/tests")
        file(GLOB _ptests "${_pdir}/tests/*.cpp")
        list(APPEND NODE_TEST_SOURCES ${_ptests})
    endif()
endforeach()

add_executable(node_tests ${NODE_TEST_SOURCES})

target_link_libraries(node_tests PRIVATE
    nodes
    Catch2::Catch2WithMain
)

catch_discover_tests(node_tests)

# Storage tests executable
add_executable(storage_tests
    tests/GraphStorageTest.cpp
)

target_link_libraries(storage_tests PRIVATE
    storage
    Catch2::Catch2WithMain
)

catch_discover_tests(storage_tests)

# PostgreSQL tests executable
add_executable(postgres_tests
    tests/PostgresPoolTest.cpp
)

target_link_libraries(postgres_tests PRIVATE
    postgres
    nodes
    Catch2::Catch2WithMain
)

catch_discover_tests(postgres_tests)
