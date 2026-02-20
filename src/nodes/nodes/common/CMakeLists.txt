add_library(common_nodes OBJECT
    register.cpp
    ScalarNodes.cpp
    CsvNodes.cpp
    MathNodes.cpp
    AggregateNodes.cpp
    SelectNodes.cpp
    StringNodes.cpp
    PostgresNodes.cpp
    DynamicNodes.cpp
    LabelNodes.cpp
    VizNodes.cpp
)

target_include_directories(common_nodes PUBLIC
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(common_nodes PUBLIC dataframe postgres)
