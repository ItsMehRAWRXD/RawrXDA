# Agentic Framework Tests
# Minimal test configuration - tests will be added incrementally

# Placeholder test - can be replaced with real tests
add_executable(test_agentic_smoke
    smoke_test.cpp
)

target_link_libraries(test_agentic_smoke
    RawrXD-Agentic
)

add_executable(test_sentinel_stress
    SentinelTest.cpp
)

target_link_libraries(test_sentinel_stress
    RawrXD-Common
    RawrXD-Agentic
    RawrXD-Win32IDE # Linked to include all shims and MASM symbols
)

add_test(NAME sentinel_stress COMMAND test_sentinel_stress)

add_test(NAME agentic_smoke COMMAND test_agentic_smoke)
