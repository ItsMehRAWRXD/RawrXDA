# RawrXD PRODUCTION READINESS ACTION PLAN
## Week-by-Week Implementation Roadmap

**Target**: Production Ready by April 20, 2026  
**Team Size**: 3-4 developers  
**Total Effort**: 504 hours (~12 weeks, or 6-8 weeks with 3-4 person team)

---

## PHASE 1: SECURITY HARDENING (Weeks 1-2) 
**Effort**: 80 hours | **Team**: 2 developers | **Owner**: Security Lead

### Week 1: API Security Foundation

#### Task 1.1.1: Add TLS/HTTPS Support
- [ ] **Research**: Evaluate Windows Schannel vs OpenSSL
  - *Time*: 2 hours
  - *Owner*: Security Lead
  - *Files to modify*: `src/api_server.cpp`, `src/net_impl_win32.h`
  
- [ ] **Implement**: Add HTTPS endpoint
  ```cpp
  // src/net_impl_win32.h - ADD AFTER Line 40
  namespace WinHttps {
      HINTERNET CreateSecureConnection(LPWSTR hostname, INT port);
      bool ValidateServerCertificate(HINTERNET hConnect);
  }
  ```
  - *Time*: 12 hours
  - *Acceptance Criteria*:
    - POSTing to https://localhost:11434/api/chat works
    - Certificate validation passes
    - TLS 1.3 only (reject TLS 1.0-1.2)

- [ ] **Testing**: Security tests
  - *Time*: 3 hours
  - *Test File*: `test/test_api_security.cpp` (CREATE NEW)
  ```cpp
  TEST(ApiSecurityTest, HTTPSRequired) {
      EXPECT_FALSE(ApiServer::AllowHTTP());
  }
  TEST(ApiSecurityTest, TLS13Only) {
      EXPECT_EQ(ApiServer::MinVersion(), TLS_1_3);
  }
  TEST(ApiSecurityTest, CertificateValidation) {
      EXPECT_TRUE(ValidateCertificate(invalid_cert));
  }
  ```

#### Task 1.1.2: Input Validation on POST Endpoints
- [ ] **Audit**: Document all POST endpoint parameters
  - *Time*: 2 hours
  - *Output*: `docs/API_INPUT_VALIDATION_SPEC.md`
  - *Checklist*:
    - [ ] POST /api/generate - validate prompt, max_tokens, temperature
    - [ ] POST /v1/chat/completions - validate messages array
    - [ ] POST /api/read-file - validate file path (NO DIRECTORY TRAVERSAL)
    - [ ] POST /api/pull - validate model name

- [ ] **Implement**: Add validation functions
  - *Time*: 8 hours
  - *Location*: `src/api_validation.cpp` (CREATE NEW)
  ```cpp
  class APIValidator {
      static bool ValidateFilePath(const std::string& path) {
          // Must be within allowed directory
          auto canonical = fs::canonical(path);
          auto allowed = fs::canonical(GetAllowedBasePath());
          return canonical.string().find(allowed.string()) == 0;
      }
      
      static bool ValidatePrompt(const std::string& prompt) {
          return prompt.length() > 0 && prompt.length() <= 10000;
      }
      
      static bool ValidateModelName(const std::string& name) {
          // Only alphanumeric, dash, underscore
          return std::regex_match(name, std::regex("^[a-zA-Z0-9_-]+$"));
      }
  };
  ```
  
- [ ] **Testing**: Injection tests
  - *Time*: 4 hours
  - *Test cases*:
    - [ ] Directory traversal blocked: `../../etc/passwd`
    - [ ] SQL injection escaped: `'; DROP TABLE--`
    - [ ] Path traversal blocked: `C:\Windows\System32`
    - [ ] Oversized prompts rejected

#### Task 1.1.3: API Authentication (Bearer Tokens)
- [ ] **Design**: Authentication strategy
  - *Time*: 3 hours
  - *Decision*: Implement bearer token in Authorization header
  
- [ ] **Implementation**: Add auth middleware
  - *Time*: 10 hours
  - *File*: `src/api_auth.cpp` (CREATE NEW)
  ```cpp
  class APIAuthenticator {
      std::string GenerateToken(const std::string& secret) {
          // Use HMAC-SHA256 to sign requests
      }
      
      bool ValidateToken(const std::string& token, 
                        const std::string& secret) {
          // Verify HMAC signature
      }
  };
  ```
  - *Side Effect*: All POST calls now require: `Authorization: Bearer <token>`

