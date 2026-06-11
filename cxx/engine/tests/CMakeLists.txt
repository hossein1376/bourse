include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(engine_test
    order_book_test.cpp
)

target_link_libraries(engine_test PRIVATE engine GTest::gtest_main)

enable_testing()
add_test(NAME engine_test COMMAND engine_test)
