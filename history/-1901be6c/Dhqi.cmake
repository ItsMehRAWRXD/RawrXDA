set(Qt6_DIR "C:/Qt/6.7.3/msvc2022_64/lib/cmake/Qt6" CACHE PATH "Path to Qt6 configuration")
list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.7.3/msvc2022_64")

message(STATUS "=== STARTING QT6 DETECTION ===")
find_package(Qt6 REQUIRED COMPONENTS Core)

if(Qt6_FOUND)
    message(STATUS "Qt6 found successfully: ${Qt6_VERSION}")
else()
    message(FATAL_ERROR "Qt6 NOT FOUND")
endif()