include(FetchContent)

# ── FlatBuffers ──
set(FLATBUFFERS_BUILD_FLATC ON CACHE BOOL "Build flatc" FORCE)

FetchContent_Declare(flatbuffers
    GIT_REPOSITORY https://github.com/google/flatbuffers.git
    GIT_TAG v25.2.10
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(flatbuffers)

set(FBS_SCHEMAS
    ${CMAKE_CURRENT_SOURCE_DIR}/schema/order.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/schema/trade_event.fbs
    ${CMAKE_CURRENT_SOURCE_DIR}/schema/book_update.fbs
)

set(FBS_GENERATED
    ${CMAKE_CURRENT_BINARY_DIR}/order_generated.h
    ${CMAKE_CURRENT_BINARY_DIR}/trade_event_generated.h
    ${CMAKE_CURRENT_BINARY_DIR}/book_update_generated.h
)

add_custom_command(OUTPUT ${FBS_GENERATED}
    COMMAND flatc --cpp -o ${CMAKE_CURRENT_BINARY_DIR}
        ${FBS_SCHEMAS}
    DEPENDS flatc ${FBS_SCHEMAS}
    COMMENT "Generating FlatBuffers C++ headers"
)

# ── librdkafka ──
FetchContent_Declare(rdkafka
    GIT_REPOSITORY https://github.com/confluentinc/librdkafka.git
    GIT_TAG v2.6.1
    GIT_SHALLOW ON
)
FetchContent_MakeAvailable(rdkafka)

# ── Executable ──
add_executable(kafka_engine
    main.cpp
    consumer.cpp
    producer.cpp
    ${FBS_GENERATED}
)

add_executable(publish_test_order
    publish_test_order.cpp
    ${FBS_GENERATED}
)

foreach(tgt kafka_engine publish_test_order)
  target_include_directories(${tgt} PRIVATE
      ${CMAKE_CURRENT_BINARY_DIR}
      ${flatbuffers_SOURCE_DIR}/include
  )
  target_link_libraries(${tgt} PRIVATE
      engine
      flatbuffers
      rdkafka++
  )
  target_compile_options(${tgt} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