- [ ] **Testing**: Auth tests (5 hours)
  ```cpp
  TEST(APIAuthTest, ValidTokenAccepted) { }
  TEST(APIAuthTest, InvalidTokenRejected) { }
  TEST(APIAuthTest, ExpiredTokenRejected) { }
  ```

### Week 2: Application-Level Security

#### Task 1.2.1: CSRF Protection
- [ ] **Implementation**: Add CSRF tokens
  - *Time*: 5 hours
  - *Pattern*: 
    1. Client requests token from GET /api/csrf-token
    2. Client sends token in X-CSRF-Token header
    3. Server validates before processing POST

#### Task 1.2.2: Environment Variable Hardening
- [ ] **Audit**: Find all env var access
  - *Time*: 2 hours
  - ```bash
    grep -r "getenv\|GetEnvironmentVariable" src/
    ```
  - *Expected Files*:
    - `src/chat_panel_integration.cpp`
    - `src/headless_backend.cpp`
    - `src/cloud_api_client.cpp`

- [ ] **Secure Access**: Implement encrypted storage
  - *Time*: 8 hours
  - *File*: `src/secure_credentials.cpp` (CREATE NEW)
  ```cpp
  class SecureCredentials {
      static std::string GetEnvVar(const char* name) {
          // Don't use plaintext env vars
          // Instead use Windows DPAPI to decrypt from registry
          return DecryptFromRegistry(name);
      }
      
      static uint8_t* GetKey(const char* name) {
          // Use CNG to encrypt/decrypt sensitive data
          // HKEY_CURRENT_USER\Software\RawrXD\Secrets\{name}
      }
  };
  ```

#### Task 1.2.3: Rate Limiting
- [ ] **Implementation**: Add token bucket rate limiter
  - *Time*: 8 hours
  - *File*: `src/rate_limiter.cpp` (CREATE NEW)
  ```cpp
  class RateLimiter {
      std::unordered_map<std::string, TokenBucket> m_buckets;
      
      bool AllowRequest(const std::string& clientIP) {
          // 100 requests per minute per IP
          auto& bucket = m_buckets[clientIP];
          return bucket.TryConsume(1);
      }
  };
  ```

#### Task 1.2.4: Security Audit Report
- [ ] **Generate Report**: Document fixes
  - *Time*: 4 hours
  - *Output*: `SECURITY_AUDIT_FIXES_APPLIED.md`
  - *Coverage*:
    - [ ] Before/after code samples
    - [ ] Critical vulns addressed
    - [ ] Known remaining issues
    - [ ] Future hardening roadmap

---

## PHASE 2: IDE FEATURE COMPLETION (Weeks 1.5-3.5)
**Effort**: 100 hours | **Team**: 2 developers | **Owner**: IDE Lead

*Note: Run in parallel with Phase 1 Security work*

### Week 1.5: File Operations (Critical Blocker)

#### Task 2.1.1: File CRUD Operations  
- [ ] **Implement**: FileManager class
  - *Time*: 12 hours
  - *File*: `src/core/file_manager.cpp` (CREATE NEW)
  ```cpp
  class FileManager {
      bool CreateFile(const std::string& path);
      bool DeleteFile(const std::string& path);
      bool RenameFile(const std::string& oldPath, 
                      const std::string& newPath);
      bool MoveFile(const std::string& from, 
                    const std::string& to);
      bool CreateDirectory(const std::string& path);
      std::string ReadFile(const std::string& path);
      bool WriteFile(const std::string& path, 
                     const std::string& content);
  };
  ```
  - *Requirements*:
    - [ ] Atomic writes (write to .tmp, rename)
    - [ ] Error handling for all operations
    - [ ] Support UTF-8 + BOM detection
    - [ ] Preserve timestamps where possible

- [ ] **Wire to UI**: Project Explorer Context Menu
  - *Time*: 8 hours
  - *Location*: `src/win32app/project_explorer.cpp`
  - *Add Right-Click Menu Items*:
    - [ ] New File...
    - [ ] New Folder...
    - [ ] Delete...
    - [ ] Rename...
    - [ ] Reveal in Explorer

