# RawrXD CLI Executable
# PowerShell-style command-line interface with identical capabilities to GUI

set(CLI_SOURCES
    cli_main.cpp
)

# Add the CLI executable
add_executable(rawrxd-cli-new ${CLI_SOURCES})

# Link against the shared core library
target_link_libraries(rawrxd-cli-new PRIVATE
    RawrXD-Core
)

# Include directories
target_include_directories(rawrxd-cli-new PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/core
    ${Qt6Core_INCLUDE_DIRS}
)

# Qt settings
set_target_properties(rawrxd-cli-new PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE  # Console application, not GUI
    OUTPUT_NAME "rawrxd-cli"
)

# Installation
install(TARGETS rawrxd-cli-new
    RUNTIME DESTINATION bin
    COMPONENT cli
)

# IDE CLI client (QLocalSocket JSON commands)
add_executable(rawrxd-ide-cli
    ide_cli.cpp
)

target_link_libraries(rawrxd-ide-cli PRIVATE
    Qt6::Core
)

target_include_directories(rawrxd-ide-cli PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${Qt6Core_INCLUDE_DIRS}
)

set_target_properties(rawrxd-ide-cli PROPERTIES
    AUTOMOC ON
    WIN32_EXECUTABLE FALSE
    OUTPUT_NAME "rawrxd-ide-cli"
)

install(TARGETS rawrxd-ide-cli
    RUNTIME DESTINATION bin
    COMPONENT cli
)
