# AgentHotPatcher Test Suite - Maintenance Guide

## Overview

This guide provides comprehensive instructions for maintaining, expanding, and troubleshooting the AgentHotPatcher test suite to ensure long-term reliability and effectiveness.

## Maintenance Schedule

### Daily (Development Phase)
- [ ] Run unit tests before committing changes
- [ ] Verify integration tests pass
- [ ] Check for performance regressions

### Weekly
- [ ] Run complete test suite
- [ ] Review test coverage reports
- [ ] Update test data if needed

### Monthly
- [ ] Performance benchmarking
- [ ] Security review of test data
- [ ] Documentation updates

### Quarterly
- [ ] Full test suite review
- [ ] Update for Qt version changes
- [ ] Add new test scenarios

## Test Maintenance Procedures

### 1. Adding New Test Cases

#### Step 1: Identify Test Scenario
```cpp
// Example: Adding test for new hallucination type
void TestAgentHotPatcher::testNewHallucinationType()
{
    QString content = "Content with new hallucination pattern";
    HallucinationDetection detection = m_patcher->detectHallucination(content, QJsonObject());
    QVERIFY(!detection.detectionId.isEmpty());
    QCOMPARE(detection.hallucinationType, QString("new_type"));
}
```

#### Step 2: Update CMake Configuration
```cmake
# Ensure new test file is included in build
add_executable(test_agent_hot_patcher
    test_agent_hot_patcher.cpp
    new_test_scenarios.cpp  # Add new file
    src/agent/agent_hot_patcher.cpp
)
```

#### Step 3: Update Documentation
- Add new test description to README
- Update test coverage matrix
- Document any new dependencies

### 2. Updating Existing Tests

#### When AgentHotPatcher Interface Changes
```cpp
// Before: Old interface
void testOldMethod() {
    m_patcher->oldMethod(param);
}

// After: Updated interface
void testNewMethod() {
    m_patcher->newMethod(param, context);
}
```

#### When Data Structures Change
```cpp
// Update test data to match new structure
HallucinationDetection detection;
detection.newField = "updated_value";  // Add new field
// ... existing fields ...
```

### 3. Performance Optimization

#### Monitor Test Execution Time
```bash
# Time individual tests
time ./test_agent_hot_patcher testPerformance

# Profile with gprof
gprof ./test_agent_hot_patcher gmon.out > performance_report.txt
```

#### Optimize Slow Tests
```cpp
// Before: Slow pattern matching
for (int i = 0; i < 10000; i++) {
    // Expensive operation
}

// After: Optimized approach
QHash<QString, QString> patternCache;  // Use caching
// ... optimized logic ...
```

## Troubleshooting Guide

### Common Issues and Solutions

#### Issue 1: Tests Fail After Code Changes

**Symptoms**:
- Compilation errors
- Runtime assertion failures
- Unexpected test results

**Solutions**:
1. Check interface compatibility
2. Verify test data matches current implementation
3. Review signal/slot connections
4. Check Qt meta-type registrations

#### Issue 2: Performance Degradation

**Symptoms**:
- Test execution time increases
- Memory usage spikes
- Timeout failures

**Solutions**:
1. Profile with `valgrind --tool=callgrind`
2. Optimize pattern matching algorithms
3. Implement caching where appropriate
4. Review thread safety measures

#### Issue 3: Intermittent Test Failures

**Symptoms**:
- Tests pass/fail randomly
- Race conditions in multi-threaded tests
- Timing-related issues

**Solutions**:
1. Add `QTest::qWait()` for timing synchronization
2. Use `QMutexLocker` for thread-safe operations
3. Implement retry logic for flaky tests
4. Add more specific assertions

### Debugging Procedures

#### Enable Detailed Logging
```cpp
// In test setup
m_patcher->setDebugLogging(true);

// Or set environment variable
export QT_LOGGING_RULES="agent.hotpatcher.debug=true"
```

#### Use Qt Test Debug Features
```bash
# Run with verbose output
./test_agent_hot_patcher -v2

# Run specific test with debug
./test_agent_hot_patcher testHallucinationDetection -v2
```