- [ ] **Testing**: File ops tests
  - *Time*: 5 hours
  - *Test File*: `test/test_file_operations.cpp` (CREATE NEW)
  ```cpp
  TEST(FileManagerTest, CreateAndDelete) {
      FileManager fm;
      EXPECT_TRUE(fm.CreateFile("test.txt"));
      EXPECT_TRUE(fm.DeleteFile("test.txt"));
  }
  TEST(FileManagerTest, AtomicWrite) {
      FileManager fm;
      EXPECT_TRUE(fm.WriteFile("atomic.txt", "data"));
      // Verify .tmp doesn't exist after
  }
  TEST(FileManagerTest, DirectoryTraversal) {
      FileManager fm;
      EXPECT_FALSE(fm.CreateFile("../../etc/passwd"));
  }
  ```

#### Task 2.1.2: Multi-File Project Support
- [ ] **Implement**: ProjectManager
  - *Time*: 8 hours
  - *File*: `src/core/project_manager.cpp` (CREATE NEW)
  - Features:
    - [ ] Detect project root (.git, CMakeLists.txt)
    - [ ] Load directory structure
    - [ ] .gitignore support (hide ignored files)
    - [ ] File tree lazy-loading for large projects

### Week 2: Find & Replace (Critical Blocker)

#### Task 2.2.1: Find Widget
- [ ] **Implement**: Find bar with Ctrl+F
  - *Time*: 10 hours
  - *File*: `src/win32app/find_widget.cpp` (CREATE NEW)
  ```cpp
  class FindWidget {
      void Find(const std::string& pattern);
      void FindNext();
      void FindPrevious();
      int GetMatchCount();
      
      bool m_caseSensitive;
      bool m_wholeWord;
      bool m_regex;
  };
  ```
  - Keyboard shortcut: Ctrl+F (current file), Ctrl+Shift+F (project)

#### Task 2.2.2: Replace Functionality
- [ ] **Implement**: Replace bar
  - *Time*: 8 hours
  - *Features*:
    - [ ] Replace current match
    - [ ] Replace all
    - [ ] Undo replace operation
    - [ ] Support regex capture groups (\1, \2)

- [ ] **Testing**: Search tests (6 hours)
  ```cpp
  TEST(FindWidgetTest, SimpleFind) {
      FindWidget fw;
      fw.SetDocument("hello world");
      fw.Find("world");
      EXPECT_EQ(fw.GetMatchCount(), 1);
  }
  TEST(FindWidgetTest, RegexFind) {
      FindWidget fw;
      fw.SetRegex(true);
      fw.Find(R"(\d{3}-\d{4})");  // Phone numbers
      EXPECT_GT(fw.GetMatchCount(), 0);
  }
  ```

### Week 2.5: Multi-Tab Editor Completion

#### Task 2.3.1: Missing Methods
- [ ] **Implement**: getCurrentText() method
  - *Time*: 4 hours
  - *File*: `src/multi_tab_editor.cpp`
  - Location: Add to MultiTabEditor class
  ```cpp
  std::string MultiTabEditor::getCurrentText() {
      if (m_currentTabIndex >= 0 && 
          m_currentTabIndex < m_tabs.size()) {
          return m_tabs[m_currentTabIndex]->GetText();
      }
      return "";
  }
  ```

- [ ] **Implement**: replace() method
  - *Time*: 6 hours
  - Complete find/replace implementation

#### Task 2.3.2: File Path Tracking
- [ ] **Implement**: Tab file associations
  - *Time*: 4 hours
  - Track which file each tab is editing
  - Enable "Save" instead of "Save As" for existing files

### Week 3: Settings & Chat

#### Task 2.4.1: Settings Dialog (Not Just Placeholder)
- [ ] **Implement**: Real settings dialog
  - *Time*: 8 hours
  - *File*: `src/win32app/settings_dialog.cpp`
  - Settings to persist:
    - [ ] Theme (light/dark)
    - [ ] Default model path
    - [ ] Default shell (PowerShell/cmd/bash)
    - [ ] Window geometry
    - [ ] Editor font & size
    - [ ] API endpoint
    - [ ] Rate limiting preferences

