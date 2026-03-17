#=============================================================================
# Pure MASM Crypto Library CMake Configuration
# Zero dependencies - OpenSSL API compatible
#=============================================================================

cmake_minimum_required(VERSION 3.16)

# Define project
project(masm_crypto C ASM_MASM)

# Enable MASM
enable_language(ASM_MASM)

# Set MASM flags
set(CMAKE_ASM_MASM_FLAGS "${CMAKE_ASM_MASM_FLAGS} /c /Zi")

# Source files
set(MASMC_SOURCES
    masm_crypto_rand.asm
    masm_crypto_aes.asm
    masm_crypto_evp.asm
)

# Create static library
add_library(masm_crypto STATIC ${MASMC_SOURCES})

# Set library properties
set_target_properties(masm_crypto PROPERTIES
    LINKER_LANGUAGE C
    ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Include directories
target_include_directories(masm_crypto PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
    $<INSTALL_INTERFACE:include>
)

# Install rules
install(TARGETS masm_crypto
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/../include/masm_crypto.h
    DESTINATION include
)

# Export for find_package
install(EXPORT masm_crypto-targets
    FILE masm_crypto-targets.cmake
    NAMESPACE masm_crypto::
    DESTINATION lib/cmake/masm_crypto
)

# Create config file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/masm_crypto-config-version.cmake"
    VERSION 1.0.0
    COMPATIBILITY AnyNewerVersion
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/masm_crypto-config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/masm_crypto-config.cmake"
    @ONLY
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/masm_crypto-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/masm_crypto-config-version.cmake"
    DESTINATION lib/cmake/masm_crypto
)