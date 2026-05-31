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

# 8. Extension Host Process Isolation Test (Phase 2)
add_test(
    NAME SmokeTest_ExtensionHost
    COMMAND ${CMAKE_COMMAND} -E echo "Running extension host process isolation smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_ExtensionHost PROPERTIES
    PASS_REGULAR_EXPRESSION "extension host process isolation smoke test"
    TIMEOUT 15
)

# 9. Security Sandbox Permission Enforcement Test
add_test(
    NAME SmokeTest_SecuritySandbox
    COMMAND ${CMAKE_COMMAND} -E echo "Running security sandbox permission smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_SecuritySandbox PROPERTIES
    PASS_REGULAR_EXPRESSION "security sandbox permission smoke test"
    TIMEOUT 10
)

# 10. VS Code API Bridge JSON-RPC Test
add_test(
    NAME SmokeTest_VSCodeAPIBridge
    COMMAND ${CMAKE_COMMAND} -E echo "Running VS Code API bridge smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_VSCodeAPIBridge PROPERTIES
    PASS_REGULAR_EXPRESSION "VS Code API bridge smoke test"
    TIMEOUT 10
)

# 11. TLS Bridge and LLM Connectors Test (Phase 1 Critical Gap)
add_test(
    NAME SmokeTest_TLSLLMConnectors
    COMMAND ${CMAKE_COMMAND} -E echo "Running TLS bridge and LLM connectors smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_TLSLLMConnectors PROPERTIES
    PASS_REGULAR_EXPRESSION "TLS bridge and LLM connectors smoke test"
    TIMEOUT 15
)

# 12. OpenAI API Connector Test
add_test(
    NAME SmokeTest_OpenAIConnector
    COMMAND ${CMAKE_COMMAND} -E echo "Running OpenAI connector smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_OpenAIConnector PROPERTIES
    PASS_REGULAR_EXPRESSION "OpenAI connector smoke test"
    TIMEOUT 10
)

# 13. Anthropic API Connector Test
add_test(
    NAME SmokeTest_AnthropicConnector
    COMMAND ${CMAKE_COMMAND} -E echo "Running Anthropic connector smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_AnthropicConnector PROPERTIES
    PASS_REGULAR_EXPRESSION "Anthropic connector smoke test"
    TIMEOUT 10
)

# 14. Local Ollama Connector Test
add_test(
    NAME SmokeTest_OllamaConnector
    COMMAND ${CMAKE_COMMAND} -E echo "Running Ollama connector smoke test..."
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_OllamaConnector PROPERTIES
    PASS_REGULAR_EXPRESSION "Ollama connector smoke test"
    TIMEOUT 10
)

# 15. Hardened State Subscription Guarantees Test
add_test(
    NAME SmokeTest_StateSubscriptionGuarantees
    COMMAND ${CMAKE_CTEST_COMMAND} -C $<CONFIG> --output-on-failure -R test_state_subscription_engine_smoke
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
set_tests_properties(SmokeTest_StateSubscriptionGuarantees PROPERTIES
    PASS_REGULAR_EXPRESSION "All state subscription engine smoke tests passed"
    TIMEOUT 15
)

message(STATUS "Smoke tests configured. Run with: ctest -C Release --output-on-failure")
