add_executable(order_generator
    order_generator.cpp
)

target_link_libraries(order_generator PRIVATE engine)
target_compile_options(order_generator PRIVATE -Wall -Wextra -Wpedantic)