#### Task 2.4.2: Chat Interface Methods
- [ ] **Implement**: Missing methods
  - *Time*: 6 hours
  - [ ] displayResponse(const QString&) - append to chat
  - [ ] addMessage(sender, message) - add user/system message
  - [ ] focusInput() - focus message input field

---

## PHASE 3: COMPLETENESS AUDIT (Weeks 3-4.5)
**Effort**: 50 hours | **Team**: 1-2 developers | **Owner**: QA Lead

### Week 3: Stub Elimination

#### Task 3.1.1: Audit Stubbed Functions
- [ ] **Scan**: All function implementations
  ```bash
  grep -r "NOT IMPLEMENTED\|STUB\|TODO\|FIXME" src/ --include="*.cpp"
  ```
  - *Time*: 3 hours
  - *Output*: `STUBS_TO_FIX.md` listing all 50+ stubs

#### Task 3.1.2: Prioritize & Fix
- [ ] **High Priority** (Week 3):
  - Real agentic engine inference (not heuristic)
  - Hot-patcher implementation
  - File research functionality
  
- [ ] **Medium Priority** (Week 4):
  - Swarm orchestration
  - Self-correction logic
  - Enterprise license features

#### Task 3.1.3: Verification
- [ ] **Code Review**: Each component
  - *Time*: 20 hours
  - Does this component return real results or placeholders?
  - Can it be tested automatically?
  - Does it have error handling?

---

## PHASE 4: TESTING & QUALITY (Weeks 4-5.5)
**Effort**: 100 hours | **Team**: 2 developers | **Owner**: QA Lead

### Week 4: Unit Tests

#### Task 4.1.1: Create Test Suite
- [ ] **Unit Tests**: Create 200+ tests
  - *Target*: 85%+ code coverage
  - *Files to test*:
    - [ ] AgenticEngine (50 tests)
    - [ ] CPUInferenceEngine (40 tests)
    - [ ] GGUFLoader (50 tests)
    - [ ] APIServer (40 tests)
    - [ ] FileManager (20 tests)

- [ ] **Use CMake**: Add test target
  ```cmake
  enable_testing()
  add_subdirectory(test)
  
  add_test(NAME UnitTests 
           COMMAND ${CMAKE_CTEST_COMMAND} --verbose)
  ```

#### Task 4.1.2: AddressSanitizer Build
- [ ] **Add Build Config**
  - *Time*: 3 hours
  ```cmake
  if(MSVC AND ENABLE_ASAN)
      add_compile_options(/fsanitize=address)
      # MSVC 16.10+ required
  endif()
  ```

### Week 5: Integration & Performance Tests

#### Task 4.2.1: Integration Tests
- [ ] **Create**: End-to-end test scenarios
  - [ ] Load GGUF → Generate tokens → Verify output
  - [ ] Chat → Agentic analyze → Return result
  - [ ] File ops → Project structure → Search
  - [ ] API auth → Rate limiting → Response

#### Task 4.2.2: Performance Benchmarks
- [ ] **Measure & Optimize**:
  - [ ] Model load time (target: <5s)
  - [ ] Token generation (target: <10ms per token)
  - [ ] Concurrent requests (target: 20+)
  - [ ] Memory usage baseline

- [ ] **Optimization Tasks**:
  - [ ] Implement lazy tensor loading (8 hours)
  - [ ] Optimize tokenizer with SIMD (6 hours)
  - [ ] Thread pool for inference (5 hours)

---

## PHASE 5: DOCUMENTATION & RELEASE (Weeks 5.5-6.5)
**Effort**: 40 hours | **Team**: 1 developer + manager

### Week 5.5: Documentation

#### Task 5.1.1: API Reference
- [ ] **Generate Doxygen docs**
  ```bash
  doxygen Doxyfile
  ```
  - *Time*: 4 hours

#### Task 5.1.2: User Guide
- [ ] **Write**: Installation, configuration, usage
  - *Time*: 8 hours
  - Chapters:
    - [ ] Installation & setup
    - [ ] Creating your first project
    - [ ] Using the AI chat
    - [ ] Agentic features
    - [ ] Troubleshooting

