# PHASE 2 POLISH FEATURES - CMAKE INTEGRATION
# Add this to your CMakeLists.txt

# ============================================================================
# Define Phase 2 source files
# ============================================================================

set(PHASE2_POLISH_SOURCES
    src/ui/diff_preview_widget.cpp
    src/ui/streaming_token_progress.cpp
    src/ui/gpu_backend_selector.cpp
    src/ui/auto_model_downloader.cpp
    src/ui/telemetry_optin_dialog.cpp
    src/ui/phase2_integration_example.cpp  # Optional - just examples
)

set(PHASE2_POLISH_HEADERS
    src/ui/diff_preview_widget.h
    src/ui/streaming_token_progress.h
    src/ui/gpu_backend_selector.h
    src/ui/auto_model_downloader.h
    src/ui/model_download_dialog.h  # Header-only implementation
    src/ui/telemetry_optin_dialog.h
)

# ============================================================================
# Add to existing target
# ============================================================================

# Option 1: If you have a single executable target
target_sources(RawrXD_IDE PRIVATE
    ${PHASE2_POLISH_SOURCES}
    ${PHASE2_POLISH_HEADERS}
)

# Option 2: If you have a library target for UI components
# add_library(RawrXD_UI_Phase2 STATIC
#     ${PHASE2_POLISH_SOURCES}
#     ${PHASE2_POLISH_HEADERS}
# )
# target_link_libraries(RawrXD_IDE PRIVATE RawrXD_UI_Phase2)

# ============================================================================
# Required Qt modules
# ============================================================================

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Network      # For AutoModelDownloader
)

target_link_libraries(RawrXD_IDE PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
)

# ============================================================================
# Windows-specific dependencies (for GPU detection)
# ============================================================================

if(WIN32)
    target_link_libraries(RawrXD_IDE PRIVATE
        dxgi        # DirectX Graphics Infrastructure (GPU detection)
        d3d12       # Direct3D 12 (optional)
    )
endif()

# ============================================================================
# Include directories
# ============================================================================

target_include_directories(RawrXD_IDE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
)

# ============================================================================
# Compiler flags for production readiness
# ============================================================================

if(MSVC)
    target_compile_options(RawrXD_IDE PRIVATE
        /W4                 # Warning level 4
        /WX-                # Warnings are NOT errors (for production)
        /permissive-        # Strict conformance
        /EHsc               # Exception handling model
    )
else()
    target_compile_options(RawrXD_IDE PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter  # Qt often has unused params
    )
endif()

# ============================================================================
# Installation rules (optional)
# ============================================================================

install(TARGETS RawrXD_IDE
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# Install UI resources if any
# install(DIRECTORY resources/ui
#     DESTINATION share/RawrXD/ui
# )

# ============================================================================
# Testing (optional)
# ============================================================================

# Enable testing
# enable_testing()

# add_executable(test_diff_preview tests/test_diff_preview.cpp)
# target_link_libraries(test_diff_preview PRIVATE RawrXD_UI_Phase2 Qt6::Test)
# add_test(NAME DiffPreviewTest COMMAND test_diff_preview)

# ============================================================================
# Package configuration
# ============================================================================

set(CPACK_PACKAGE_NAME "RawrXD-IDE")
set(CPACK_PACKAGE_VERSION "5.0.0")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "RawrXD Agentic IDE - Phase 2 Polish")
set(CPACK_PACKAGE_VENDOR "RawrXD")

include(CPack)
