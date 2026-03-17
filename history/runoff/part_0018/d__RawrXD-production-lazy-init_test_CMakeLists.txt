cmake_minimum_required(VERSION 3.20)

project(RawrXD-Orch-Test LANGUAGES C)

set(TEST_SOURCES
  test_stream_orch.c
)

if(TARGET gguf_analyzer_masm64)
  # If the MASM target exists in the project, compile only the test and link MASM objects
  add_executable(test_stream_orch ${TEST_SOURCES})
  target_include_directories(test_stream_orch PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
  target_link_libraries(test_stream_orch PRIVATE gguf_analyzer_masm64)
else()
  # Otherwise build with the C fallback implementation so tests are runnable
  list(APPEND TEST_SOURCES stream_orch_fallback.c)
  add_executable(test_stream_orch ${TEST_SOURCES})
  target_include_directories(test_stream_orch PRIVATE ${CMAKE_SOURCE_DIR}/src/core)
endif()

set_target_properties(test_stream_orch PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
  C_STANDARD 11
)

# Optional: Enhanced Compression Test (only if Qt6 is available)
if(NOT DEFINED Qt6_FOUND)
  find_package(Qt6 QUIET COMPONENTS Core Test)
endif()

if(Qt6_FOUND)
  # Enhanced Compression Test CMake Configuration
  # Find Qt6 components
  find_package(Qt6 REQUIRED COMPONENTS Core Test)

  # Create test executable
  add_executable(test_enhanced_compression
      test_enhanced_compression.cpp
      ../src/compression_interface.cpp
  )

  # Link Qt6 libraries
  target_link_libraries(test_enhanced_compression
      Qt6::Core
      Qt6::Test
  )

  # Set Qt6 requirements
  qt6_standard_project_setup()

  # Include directories
  target_include_directories(test_enhanced_compression PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/../src
      ${CMAKE_CURRENT_SOURCE_DIR}
  )

  # Compiler definitions for compression
  target_compile_definitions(test_enhanced_compression PRIVATE
      HAVE_COMPRESSION=1
      HAVE_ZLIB=1
      QT_NO_KEYWORDS
  )

  # Enable C++17
  target_compile_features(test_enhanced_compression PRIVATE cxx_std_17)

  # Platform-specific settings
  if(WIN32)
      # Windows-specific settings
      target_compile_definitions(test_enhanced_compression PRIVATE
          WIN32_LEAN_AND_MEAN
          NOMINMAX
      )
      
      # Link Windows libraries for SIMD and threading
      target_link_libraries(test_enhanced_compression
          kernel32
          user32
      )
      
      # Enable SIMD optimizations
      if(MSVC)
          target_compile_options(test_enhanced_compression PRIVATE
              /arch:AVX2
              /O2
          )
      endif()
  elseif(UNIX)
      # Unix-specific settings
      target_link_libraries(test_enhanced_compression
          pthread
      )
      
      # Enable SIMD optimizations for GCC/Clang
      target_compile_options(test_enhanced_compression PRIVATE
          -mavx2
          -msse4.2
          -O2
      )
  endif()

  # Test runner configuration
  enable_testing()
  add_test(
      NAME enhanced_compression_test
      COMMAND test_enhanced_compression
      WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
  )

  # Set test properties
  set_tests_properties(enhanced_compression_test
      PROPERTIES
      TIMEOUT 300  # 5 minutes timeout
      LABELS "compression;performance"
  )

  # Add custom target for running tests
  add_custom_target(run_compression_tests
      COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure --label-regex "compression"
      DEPENDS test_enhanced_compression
      COMMENT "Running enhanced compression tests"
  )

  # Performance test target
  add_custom_target(compression_benchmark
      COMMAND test_enhanced_compression
      DEPENDS test_enhanced_compression
      COMMENT "Running compression performance benchmarks"
  )

  # Debug information
  message(STATUS "Enhanced Compression Test Configuration:")
  message(STATUS "  Qt6 Core: ${Qt6Core_FOUND}")
  message(STATUS "  Qt6 Test: ${Qt6Test_FOUND}")
  message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID}")
  message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
else()
  message(STATUS "Qt6 not found - skipping enhanced compression tests")
endif()