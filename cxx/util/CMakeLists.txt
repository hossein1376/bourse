include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
)
FetchContent_MakeAvailable(googletest)

add_executable(linked_list_test
    linked_list_test.cpp
)

target_include_directories(linked_list_test PRIVATE ..)
target_link_libraries(linked_list_test PRIVATE GTest::gtest_main)
target_compile_options(linked_list_test PRIVATE -Wall -Wextra -Wpedantic)

include(GoogleTest)
gtest_discover_tests(linked_list_test)