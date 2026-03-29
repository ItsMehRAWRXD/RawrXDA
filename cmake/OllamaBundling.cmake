# CMake script to integrate Ollama bundling into RawrXD build process
# This can be included in the main CMakeLists.txt to automatically
# bundle Ollama binaries during the build process.

# Function to add Ollama bundling target
function(add_ollama_bundling)
    set(options REQUIRED)
    set(oneValueArgs 
        TARGET_NAME
        OLLAMA_VERSION
        BUNDLE_DIR
        FORCE_DOWNLOAD
    )
    set(multiValueArgs)
    
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Set defaults
    if(NOT ARG_TARGET_NAME)
        set(ARG_TARGET_NAME "bundle_ollama")
    endif()
    
    if(NOT ARG_OLLAMA_VERSION)
        set(ARG_OLLAMA_VERSION "latest")
    endif()
    
    if(NOT ARG_BUNDLE_DIR)
        set(ARG_BUNDLE_DIR "${CMAKE_BINARY_DIR}/bundle")
    endif()
    
    # Detect PowerShell executable
    find_program(POWERSHELL_EXECUTABLE 
        NAMES pwsh powershell
        DOC "PowerShell executable for Ollama bundling"
    )
    
    if(NOT POWERSHELL_EXECUTABLE)
        if(ARG_REQUIRED)
            message(FATAL_ERROR "PowerShell not found - required for Ollama bundling")
        else
            message(WARNING "PowerShell not found - Ollama bundling disabled")
            return()
        endif()
    endif()
    
    # Set up bundle script path
    set(BUNDLE_SCRIPT "${CMAKE_SOURCE_DIR}/scripts/bundle_ollama.ps1")
    
    if(NOT EXISTS "${BUNDLE_SCRIPT}")
        if(ARG_REQUIRED)
            message(FATAL_ERROR "Ollama bundle script not found: ${BUNDLE_SCRIPT}")
        else
            message(WARNING "Ollama bundle script not found - skipping bundling")
            return()
        endif()
    endif()
    
    # Build PowerShell arguments
    set(PS_ARGS
        -ExecutionPolicy Bypass
        -NoProfile
        -File "${BUNDLE_SCRIPT}"
        -BuildDir "${CMAKE_BINARY_DIR}"
        -BundleDir "${ARG_BUNDLE_DIR}"
        -OllamaVersion "${ARG_OLLAMA_VERSION}"
    )
    
    if(ARG_FORCE_DOWNLOAD)
        list(APPEND PS_ARGS -Force)
    endif()
    
    # Create the bundling target
    add_custom_target(${ARG_TARGET_NAME}
        COMMAND ${POWERSHELL_EXECUTABLE} ${PS_ARGS}
        COMMENT "Bundling Ollama ${ARG_OLLAMA_VERSION} for embedded service"
        VERBATIM
    )
    
    # Create verification target
    add_custom_target(${ARG_TARGET_NAME}_verify
        COMMAND ${POWERSHELL_EXECUTABLE} ${PS_ARGS} -Verify
        COMMENT "Verifying Ollama bundle integrity"
        VERBATIM
    )
    
    # Create clean target
    add_custom_target(${ARG_TARGET_NAME}_clean
        COMMAND ${POWERSHELL_EXECUTABLE} ${PS_ARGS} -Clean
        COMMENT "Cleaning Ollama bundle"
        VERBATIM
    )
    
    # Set target properties
    set_target_properties(${ARG_TARGET_NAME} PROPERTIES
        FOLDER "Bundling"
        ADDITIONAL_CLEAN_FILES "${ARG_BUNDLE_DIR}"
    )
    
    # Export variables for use by other targets
    set(OLLAMA_BUNDLE_DIR "${ARG_BUNDLE_DIR}" PARENT_SCOPE)
    set(OLLAMA_BINARY_PATH "${ARG_BUNDLE_DIR}/bin/ollama.exe" PARENT_SCOPE)
    set(OLLAMA_MODELS_PATH "${ARG_BUNDLE_DIR}/models" PARENT_SCOPE)
    set(OLLAMA_CONFIG_PATH "${ARG_BUNDLE_DIR}/config/ollama" PARENT_SCOPE)
    
    message(STATUS "Ollama bundling configured:")
    message(STATUS "  Target: ${ARG_TARGET_NAME}")
    message(STATUS "  Version: ${ARG_OLLAMA_VERSION}")
    message(STATUS "  Bundle Dir: ${ARG_BUNDLE_DIR}")
    message(STATUS "  PowerShell: ${POWERSHELL_EXECUTABLE}")
    
endfunction()