#### Task 5.1.3: Security Policy
- [ ] **Document**: Security vulnerabilities & reporting
  - *Time*: 4 hours
  - File: `SECURITY.md`

### Week 6: Release Preparation

#### Task 5.2.1: Installation Package
- [ ] **Create MSI installer**
  - *Time*: 6 hours
  - Tools: WiX Toolset or NSIS
  - Features:
    - [ ] Install RawrXD-Win32IDE.exe
    - [ ] Install dependencies (GGML, zlib)
    - [ ] Create Start Menu shortcuts
    - [ ] Add to PATH (optional)

#### Task 5.2.2: Release Notes
- [ ] **Document**: What's new, known issues
  - *Time*: 4 hours
  - Template:
    - [ ] New features
    - [ ] Bug fixes
    - [ ] Known limitations
    - [ ] Upgrade instructions

#### Task 5.2.3: Testing on Clean System
- [ ] **QA**: Test on fresh Windows install
  - *Time*: 4 hours
  - Checklist:
    - [ ] Installer completes successfully
    - [ ] Binary runs and launches
    - [ ] All core features work
    - [ ] No missing DLLs

---

## WEEKLY CHECKLIST TEMPLATE

### Week [N]: [Theme]

**Completed**:
- [ ] Task 1: [Description] - [Owner] - [Hours spent] / [Planned]
- [ ] Task 2: [Description] - [Owner] - [Hours spent] / [Planned]

**In Progress**:
- [ ] Task 3: [Description] - [Owner] - [% complete]

**Blocked**:
- [ ] Task 4: [Description] - [Reason] - [Unblock plan]

**Status**: [RED/YELLOW/GREEN] - [Brief status]

**Metrics**:
- Unit tests passing: [N]/[Total]
- Code coverage: [%]
- Critical issues open: [N]
- Velocity: [Hours/week]

---

## BUILD VERIFICATION CHECKLIST

### Daily (Automated)
```bash
# Run in CI/CD pipeline
cmake --build . --config Release
ctest --verbose
./tools/run-static-analysis.sh
valgrind ./bin/RawrXD-Win32IDE
```

### Weekly (Manual)
- [ ] Build on clean system
- [ ] Test on Windows 10 + Windows 11
- [ ] Test with fresh Git clone
- [ ] Verify no uncommitted dependencies

### Before Release
- [ ] Full test suite passes
- [ ] Zero compiler warnings
- [ ] AddressSanitizer clean
- [ ] No memory leaks (valgrind)
- [ ] Code coverage >85%
- [ ] Security audit passed
- [ ] Documentation complete

---

## GO/NO-GO GATES

### Gate 1: Security (End of Week 2)
**Must Pass**:
- [ ] TLS/HTTPS working
- [ ] All POST endpoints validated
- [ ] API authentication implemented
- [ ] Rate limiting active
- [ ] Zero critical vulns
- [ ] CSRF protection in place

**If Failed**: Add 1 week to timeline

### Gate 2: IDE Features (End of Week 3.5)
**Must Pass**:
- [ ] File CRUD fully working
- [ ] Find & Replace complete
- [ ] Settings persist to disk
- [ ] Chat responses show
- [ ] Project explorer shows real files

**If Failed**: Add 1-2 weeks

### Gate 3: Quality (End of Week 5.5)
**Must Pass**:
- [ ] 200+ unit tests passing
- [ ] 85%+ code coverage
- [ ] AddressSanitizer clean
- [ ] Performance benchmarks met
- [ ] Zero C++ warnings

**If Failed**: Add 1 week for fixup

### Gate 4: Release (End of Week 6.5)
**Must Pass**:
- [ ] All documentation complete
- [ ] Installer tested on 3 systems
- [ ] Release notes written
- [ ] Marketing approved
- [ ] Support team trained

---

## SIGN-OFF & APPROVAL

| Role | Name | Date | Status |
|------|------|------|--------|
| Project Manager | _________ | __/__/__ | ☐ Approved |
| Security Lead | _________ | __/__/__ | ☐ Approved |
| Technical Lead | _________ | __/__/__ | ☐ Approved |
| QA Lead | _________ | __/__/__ | ☐ Approved |
| Product Owner | _________ | __/__/__ | ☐ Approved |
| Executive | _________ | __/__/__ | ☐ Approved |

