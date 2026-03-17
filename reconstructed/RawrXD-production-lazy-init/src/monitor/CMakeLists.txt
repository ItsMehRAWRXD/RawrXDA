# RawrXD IDE Health Monitor CMake configuration
add_library(ide_health_monitor
    ide_health_monitor.hpp
    ide_health_monitor.cpp
    feature_probes.hpp
)

target_include_directories(ide_health_monitor PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(ide_health_monitor
    ws2_32  # Windows sockets
)

# IDE Monitor executable
add_executable(ide_monitor
    ide_monitor_main.cpp
)

target_link_libraries(ide_monitor
    ide_health_monitor
    ws2_32
)

set_target_properties(ide_monitor PROPERTIES
    FOLDER "Tests"
    OUTPUT_NAME "ide_monitor"
)

# Deploy to tests directory
add_custom_command(TARGET ide_monitor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy
    $<TARGET_FILE:ide_monitor>
    "${CMAKE_BINARY_DIR}/bin/tests/Release/ide_monitor.exe"
)