#### Memory Leak Detection
```bash
# Valgrind memory check
valgrind --leak-check=full ./test_agent_hot_patcher

# AddressSanitizer (if supported)
./test_agent_hot_patcher_asan  # Built with -fsanitize=address
```

## Test Data Management

### Safe Test Data Practices

#### Never Use Real Sensitive Data
```cpp
// ❌ Dangerous
QString realEmail = "user@realcompany.com";

// ✅ Safe
QString testEmail = "test@example.com";
QString ssnPattern = "***-**-****";
```

#### Maintain Test Data Consistency
```cpp
// Centralized test data
namespace TestData {
    const QString INVALID_PATH = "/mystical/path";
    const QString CONTRADICTION = "always succeeds but always fails";
    // ... other test patterns ...
}
```

### Test Data Versioning

```json
{
    "test_data_version": "1.2.0",
    "compatible_with": "AgentHotPatcher v3.1+",
    "patterns": {
        "path_hallucinations": ["/mystical/path", "/phantom/dir"],
        "logic_contradictions": ["always succeeds but always fails"]
    }
}
```

## Security Considerations

### Input Validation in Tests
```cpp
// Validate test inputs
void testMaliciousInput() {
    QString maliciousContent = "../../etc/passwd";  // Path traversal
    // Ensure patcher handles this safely
    HallucinationDetection detection = m_patcher->detectHallucination(maliciousContent, QJsonObject());
    // Verify no security issues
}
```

### Secure Test Execution
- Run tests in isolated environments
- Use test-specific configuration files
- Avoid network access in unit tests
- Clean up test artifacts properly

## Integration with Development Workflow

### Git Hooks for Automated Testing

#### Pre-commit Hook
```bash
#!/bin/bash
# .git/hooks/pre-commit

# Run quick test suite before commit
if ! ./build_tests.sh --quick; then
    echo "Tests failed! Fix issues before committing."
    exit 1
fi
```

#### Pre-push Hook
```bash
#!/bin/bash
# .git/hooks/pre-push

# Run full test suite before pushing
if ! ./build_tests.sh --full; then
    echo "Full test suite failed! Fix issues before pushing."
    exit 1
fi
```

### Continuous Integration Pipeline

```yaml
# GitHub Actions configuration
name: AgentHotPatcher CI

on:
  push:
    branches: [ main, develop ]
  pull_request:
    branches: [ main ]

jobs:
  test:
    runs-on: ubuntu-latest
    steps:
    - uses: actions/checkout@v3
    - name: Setup Qt6
      run: sudo apt-get install qt6-base-dev
    - name: Run Tests
      run: ./build_tests.sh
    - name: Upload Results
      uses: actions/upload-artifact@v3
      with:
        name: test-results
        path: test_reports/
```

## Metrics and Reporting

### Test Coverage Tracking

```bash
# Generate coverage reports
gcovr --html --html-details -o coverage_report.html

# Track coverage over time
# Should maintain > 90% coverage
```

### Performance Metrics

```json
{
    "test_metrics": {
        "execution_time": "< 100ms",
        "memory_usage": "< 50MB",
        "test_coverage": "95%",
        "last_run": "2025-12-05T10:30:00Z"
    }
}
```

## Emergency Procedures

### Test Suite Failure

1. **Immediate Actions**:
   - Isolate failing tests
   - Revert recent changes if needed
   - Notify team

2. **Investigation**:
   - Check recent code changes
   - Review test data modifications
   - Verify environment consistency

3. **Resolution**:
   - Fix root cause
   - Update tests if interface changed
   - Document lessons learned

### Performance Crisis

1. **Identify Bottleneck**:
   - Profile test execution
   - Check for memory leaks
   - Review algorithm complexity

2. **Optimization**:
   - Implement caching
   - Parallelize where possible
   - Reduce test data size

## Conclusion

This maintenance guide ensures the AgentHotPatcher test suite remains reliable, secure, and effective throughout the development lifecycle. Regular maintenance and proactive monitoring are essential for long-term success.

**Remember**: Well-maintained tests are the foundation of production-ready software.

## License

Production Grade - Enterprise Ready