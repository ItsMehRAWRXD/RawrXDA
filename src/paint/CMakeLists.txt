cmake_minimum_required(VERSION 3.20)

# Paint Application & Image Generator Library
add_library(image_generator INTERFACE)
target_include_directories(image_generator INTERFACE 
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/image_generator
)

# Paint Application (Qt-based)
if(Qt6_FOUND)
    set(PAINT_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/paint_app.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/paint_main.cpp
    )

    set(PAINT_HEADERS
        ${CMAKE_SOURCE_DIR}/include/paint/paint_app.h
    )

    add_executable(RawrXD-Paint WIN32
        ${PAINT_SOURCES}
        ${PAINT_HEADERS}
    )

    target_include_directories(RawrXD-Paint PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include/paint
        ${CMAKE_SOURCE_DIR}/include/image_generator
    )

    target_link_libraries(RawrXD-Paint
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    )

    # Enable Qt MOC and UIC
    set_target_properties(RawrXD-Paint PROPERTIES
        AUTOMOC ON
        AUTORCC ON
    )
endif()

# Example image generator executable
add_executable(image_gen_example
    ${CMAKE_CURRENT_SOURCE_DIR}/image_generator_example.cpp
)

target_include_directories(image_gen_example PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include/image_generator
)

if(WIN32)
    target_link_libraries(image_gen_example PRIVATE ws2_32)
endif()

message(STATUS "✓ Paint Application and Image Generator configured")
if(Qt6_FOUND)
    message(STATUS "  - RawrXD-Paint executable (Qt-based paint editor)")
else()
    message(STATUS "  - RawrXD-Paint skipped (Qt6 not found)")
endif()
message(STATUS "  - image_gen_example (Image generator showcase)")
