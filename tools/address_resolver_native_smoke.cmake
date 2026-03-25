if(NOT DEFINED RESOLVER_EXE)
    message(FATAL_ERROR "RESOLVER_EXE is required")
endif()
if(NOT DEFINED MODULE_EXE)
    message(FATAL_ERROR "MODULE_EXE is required")
endif()
if(NOT DEFINED MAP_FILE)
    message(FATAL_ERROR "MAP_FILE is required")
endif()

if(NOT DEFINED ALLOW_MISSING_ARTIFACTS)
    set(ALLOW_MISSING_ARTIFACTS OFF)
endif()

if(NOT DEFINED FORCE_HELP_PROBE)
    set(FORCE_HELP_PROBE OFF)
endif()

string(TOUPPER "${ALLOW_MISSING_ARTIFACTS}" _allow_missing_upper)
set(_allow_missing OFF)
if(_allow_missing_upper STREQUAL "ON" OR _allow_missing_upper STREQUAL "TRUE" OR _allow_missing_upper STREQUAL "1")
    set(_allow_missing ON)
endif()

string(TOUPPER "${FORCE_HELP_PROBE}" _force_help_upper)
set(_force_help OFF)
if(_force_help_upper STREQUAL "ON" OR _force_help_upper STREQUAL "TRUE" OR _force_help_upper STREQUAL "1")
    set(_force_help ON)
endif()

function(_run_help_probe)
    execute_process(
        COMMAND "${RESOLVER_EXE}" --help
        TIMEOUT 10
        RESULT_VARIABLE _help_result
        OUTPUT_VARIABLE _help_output
        ERROR_VARIABLE _help_error
    )

    # --help currently exits with non-zero in the native resolver usage path.
    if(NOT _help_result EQUAL 0 AND NOT _help_result EQUAL 2)
        message(FATAL_ERROR "Resolver --help probe failed with code ${_help_result}\nSTDOUT:\n${_help_output}\nSTDERR:\n${_help_error}")
    endif()

    string(FIND "${_help_output}" "Usage:" _usage_idx)
    if(_usage_idx EQUAL -1)
        message(FATAL_ERROR "Resolver --help probe did not emit usage text.\nSTDOUT:\n${_help_output}\nSTDERR:\n${_help_error}")
    endif()

    message(STATUS "address_resolver_native smoke fallback probe passed (--help)")
endfunction()

if(NOT EXISTS "${RESOLVER_EXE}")
    if(_allow_missing)
        message(STATUS "address_resolver_native smoke skipped: resolver missing at ${RESOLVER_EXE}")
        return()
    endif()
    message(FATAL_ERROR "Resolver executable not found: ${RESOLVER_EXE}")
endif()

if(_force_help)
    message(STATUS "address_resolver_native smoke: FORCE_HELP_PROBE enabled; running resolver --help probe")
    _run_help_probe()
    return()
endif()

if(NOT EXISTS "${MODULE_EXE}")
    if(_allow_missing)
        message(STATUS "address_resolver_native smoke: module missing at ${MODULE_EXE}; running resolver --help fallback probe")
        _run_help_probe()
        return()
    endif()
    message(FATAL_ERROR "Module executable not found: ${MODULE_EXE}")
endif()
if(NOT EXISTS "${MAP_FILE}")
    if(_allow_missing)
        message(STATUS "address_resolver_native smoke: map missing at ${MAP_FILE}; running resolver --help fallback probe")
        _run_help_probe()
        return()
    endif()
    message(FATAL_ERROR "Map file not found: ${MAP_FILE}")
endif()

execute_process(
    COMMAND "${RESOLVER_EXE}" --module "${MODULE_EXE}" --address 0x0000000000DDFD00 --map "${MAP_FILE}" --skip-pdb
    TIMEOUT 15
    RESULT_VARIABLE _resolver_result
    OUTPUT_VARIABLE _resolver_output
    ERROR_VARIABLE _resolver_error
)

if(NOT _resolver_result EQUAL 0)
    message(FATAL_ERROR "Resolver exited with code ${_resolver_result}\nSTDOUT:\n${_resolver_output}\nSTDERR:\n${_resolver_error}")
endif()

string(LENGTH "${_resolver_output}" _resolver_output_len)
if(_resolver_output_len LESS 8)
    message(FATAL_ERROR "Resolver produced empty or near-empty output unexpectedly.\nSTDOUT:\n${_resolver_output}\nSTDERR:\n${_resolver_error}")
endif()

string(FIND "${_resolver_output}" "inside the module image" _inside_idx)
if(_inside_idx EQUAL -1)
    message(FATAL_ERROR "Expected 'inside the module image' in output but it was missing.\nOutput:\n${_resolver_output}")
endif()

string(FIND "${_resolver_output}" "Map Symbol" _map_symbol_idx)
if(_map_symbol_idx EQUAL -1)
    message(FATAL_ERROR "Expected 'Map Symbol' in output but it was missing.\nOutput:\n${_resolver_output}")
endif()

message(STATUS "address_resolver_native smoke check passed")
