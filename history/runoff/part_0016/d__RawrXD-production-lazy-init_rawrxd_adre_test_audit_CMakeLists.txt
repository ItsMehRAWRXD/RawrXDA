add_executable(test_audit test_audit.cpp)
target_link_libraries(test_audit core ckg agents orchestrator replay integration)
