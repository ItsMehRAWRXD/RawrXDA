# tests/unit/CMakeLists.txt — C++20 unit tests (no gtest, no Qt)

function(add_unit_test target_name src_file)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${src_file}")
        add_executable(${target_name} "${CMAKE_CURRENT_SOURCE_DIR}/${src_file}")
        target_include_directories(${target_name} PRIVATE
            ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
        )
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
            CXX_STANDARD 20
        )
        add_test(NAME ${target_name} COMMAND ${target_name})
        message(STATUS "  ✓ Unit: ${target_name}")
    endif()
endfunction()

add_unit_test(test_xss_sanitizer         test_xss_sanitizer.cpp)
add_unit_test(test_security_manager      test_security_manager.cpp)
if(TARGET test_security_manager)
    target_sources(test_security_manager PRIVATE security_manager_test_impl.cpp)
endif()
add_unit_test(test_additional_components test_additional_components.cpp)
if(TARGET test_additional_components)
    target_sources(test_additional_components PRIVATE hardware_backend_selector_test_impl.cpp)
endif()
add_unit_test(test_distributed_trainer   test_distributed_trainer.cpp)
if(TARGET test_distributed_trainer)
    target_sources(test_distributed_trainer PRIVATE distributed_trainer_test_impl.cpp)
endif()
add_unit_test(test_debugger_error_status test_debugger_error_status.cpp)
