cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED BINARY_DIR OR BINARY_DIR STREQUAL "")
    message(FATAL_ERROR "BINARY_DIR is required")
endif()

if(NOT DEFINED BUILD_TARGET OR BUILD_TARGET STREQUAL "")
    message(FATAL_ERROR "BUILD_TARGET is required")
endif()

if(NOT DEFINED CONFIG)
    set(CONFIG "")
endif()

set(MAX_ATTEMPTS 3)
set(attempt 1)
set(last_result 1)
set(lock_error_found FALSE)

while(attempt LESS_EQUAL MAX_ATTEMPTS)
    message(STATUS "[gate-retry] Attempt ${attempt}/${MAX_ATTEMPTS}: building target '${BUILD_TARGET}'")

    set(build_cmd ${CMAKE_COMMAND} --build ${BINARY_DIR} --target ${BUILD_TARGET})
    if(NOT CONFIG STREQUAL "")
        list(APPEND build_cmd --config ${CONFIG})
    endif()

    execute_process(
        COMMAND ${build_cmd}
        RESULT_VARIABLE build_result
        OUTPUT_VARIABLE build_output
        ERROR_VARIABLE build_error
    )

    set(combined_output "${build_output}\n${build_error}")

    if(build_result EQUAL 0)
        message(STATUS "[gate-retry] Build succeeded on attempt ${attempt}")
        message("${combined_output}")
        set(last_result 0)
        break()
    endif()

    string(FIND "${combined_output}" "opening deps log: Permission denied" deps_perm_idx)
    string(FIND "${combined_output}" "bad deps log signature or version" deps_sig_idx)

    if((deps_perm_idx GREATER -1) OR (deps_sig_idx GREATER -1))
        set(lock_error_found TRUE)
        message(WARNING "[gate-retry] Ninja deps-log contention/corruption detected. Retrying...")

        # Remove stale recompact side-file if present; ninja can regenerate it.
        file(REMOVE "${BINARY_DIR}/.ninja_deps.recompact")

        if(attempt LESS MAX_ATTEMPTS)
            execute_process(COMMAND ${CMAKE_COMMAND} -E sleep 1)
        endif()
    else()
        message("${combined_output}")
        message(FATAL_ERROR "[gate-retry] Build failed with non-retryable error (exit ${build_result})")
    endif()

    math(EXPR attempt "${attempt} + 1")
    set(last_result ${build_result})
endwhile()

if(last_result EQUAL 0)
    return()
endif()

if(lock_error_found)
    message(FATAL_ERROR "[gate-retry] Build failed after ${MAX_ATTEMPTS} attempts due to persistent ninja deps-log lock contention")
endif()

message(FATAL_ERROR "[gate-retry] Build failed after ${MAX_ATTEMPTS} attempts (exit ${last_result})")
