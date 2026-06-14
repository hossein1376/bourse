include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(engine_test
    order_book_test.cpp
    matching_engine_test.cpp
    kafka_engine_test.cpp
    kafka_integration_test.cpp
)

target_include_directories(engine_test PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}/../tools/kafka_engine
)

target_link_libraries(engine_test PRIVATE engine GTest::gtest_main flatbuffers rdkafka++)

enable_testing()
add_test(NAME engine_test COMMAND engine_test)
