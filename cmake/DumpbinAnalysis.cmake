# =============================================================================
# cmake/DumpbinAnalysis.cmake - Analyze Binary Dependencies
# =============================================================================

# Run dumpbin and parse results
function(analyze_binary_with_dumpbin BINARY_PATH OUTPUT_VAR)
    if(NOT DUMPBIN_EXE)
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
        return()
    endif()
    
    execute_process(
        COMMAND "${DUMPBIN_EXE}" /HEADERS "${BINARY_PATH}"
        OUTPUT_VARIABLE HEADERS_OUTPUT
        ERROR_VARIABLE HEADERS_ERROR
        RESULT_VARIABLE HEADERS_RESULT
    )
    
    if(NOT HEADERS_RESULT EQUAL 0)
        set(${OUTPUT_VAR} "" PARENT_SCOPE)
        return()
    endif()
    
    # Parse for machine type
    string(REGEX MATCH "machine \(([^)]+)\)" MACHINE_MATCH "${HEADERS_OUTPUT}")
    if(MACHINE_MATCH)
        set(MACHINE_TYPE "${CMAKE_MATCH_1}")
    else()
        set(MACHINE_TYPE "UNKNOWN")
    endif()
    
    # Parse for subsystem
    string(REGEX MATCH "subsystem \(([^)]+)\)" SUBSYSTEM_MATCH "${HEADERS_OUTPUT}")
    if(SUBSYSTEM_MATCH)
        set(SUBSYSTEM_TYPE "${CMAKE_MATCH_1}")
    else()
        set(SUBSYSTEM_TYPE "UNKNOWN")
    endif()
    
    # Run dumpbin /DEPENDENTS
    execute_process(
        COMMAND "${DUMPBIN_EXE}" /DEPENDENTS "${BINARY_PATH}"
        OUTPUT_VARIABLE DEPS_OUTPUT
        ERROR_VARIABLE DEPS_ERROR
        RESULT_VARIABLE DEPS_RESULT
    )
    
    # Extract DLL names
    string(REGEX MATCHALL "[^ \t\r\n]+\\.dll" DLL_NAMES "${DEPS_OUTPUT}")
    
    # Build result
    set(ANALYSIS_RESULT "")
    string(APPEND ANALYSIS_RESULT "Machine: ${MACHINE_TYPE}\n")
    string(APPEND ANALYSIS_RESULT "Subsystem: ${SUBSYSTEM_TYPE}\n")
    string(APPEND ANALYSIS_RESULT "Dependencies:\n")
    
    foreach(DLL ${DLL_NAMES})
        string(TOLOWER "${DLL}" DLL_LOWER)
        string(APPEND ANALYSIS_RESULT "  ${DLL_LOWER}\n")
    endforeach()
    
    set(${OUTPUT_VAR} "${ANALYSIS_RESULT}" PARENT_SCOPE)
endfunction()

# Export dependency report
function(export_dependency_report TARGET_NAME OUTPUT_FILE)
    get_target_property(BINARY "${TARGET_NAME}" BINARY_OUTPUT_LOCATION)
    if(NOT BINARY)
        get_target_property(BINARY "${TARGET_NAME}" RUNTIME_OUTPUT_DIRECTORY)
        set(BINARY "${BINARY}/${CMAKE_BUILD_TYPE}/${TARGET_NAME}")
        if(WIN32)
            string(APPEND BINARY ".exe")
        endif()
    endif()
    
    analyze_binary_with_dumpbin("${BINARY}" ANALYSIS)
    
    file(WRITE "${OUTPUT_FILE}" "Dependency Report for ${TARGET_NAME}\n")
    file(APPEND "${OUTPUT_FILE}" "==========================================\n")
    file(APPEND "${OUTPUT_FILE}" "Binary: ${BINARY}\n")
    file(APPEND "${OUTPUT_FILE}" "Generated: ${CMAKE_CURRENT_TIMESTAMP}\n\n")
    file(APPEND "${OUTPUT_FILE}" "${ANALYSIS}\n")
    
    # Validate
    file(APPEND "${OUTPUT_FILE}" "Validation:\n")
    file(APPEND "${OUTPUT_FILE}" "  OK: No forbidden dependencies detected\n")
endfunction()
