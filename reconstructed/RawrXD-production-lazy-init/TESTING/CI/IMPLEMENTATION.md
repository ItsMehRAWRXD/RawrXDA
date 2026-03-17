# Testing & CI Implementation Guide

## Overview

This document describes the comprehensive testing infrastructure implemented for RawrXD IDE, including regression tests, integration tests, UI tests, and CI/CD automation.

## Test Suites

### 1. Readiness Checker Regression Tests

**File:** `tests/unit/test_readiness_checker.cpp`

**Purpose:** Validate production readiness with mocked network, ports, and disk dependencies.

**Test Coverage:**
- ✅ Network connectivity checks with mocked endpoints
- ✅ Port availability validation with mock TCP servers
- ✅ Disk space verification with mocked filesystem queries
- ✅ Model availability checks
- ✅ Dependency validation (libraries and executables)
- ✅ Performance benchmarks (check completion time)
- ✅ Error aggregation and reporting
- ✅ Edge case handling (exact minimum space, multiple failures)

**Key Features:**
- Fully mocked external dependencies (no actual network/disk I/O in tests)
- Dependency injection pattern for testability
- Comprehensive error message validation
- Performance assertions (< 100ms for mocked checks)

**Run Tests:**
```bash
# CMake target
cmake --build build --target readiness-tests

# Direct execution
./build/tests/unit/test_readiness_checker

# With CTest
cd build && ctest -R readiness -V
```

**Example Test:**
```cpp
TEST_F(ReadinessCheckerTest, NetworkCheck_EndpointUnreachable_ReturnsNotReady) {
    TestableReadinessChecker checker(config);
    
    // Mock: endpoint unreachable
    checker.mock_endpoint_check = [](const std::string& endpoint) {
        return endpoint != "http://localhost:11434/api/tags";
    };
    
    auto result = checker.performReadinessCheck();
    
    EXPECT_FALSE(result.ready);
    EXPECT_THAT(result.errors[0], HasSubstr("Network endpoint unreachable"));
}
```

### 2. MASM Build Integration Tests

**File:** `tests/integration/test_masm_build_integration.cpp`

**Purpose:** Test MASM build process and Problems panel error parsing.

**Test Coverage:**
- ✅ Valid MASM program compilation
- ✅ Syntax error detection and reporting
- ✅ Undefined symbol handling
- ✅ Multiple error reporting
- ✅ Problems panel integration
- ✅ Error message parsing (filename, line, code, message)
- ✅ Filter by file/severity
- ✅ Linking tests (valid and undefined symbols)
- ✅ Multi-file project builds
- ✅ Performance benchmarks (build time < 5s for simple programs)

**Key Features:**
- Real MASM compiler integration (ml64.exe)
- Temporary directory fixture for isolated testing
- Full error parsing with regex patterns
- Problems panel mock for UI integration testing
- Build → Fix → Rebuild workflow validation

**Run Tests:**
```bash
# CMake target
cmake --build build --target masm-build-tests

# Direct execution
./build/tests/integration/test_masm_build_integration

# With CTest
cd build && ctest -R masm -V
```

**Example Test:**
```cpp
TEST_F(MasmBuildTest, ProblemsPanel_ParsesErrorsCorrectly) {
    std::string source = R"(
.code
main PROC
    mov rax ; Missing operand - line 4
    ret
main ENDP
END
)";
    
    auto result = build_system->build(source_file, object_file);
    
    problems_panel->clearProblems();
    problems_panel->addProblemsFromBuildResult(result, "masm");
    
    auto problems = problems_panel->getProblems();
    
    EXPECT_FALSE(problems.empty());
    EXPECT_EQ(problems[0].severity, "error");
    EXPECT_THAT(problems[0].file, HasSubstr("error.asm"));
}
```

### 3. UI Tests (Project Explorer & Hotpatch)

**File:** `tests/ui/test_project_explorer_hotpatch.cpp`

**Purpose:** Test UI components for Project Explorer operations and hotpatch workflows.

**Test Coverage:**

**Project Explorer:**
- ✅ Load project from directory
- ✅ Display directory tree structure
- ✅ Create new files
- ✅ Create new folders
- ✅ Delete files and folders
- ✅ Rename items
- ✅ Get selected files
- ✅ Double-click to open files
- ✅ Context menu operations
- ✅ Performance (load 500+ files < 2s)

**Hotpatch Manager:**
- ✅ Apply hotpatch to function
- ✅ Track active patches
- ✅ Check patch status
- ✅ Revert hotpatch
- ✅ Multiple patch management
- ✅ Selective revert
- ✅ Signal/slot integration
- ✅ Performance (apply 100 patches < 500ms)

**Key Features:**
- Qt Test framework integration
- QSignalSpy for signal validation
- Temporary directory fixtures
- Offscreen rendering for headless CI
- Real filesystem operations in isolated environment

