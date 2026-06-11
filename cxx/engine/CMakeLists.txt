add_library(engine
    src/order_book.cpp
    src/matching_engine.cpp
)

target_include_directories(engine PUBLIC include)

target_compile_options(engine PRIVATE -Wall -Wextra -Wpedantic)

add_subdirectory(tests)
