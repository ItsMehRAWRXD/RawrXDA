# Add Autonomous IDE Diagnostic and Self-Healing System

if(MSVC)
    message(STATUS "========================================")
    message(STATUS "Building Autonomous IDE Diagnostic System")
    message(STATUS "========================================")
    
    # IDEDiagnosticAutoHealer library (core self-healing engine)
    add_library(IDEDiagnosticAutoHealer STATIC
        src/win32app/IDEDiagnosticAutoHealer_Impl.cpp
        src/win32app/IDEDiagnosticAutoHealer.h
    )
    
    target_include_directories(IDEDiagnosticAutoHealer PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/win32app
    )
    
    target_link_libraries(IDEDiagnosticAutoHealer PRIVATE
        user32
        kernel32
        ntdll
    )
    
    target_compile_options(IDEDiagnosticAutoHealer PRIVATE /EHsc /O2 /W3)
    target_compile_definitions(IDEDiagnosticAutoHealer PRIVATE _CRT_SECURE_NO_WARNINGS)
    
    set_target_properties(IDEDiagnosticAutoHealer PROPERTIES
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )
    
    message(STATUS "✓ IDEDiagnosticAutoHealer library configured")
    
    # IDEAutoHealerLauncher executable (autonomous test harness)
    add_executable(IDEAutoHealerLauncher
        src/win32app/IDEAutoHealerLauncher.cpp
    )
    
    target_include_directories(IDEAutoHealerLauncher PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/src/win32app
    )
    
    target_link_libraries(IDEAutoHealerLauncher PRIVATE
        IDEDiagnosticAutoHealer
        user32
        kernel32
        ntdll
    )
    
    target_compile_options(IDEAutoHealerLauncher PRIVATE /EHsc /O2)
    target_compile_definitions(IDEAutoHealerLauncher PRIVATE _CRT_SECURE_NO_WARNINGS NOMINMAX)
    
    set_target_properties(IDEAutoHealerLauncher PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        WIN32_EXECUTABLE FALSE  # Console app for diagnostic output
    )
    
    message(STATUS "✓ IDEAutoHealerLauncher executable configured")
    
    # Integration: Link autonomous healer into RawrXD-Win32IDE
    if(TARGET RawrXD-Win32IDE)
        target_link_libraries(RawrXD-Win32IDE PRIVATE IDEDiagnosticAutoHealer)
        target_compile_definitions(RawrXD-Win32IDE PRIVATE HAVE_AUTONOMOUS_HEALER=1)
        message(STATUS "✓ Autonomous healer integrated with RawrXD-Win32IDE")
    else()
        message(STATUS "RawrXD-Win32IDE target not found - autonomous healer available as standalone only")
    endif()
    
    message(STATUS "========================================")
    message(STATUS "Autonomous IDE Diagnostic System Ready")
    message(STATUS "========================================")
    message(STATUS "Executables:")
    message(STATUS "  • IDEAutoHealerLauncher - Run autonomous diagnostics")
    message(STATUS "  • RawrXD-Win32IDE - IDE with integrated healing (if built)")
    message(STATUS "")
    message(STATUS "Usage:")
    message(STATUS "  IDEAutoHealerLauncher.exe --full-diagnostic")
    message(STATUS "  IDEAutoHealerLauncher.exe --recover")
    message(STATUS "========================================")
    
else()
    message(STATUS "⚠ Autonomous diagnostics require MSVC compiler - skipping")
endif()

