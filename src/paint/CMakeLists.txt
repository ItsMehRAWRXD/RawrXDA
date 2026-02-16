cmake_minimum_required(VERSION 3.20)

# Paint Application & Image Generator Library
add_library(image_generator INTERFACE)
target_include_directories(image_generator INTERFACE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/image_generator
)

# Paint Application (Win32 GDI+/D2D)
set(PAINT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/paint_app.cpp
)

set(PAINT_HEADERS
    ${CMAKE_SOURCE_DIR}/include/paint/paint_app.h
)

add_executable(RawrXD-Paint WIN32
    ${PAINT_SOURCES}
    ${PAINT_HEADERS}
)

target_include_directories(RawrXD-Paint PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/paint
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/image_generator
)

# Qt-free: uses Win32 GDI/GDI+ / Direct2D
target_link_libraries(RawrXD-Paint
    gdi32
    gdiplus
    user32
    d2d1
    dwrite
    comctl32
    comdlg32
)

# Example image generator executable
add_executable(image_gen_example
    ${CMAKE_CURRENT_SOURCE_DIR}/image_generator_example.cpp
)

target_include_directories(image_gen_example PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include/image_generator
)

message(STATUS "✓ Paint Application and Image Generator configured")
message(STATUS "  - RawrXD-Paint executable (Win32 GDI+/D2D paint editor)")
message(STATUS "  - image_gen_example (Image generator showcase)")