# Function to add Ollama bundle installation
function(install_ollama_bundle)
    set(options)
    set(oneValueArgs 
        COMPONENT
        DESTINATION
        BUNDLE_DIR
    )
    set(multiValueArgs)
    
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    
    # Set defaults
    if(NOT ARG_COMPONENT)
        set(ARG_COMPONENT "ollama")
    endif()
    
    if(NOT ARG_DESTINATION)
        set(ARG_DESTINATION ".")
    endif()
    
    if(NOT ARG_BUNDLE_DIR)
        set(ARG_BUNDLE_DIR "${CMAKE_BINARY_DIR}/bundle")
    endif()
    
    # Install Ollama binary
    install(FILES "${ARG_BUNDLE_DIR}/bin/ollama.exe"
        DESTINATION "${ARG_DESTINATION}/bin"
        COMPONENT ${ARG_COMPONENT}
        OPTIONAL
    )
    
    # Install models directory (if exists)
    install(DIRECTORY "${ARG_BUNDLE_DIR}/models/"
        DESTINATION "${ARG_DESTINATION}/models"
        COMPONENT ${ARG_COMPONENT}
        OPTIONAL
    )
    
    # Install configuration
    install(DIRECTORY "${ARG_BUNDLE_DIR}/config/"
        DESTINATION "${ARG_DESTINATION}/config"
        COMPONENT ${ARG_COMPONENT}
        OPTIONAL
    )
    
    # Install bundle metadata
    install(FILES "${ARG_BUNDLE_DIR}/ollama-bundle.json"
        DESTINATION "${ARG_DESTINATION}"
        COMPONENT ${ARG_COMPONENT}
        OPTIONAL
    )
    
    message(STATUS "Ollama bundle installation configured for component '${ARG_COMPONENT}'")
endfunction()

# Function to create resource file for embedding Ollama info
function(generate_ollama_resource_header)
    set(oneValueArgs 
        OUTPUT_FILE
        BUNDLE_DIR
        NAMESPACE
    )
    
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "" ${ARGN})
    
    if(NOT ARG_OUTPUT_FILE)
        set(ARG_OUTPUT_FILE "${CMAKE_BINARY_DIR}/generated/ollama_bundle_info.h")
    endif()
    
    if(NOT ARG_BUNDLE_DIR)
        set(ARG_BUNDLE_DIR "${CMAKE_BINARY_DIR}/bundle")
    endif()
    
    if(NOT ARG_NAMESPACE)
        set(ARG_NAMESPACE "RawrXD::Embedded")
    endif()
    
    # Ensure output directory exists
    get_filename_component(OUTPUT_DIR "${ARG_OUTPUT_FILE}" DIRECTORY)
    file(MAKE_DIRECTORY "${OUTPUT_DIR}")
    
    # Generate header content
    string(TIMESTAMP BUILD_TIMESTAMP "%Y-%m-%d %H:%M:%S")
    
    file(WRITE "${ARG_OUTPUT_FILE}"
"// Generated Ollama bundle information header
// Generated at: ${BUILD_TIMESTAMP}
// This file contains compile-time information about the embedded Ollama bundle

#pragma once

namespace ${ARG_NAMESPACE} {
    namespace Ollama {
        // Bundle configuration
        constexpr const char* BUNDLE_DIR = \"${ARG_BUNDLE_DIR}\";
        constexpr const char* BINARY_PATH = \"${ARG_BUNDLE_DIR}/bin/ollama.exe\";
        constexpr const char* MODELS_PATH = \"${ARG_BUNDLE_DIR}/models\";
        constexpr const char* CONFIG_PATH = \"${ARG_BUNDLE_DIR}/config/ollama\";
        
        // Build information
        constexpr const char* BUNDLED_AT = \"${BUILD_TIMESTAMP}\";
        constexpr const char* CMAKE_BUILD_TYPE = \"${CMAKE_BUILD_TYPE}\";
        constexpr const char* CMAKE_SYSTEM_NAME = \"${CMAKE_SYSTEM_NAME}\";
        
        // Default service configuration
        constexpr const char* DEFAULT_HOST = \"127.0.0.1\";
        constexpr int DEFAULT_PORT = 11434;
        constexpr const char* DEFAULT_LOG_LEVEL = \"info\";
        constexpr bool AUTO_START_SERVICE = true;
        constexpr int HEALTH_CHECK_INTERVAL_MS = 5000;
        constexpr int MAX_RESTART_ATTEMPTS = 3;
        
        // Feature flags
        constexpr bool EMBEDDED_OLLAMA_ENABLED = true;
        constexpr bool AUTO_DOWNLOAD_ENABLED = true;
        constexpr bool TERMINAL_INTEGRATION_ENABLED = true;
    }
}
")
    
    message(STATUS "Generated Ollama resource header: ${ARG_OUTPUT_FILE}")
endfunction()

# Example usage macro for easy integration
macro(setup_embedded_ollama)
    # Add Ollama bundling with default configuration
    add_ollama_bundling(
        TARGET_NAME ollama_bundle
        OLLAMA_VERSION latest
        BUNDLE_DIR ${CMAKE_BINARY_DIR}/dist/bundle
    )
    
    # Generate resource header
    generate_ollama_resource_header(
        OUTPUT_FILE ${CMAKE_BINARY_DIR}/generated/ollama_embedded.h
        BUNDLE_DIR ${CMAKE_BINARY_DIR}/dist/bundle
        NAMESPACE RawrXD::Embedded
    )
    
    # Add to main executable dependencies
    if(TARGET RawrXD-Win32IDE)
        add_dependencies(RawrXD-Win32IDE ollama_bundle)
        target_include_directories(RawrXD-Win32IDE PRIVATE ${CMAKE_BINARY_DIR}/generated)
    endif()
    
    # Setup installation
    install_ollama_bundle(
        COMPONENT runtime
        DESTINATION .
        BUNDLE_DIR ${CMAKE_BINARY_DIR}/dist/bundle
    )
endmacro()