**Run Tests:**
```bash
# CMake target
cmake --build build --target ui-workflow-tests

# Direct execution
QT_QPA_PLATFORM=offscreen ./build/tests/ui/test_project_explorer_hotpatch

# With CTest
cd build && ctest -R ui -V
```

**Example Test:**
```cpp
TEST_F(ProjectExplorerTest, CreateNewFile_ValidPath_CreatesFile) {
    explorer->loadProject(project_path);
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::fileCreated);
    
    bool result = explorer->createNewFile(project_path + "/src", "newfile.asm");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(QFile::exists(project_path + "/src/newfile.asm"));
}
```

## CI/CD Pipeline

**File:** `.github/workflows/ci-cd.yml`

### Pipeline Jobs

#### 1. Unit Tests
- Runs on: Windows, Linux
- Includes: Readiness checker tests
- Timeout: 60 seconds
- Artifacts: Test results XML

#### 2. Integration Tests
- Runs on: Windows (requires MASM)
- Includes: MASM build tests
- Timeout: 300 seconds (5 minutes)
- Artifacts: Test results XML

#### 3. UI Tests
- Runs on: Windows
- Includes: Project Explorer, Hotpatch tests
- Runs headless with offscreen rendering
- Timeout: 120 seconds
- Artifacts: Test results XML

#### 4. Code Coverage
- Runs on: Linux
- Generates: lcov coverage report
- Uploads to: Codecov
- Artifacts: HTML coverage report

#### 5. Test Summary
- Aggregates all test results
- Generates GitHub Actions summary
- Shows pass/fail status for all suites

#### 6. Build Release
- Only runs if all tests pass
- Builds full application
- Creates deployment package
- Uploads artifacts

### Triggering CI

```bash
# Automatic triggers
git push origin main          # Runs full pipeline
git push origin develop       # Runs full pipeline

# Manual trigger
# Go to GitHub Actions → CI/CD → Run workflow

# PR trigger
# Opens automatically when PR is created
```

### Viewing Results

```bash
# GitHub UI
# Navigate to: Actions → CI/CD → [latest run]

# Download artifacts
gh run download [run-id] -n unit-test-results-windows

# View test summary
# Automatically shown in GitHub Actions UI
```

## Running Tests Locally

### Prerequisites

```bash
# Install Google Test
git clone https://github.com/google/googletest.git
cd googletest
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
cmake --install build --prefix /usr/local  # or C:/gtest on Windows
```

### Build Configuration

```bash
# Configure with tests enabled
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTS=ON \
  -DENABLE_PRODUCTION_TESTS=ON \
  -DENABLE_READINESS_TESTS=ON \
  -DENABLE_MASM_BUILD_TESTS=ON \
  -DENABLE_UI_WORKFLOW_TESTS=ON

# Build all tests
cmake --build build --config Release

# Or build specific test suite
cmake --build build --target readiness-tests
cmake --build build --target masm-build-tests
cmake --build build --target ui-workflow-tests
```

### Run Tests

```bash
# All tests
cd build && ctest --output-on-failure --verbose

# Specific label
ctest -L unit          # Unit tests only
ctest -L integration   # Integration tests only
ctest -L ui            # UI tests only
ctest -L production    # All production readiness tests

# Specific test by name
ctest -R readiness -V
ctest -R masm -V

# With custom targets
cmake --build build --target production-tests
```

### Run with Coverage

```bash
# Configure with coverage
cmake -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_COVERAGE=ON \
  -DCMAKE_CXX_FLAGS="--coverage" \
  -DCMAKE_C_FLAGS="--coverage"

# Build and test
cmake --build build
cd build && ctest

# Generate coverage report
lcov --capture --directory . --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/tests/*' '*/3rdparty/*' --output-file coverage_filtered.info
genhtml coverage_filtered.info --output-directory coverage_report

# View report
firefox coverage_report/index.html  # Linux
start coverage_report/index.html    # Windows
```

## Test Configuration Options

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `BUILD_TESTS` | ON | Build test suites |
| `ENABLE_PRODUCTION_TESTS` | ON | Enable production readiness tests |
| `ENABLE_READINESS_TESTS` | ON | Enable readiness checker tests |
| `ENABLE_MASM_BUILD_TESTS` | ON | Enable MASM build integration tests |
| `ENABLE_UI_WORKFLOW_TESTS` | ON | Enable UI workflow tests |
| `ENABLE_COVERAGE` | OFF | Enable code coverage |

### Environment Variables

```bash
# UI tests headless mode
export QT_QPA_PLATFORM=offscreen

# Test timeout (seconds)
export DART_TESTING_TIMEOUT=300

# CTest output
export CTEST_OUTPUT_ON_FAILURE=1

# Google Test options
export GTEST_COLOR=yes
export GTEST_OUTPUT=xml:test-results.xml
```

## Test Development Guidelines

### Writing New Tests

1. **Choose the appropriate category:**
   - Unit tests: `tests/unit/`
   - Integration tests: `tests/integration/`
   - UI tests: `tests/ui/`

