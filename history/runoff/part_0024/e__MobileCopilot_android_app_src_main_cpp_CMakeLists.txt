cmake_minimum_required(VERSION 3.22.1)

project("mobilecopilot")

# TensorFlow Lite
set(TENSORFLOW_LITE_DIR "${CMAKE_SOURCE_DIR}/../../../../tflite")
add_library(tensorflowlite SHARED IMPORTED)
set_target_properties(tensorflowlite PROPERTIES
    IMPORTED_LOCATION "${TENSORFLOW_LITE_DIR}/lib/${ANDROID_ABI}/libtensorflowlite.so")

# Native memory mapping and file I/O library
add_library(native-fileio SHARED
    native-fileio.cpp
    memory-mapper.cpp
    model-inference.cpp
    gguf-parser.cpp
)

target_include_directories(native-fileio PRIVATE
    ${TENSORFLOW_LITE_DIR}/include
)

find_library(log-lib log)

target_link_libraries(native-fileio
    ${log-lib}
    tensorflowlite
    android
)