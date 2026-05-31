# Scenario 4 smoke companions — live extension-host IPC ping tools (Win32 only).
if(NOT WIN32)
    return()
endif()

add_executable(RawrXDIpcPing EXCLUDE_FROM_ALL
    src/tools/RawrXDIpcPing.cpp
)
target_include_directories(RawrXDIpcPing PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/win32app
)
if(MSVC)
    target_compile_options(RawrXDIpcPing PRIVATE /EHsc /W2 /O2)
endif()
target_link_libraries(RawrXDIpcPing PRIVATE kernel32)
set_target_properties(RawrXDIpcPing PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXDIpcPing
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
message(STATUS "[Smoke] RawrXDIpcPing target (Scenario 4 live IPC ping)")

add_executable(RawrXD-ExtensionHost EXCLUDE_FROM_ALL
    src/tools/RawrXDExtensionHost.cpp
    src/win32app/ExtensionHostIpcBridge.cpp
)
target_include_directories(RawrXD-ExtensionHost PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/win32app
)
target_compile_definitions(RawrXD-ExtensionHost PRIVATE RAWRXD_IPC_BRIDGE_QUIET=1)
if(MSVC)
    target_compile_options(RawrXD-ExtensionHost PRIVATE /EHsc /W2 /O2)
endif()
target_link_libraries(RawrXD-ExtensionHost PRIVATE kernel32)
set_target_properties(RawrXD-ExtensionHost PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    OUTPUT_NAME RawrXD-ExtensionHost
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)
add_custom_command(TARGET RawrXDIpcPing POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:RawrXDIpcPing>"
        "${CMAKE_BINARY_DIR}/bin/IpcPingTool.exe"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:RawrXDIpcPing>"
        "${CMAKE_BINARY_DIR}/bin/RawrXD-IpcPingTool.exe"
    COMMENT "[Smoke] Staging IpcPingTool / RawrXD-IpcPingTool aliases for Scenario 4"
)
add_custom_command(TARGET RawrXD-ExtensionHost POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:RawrXD-ExtensionHost>"
        "${CMAKE_BINARY_DIR}/bin/IpcPingTool.exe"
    COMMENT "[Smoke] Staging IpcPingTool.exe alias for Scenario 4 harness"
)
message(STATUS "[Smoke] RawrXD-ExtensionHost target (Scenario 4 companion client)")
