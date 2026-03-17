# RawrXD Marketplace Installer Test Suite

Comprehensive test suite for the MASM64 Extension Marketplace Installer.

## Test Coverage

### Unit Tests (11 tests)
1. **ParseVsixHeader_ValidFile** - Tests parsing valid VSIX package headers
2. **ParseVsixHeader_InvalidFile** - Tests error handling for invalid files
3. **ParseManifest_ValidJson** - Tests parsing valid package.json manifests
4. **ParseManifest_InvalidJson** - Tests error handling for malformed JSON
5. **ResolveDependencies_None** - Tests extensions with no dependencies
6. **ResolveDependencies_Single** - Tests single dependency resolution
7. **ExtractFiles_Valid** - Tests file extraction from VSIX packages
8. **GenerateNativeEntryPoint** - Tests MASM64 code generation
9. **RegisterExtension** - Tests registry operations
10. **InstallExtension_Complete** - Tests complete installation flow
11. **InstallExtension_AlreadyInstalled** - Tests duplicate installation handling

### Integration Tests (1 test)
12. **Integration_FullPipeline** - Tests complete end-to-end installation pipeline

### Stress Tests (1 test)
13. **Stress_MultipleInstalls** - Tests installing 10 extensions sequentially

## Building and Running

### Option 1: Batch Script (Windows)
```batch
build_tests.bat
```

### Option 2: Node.js Runner (Recommended)
```bash
# Install dependencies
npm install

# Run all tests
node run_tests.js

# Build only
node run_tests.js --build-only

# Clean artifacts
node run_tests.js --clean
```

### Option 3: Manual Build
```batch
REM Assemble
ml64.exe /c /Zi tests\test_marketplace_installer.asm

REM Link
link.exe /OUT:tests\test_marketplace_installer.exe ^
    tests\test_marketplace_installer.obj ^
    src\agentic\marketplace_installer.obj ^
    kernel32.lib user32.lib msvcrt.lib

REM Run
tests\test_marketplace_installer.exe
```

## Test Structure

```
tests/
├── test_marketplace_installer.asm  # Main test suite
├── fixtures/                       # Test fixtures
│   └── test-extension.vsix        # Mock VSIX package
├── output/                         # Test output directory
└── test-report.json               # Generated test report
```

## Test Fixtures

The test suite automatically creates mock VSIX packages with:
- Valid VSIX signature
- Sample package.json manifest
- Mock extension files

## Expected Output

```
========================================
Test Suite: Marketplace Installer Test Suite
========================================

[TEST] Starting: ParseVsixHeader_ValidFile
[PASS] ParseVsixHeader_ValidFile (12.34ms)

[TEST] Starting: ParseVsixHeader_InvalidFile
[PASS] ParseVsixHeader_InvalidFile (5.67ms)

...

========================================
Total: 13 | Passed: 13 | Failed: 0
========================================
```

## Test Framework Functions

### Test_InitSuite
Initializes test suite with name and allocates result storage.

### Test_Run
Executes a single test function and records results.

### Test_AssertEqual
Asserts two values are equal.

### Test_AssertNotNull
Asserts a pointer is not null.

### Test_AssertTrue
Asserts a condition is true.

### Test_FinalizeSuite
Prints summary and returns exit code.

## Adding New Tests

1. Create test function following naming convention:
```asm
Test_YourTestName proc frame uses rbx
    ; Setup
    ; Execute
    ; Assert
    mov eax, TEST_PASS  ; or TEST_FAIL
    ret
Test_YourTestName endp
```

2. Add to Test_RunAll:
```asm
lea rcx, szTestName
lea rdx, Test_YourTestName
call Test_Run
```

3. Add test name string:
```asm
szTestName db 'YourTestName',0
```

## Continuous Integration

### GitHub Actions Example
```yaml
name: Test Marketplace Installer

on: [push, pull_request]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - name: Setup MASM
        run: |
          # Setup Visual Studio tools
      - name: Run Tests
        run: node run_tests.js
```

## Troubleshooting

### Build Errors
- Ensure Visual Studio 2022 is installed
- Check ml64.exe and link.exe are in PATH
- Verify Windows SDK is installed

### Test Failures
- Check test fixtures exist in `tests/fixtures/`
- Ensure write permissions for `tests/output/`
- Review test output for specific error messages

### Missing Dependencies
```bash
npm install
```

## Performance Benchmarks

Expected execution times (on typical hardware):
- Unit tests: ~100-200ms total
- Integration test: ~500ms
- Stress test: ~2-5 seconds

## Code Coverage

The test suite covers:
- ✅ VSIX parsing (header, manifest, files)
- ✅ Dependency resolution
- ✅ File extraction and decompression
- ✅ Native code generation
- ✅ Registry operations
- ✅ Error handling
- ✅ Edge cases (duplicates, invalid input)

## License

MIT License - See LICENSE file for details