2. **Follow naming conventions:**
   - Test files: `test_<component>_<feature>.cpp`
   - Test fixtures: `<Component>Test`
   - Test cases: `<Method>_<Scenario>_<ExpectedBehavior>`

3. **Use mocks for external dependencies:**
   ```cpp
   class TestableComponent : public Component {
   public:
       std::function<bool()> mock_dependency;
   protected:
       bool callDependency() override {
           return mock_dependency ? mock_dependency() : Component::callDependency();
       }
   };
   ```

4. **Add to CMakeLists.txt:**
   ```cmake
   add_executable(test_new_feature
       unit/test_new_feature.cpp
   )
   
   target_link_libraries(test_new_feature
       PRIVATE gtest gtest_main Qt5::Core
   )
   
   gtest_discover_tests(test_new_feature
       PROPERTIES LABELS "unit"
   )
   ```

### Best Practices

✅ **DO:**
- Mock external dependencies (network, filesystem, processes)
- Use fixtures for common setup/teardown
- Test both success and failure paths
- Include performance benchmarks where applicable
- Write descriptive test names
- Add comments for complex test logic
- Use EXPECT over ASSERT when possible (continues testing)
- Clean up resources in TearDown()

❌ **DON'T:**
- Make network calls in unit tests
- Depend on specific file locations
- Use hardcoded timeouts (use configuration)
- Leave test files behind after execution
- Skip error case testing
- Write tests that depend on execution order
- Use sleep() for synchronization (use proper signaling)

## Troubleshooting

### Common Issues

**Issue: Test timeout**
```bash
# Increase timeout in CMakeLists.txt
set_property(TEST test_name PROPERTY TIMEOUT 600)

# Or via environment
export DART_TESTING_TIMEOUT=600
```

**Issue: Qt platform plugin not found**
```bash
# Set platform to offscreen
export QT_QPA_PLATFORM=offscreen

# Or install platform plugins
sudo apt-get install qt5-qmltooling-plugins
```

**Issue: MASM not found**
```bash
# Ensure Visual Studio is installed with C++ tools
# Or add ml64.exe to PATH
set PATH=%PATH%;C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC\[version]\bin\Hostx64\x64
```

**Issue: Google Test not found**
```bash
# Install Google Test
cmake --build googletest/build --target install

# Or set GTest_DIR
cmake -DGTest_DIR=/path/to/gtest/lib/cmake/GTest
```

## Performance Benchmarks

### Expected Test Times

| Test Suite | Expected Duration | Max Duration |
|------------|-------------------|--------------|
| Readiness Checker | < 5 seconds | 60 seconds |
| MASM Build Integration | < 30 seconds | 300 seconds |
| UI Workflow Tests | < 20 seconds | 120 seconds |
| All Tests | < 1 minute | 10 minutes |

### Performance Optimization

If tests are running slow:

1. **Use mocks aggressively** - Avoid real I/O
2. **Parallelize with CTest** - `ctest -j8`
3. **Cache dependencies** - Qt, Google Test
4. **Use RAM disk** - For temporary test files
5. **Profile tests** - `gtest --gtest_filter=Slow*`

## Maintenance

### Regular Tasks

- [ ] Review test coverage monthly (target: > 80%)
- [ ] Update CI dependencies quarterly
- [ ] Add tests for all new features
- [ ] Refactor slow tests (> 5s per test)
- [ ] Update documentation when tests change
- [ ] Monitor CI pipeline performance
- [ ] Clean up obsolete tests

### Updating Tests

When modifying production code:

1. Run affected tests locally first
2. Update tests if behavior changes
3. Add new tests for new functionality
4. Ensure CI passes before merging
5. Update documentation if interfaces change

## References

- [Google Test Documentation](https://google.github.io/googletest/)
- [Qt Test Framework](https://doc.qt.io/qt-5/qtest.html)
- [CMake Testing](https://cmake.org/cmake/help/latest/manual/ctest.1.html)
- [GitHub Actions](https://docs.github.com/en/actions)
- [Production Readiness Instructions](../.aitk/instructions/tools.instructions.md)

## Summary

✅ **Implemented:**
- Readiness checker regression tests with mocked dependencies
- MASM build integration tests with Problems panel parsing
- UI tests for Project Explorer and hotpatch workflows
- Comprehensive CI/CD pipeline with GitHub Actions
- CMake test configuration with multiple targets
- Test documentation and guidelines

✅ **Coverage:**
- Network connectivity validation
- Port availability checks
- Disk space monitoring
- MASM compilation and linking
- Error message parsing
- UI component operations
- Hotpatch workflows
- Performance benchmarks

✅ **Automation:**
- Automatic CI on push/PR
- Parallel test execution
- Test result aggregation
- Coverage reporting
- Artifact publishing
- Release building on success

The testing infrastructure is production-ready and follows best practices for observability, isolation, and maintainability.
