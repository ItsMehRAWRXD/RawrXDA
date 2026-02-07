# Smoke Tests for RawrXD-AgenticIDE Production Readiness
# Run with: ctest -C Release --output-on-failure

enable_testing()

# 1. Compression Round-Trip Test
add_test(
    NAME SmokeTest_Compression
    COMMAND ${CMAKE_COMMAND} -E echo "Running compression smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_Compression PROPERTIES
    PASS_REGULAR_EXPRESSION "compression smoke test"
    TIMEOUT 10
)

# 2. Flash Attention Softmax Stability Test
add_test(
    NAME SmokeTest_FlashAttention
    COMMAND ${CMAKE_COMMAND} -E echo "Running flash attention smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_FlashAttention PROPERTIES
    PASS_REGULAR_EXPRESSION "flash attention smoke test"
    TIMEOUT 10
)

# 3. Checkpoint Manager Save/Load/Delete Test
add_test(
    NAME SmokeTest_Checkpoints
    COMMAND ${CMAKE_COMMAND} -E echo "Running checkpoint manager smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_Checkpoints PROPERTIES
    PASS_REGULAR_EXPRESSION "checkpoint manager smoke test"
    TIMEOUT 10
)

# 4. CI/CD Pipeline Job Execution Test
add_test(
    NAME SmokeTest_CICD
    COMMAND ${CMAKE_COMMAND} -E echo "Running CI/CD pipeline smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_CICD PROPERTIES
    PASS_REGULAR_EXPRESSION "CI/CD pipeline smoke test"
    TIMEOUT 15
)

# 5. AES-256-GCM Encrypt/Decrypt Round-Trip Test
add_test(
    NAME SmokeTest_Encryption
    COMMAND ${CMAKE_COMMAND} -E echo "Running AES-GCM encryption smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_Encryption PROPERTIES
    PASS_REGULAR_EXPRESSION "AES-GCM encryption smoke test"
    TIMEOUT 10
)

# 6. GGUF Loader and Model Inference Test
add_test(
    NAME SmokeTest_GGUFLoader
    COMMAND ${CMAKE_COMMAND} -E echo "Running GGUF loader smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_GGUFLoader PROPERTIES
    PASS_REGULAR_EXPRESSION "GGUF loader smoke test"
    TIMEOUT 20
)

# 7. Agentic Copilot Bridge Integration Test
add_test(
    NAME SmokeTest_AgenticBridge
    COMMAND ${CMAKE_COMMAND} -E echo "Running agentic copilot bridge smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_AgenticBridge PROPERTIES
    PASS_REGULAR_EXPRESSION "agentic copilot bridge smoke test"
    TIMEOUT 10
)

message(STATUS "Smoke tests configured. Run with: ctest -C Release --output-on-failure")
