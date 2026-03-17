# .gitignore Specification Test Suite

## Test Cases for Full Spec Compliance

### Test 1: Pattern Order (Last Match Wins)

**`.gitignore`:**
```gitignore
*.log
!important.log
error.log
```

**Expected Results:**
- `debug.log` → Hidden (matches `*.log`)
- `important.log` → Visible (negated by `!important.log`)
- `error.log` → Hidden (matches last `error.log` pattern)

**Tests:**
```cpp
GitignoreFilter filter;
filter.addPattern("*.log");
filter.addPattern("!important.log");
filter.addPattern("error.log");

assert(filter.shouldIgnore("debug.log", "") == true);
assert(filter.shouldIgnore("important.log", "") == false);
assert(filter.shouldIgnore("error.log", "") == true);
```

---

### Test 2: Trailing Space Escaping

**`.gitignore`:**
```gitignore
trailing\ 
notrailing
```

**Expected Results:**
- `trailing ` (with space) → Hidden
- `trailing` (no space) → Visible
- `notrailing` → Hidden
- `notrailing ` (with space) → Visible

**Tests:**
```cpp
QString line1 = "trailing\\ ";
QString line2 = "notrailing";

QString parsed1 = GitignoreFilter::parseGitignoreLine(line1);
QString parsed2 = GitignoreFilter::parseGitignoreLine(line2);

assert(parsed1 == "trailing ");
assert(parsed2 == "notrailing");
```

---

### Test 3: Character Class Negation

**`.gitignore`:**
```gitignore
test[!0-9].txt
file[a-z].dat
doc[!a-c]*
```

**Expected Results:**
- `testA.txt` → Hidden (matches `[!0-9]`)
- `test1.txt` → Visible (doesn't match `[!0-9]`)
- `filea.dat` → Hidden
- `fileZ.dat` → Visible
- `docdefg` → Hidden
- `docabc` → Visible

---

### Test 4: Directory-Only Patterns

**`.gitignore`:**
```gitignore
build/
temp/
```

**Expected Results:**
- `build/` (directory) → Hidden
- `build` (file) → Visible
- `build/file.txt` → Hidden (parent excluded)
- `temp/` (directory) → Hidden
- `temp` (file) → Visible

**Tests:**
```cpp
// Directory paths should end with /
assert(filter.shouldIgnore("build/", "") == true);
assert(filter.shouldIgnore("build", "") == false);
assert(filter.shouldIgnore("src/build/", "") == true);
```

---

### Test 5: Parent Directory Exclusion

**`.gitignore`:**
```gitignore
node_modules/
!node_modules/keep-this/
!node_modules/important.txt
```

**Expected Results:**
- `node_modules/` → Hidden
- `node_modules/package/` → Hidden (parent excluded)
- `node_modules/keep-this/` → **STILL HIDDEN** (parent excluded)
- `node_modules/important.txt` → **STILL HIDDEN** (parent excluded)

**Note:** Per spec, you cannot re-include files if parent directory is excluded.

**Tests:**
```cpp
GitignoreProxyModel proxy;
proxy.setPatterns({"node_modules/", "!node_modules/important.txt"});

// Even with negation, parent exclusion prevents re-inclusion
assert(proxy.isParentDirectoryExcluded("node_modules/important.txt") == true);
```

---

### Test 6: Escaped Special Characters

**`.gitignore`:**
```gitignore
\#hash
\!exclaim
\*asterisk
trailing\\
```

**Expected Results:**
- `#hash` → Hidden (matches `\#hash`)
- `!exclaim` → Hidden (matches `\!exclaim`)
- `*asterisk` → Hidden (matches `\*asterisk`)
- Line with trailing `\` → **INVALID** (rejected)

**Tests:**
```cpp
assert(GitignoreFilter::parseGitignoreLine("\\#hash") == "#hash");
assert(GitignoreFilter::parseGitignoreLine("\\!exclaim") == "!exclaim");
assert(GitignoreFilter::parseGitignoreLine("trailing\\") == ""); // Invalid
```

---

### Test 7: Path Anchoring

**`.gitignore`:**
```gitignore
/root.txt
src/temp.txt
unanchored.txt
```

**Expected Results:**
- `root.txt` → Hidden (anchored to root)
- `sub/root.txt` → Visible (doesn't match anchored pattern)
- `src/temp.txt` → Hidden (anchored to src/)
- `other/src/temp.txt` → Visible (anchored to src/)
- `unanchored.txt` → Hidden (matches anywhere)
- `sub/unanchored.txt` → Hidden (matches anywhere)

---

### Test 8: Recursive Wildcards (`**`)

**`.gitignore`:**
```gitignore
**/logs
logs/**
a/**/b
```

**Expected Results:**
- `logs` → Hidden (matches `**/logs`)
- `src/logs` → Hidden (matches `**/logs`)
- `src/sub/logs` → Hidden (matches `**/logs`)
- `logs/debug.txt` → Hidden (matches `logs/**`)
- `logs/sub/error.txt` → Hidden (matches `logs/**`)
- `a/b` → Hidden (matches `a/**/b`)
- `a/x/b` → Hidden (matches `a/**/b`)
- `a/x/y/z/b` → Hidden (matches `a/**/b`)

---

### Test 9: Simple Wildcards

**`.gitignore`:**
```gitignore
*.log
test?.txt
temp*
```

**Expected Results:**
- `error.log` → Hidden
- `test1.txt` → Hidden (single char)
- `test10.txt` → Visible (two chars)
- `temp` → Hidden
- `temp123` → Hidden
- `tempfile.txt` → Hidden

---

### Test 10: Comment Handling

**`.gitignore`:**
```gitignore
# This is a comment
*.log    # Inline comment style
\#notacomment
```

**Expected Results:**
- First line → Ignored (comment)
- `*.log` → Parsed (second line, ignore inline comment)
- `#notacomment` → Hidden (escaped hash)

**Tests:**
```cpp
assert(GitignoreFilter::parseGitignoreLine("# comment") == "");
assert(GitignoreFilter::parseGitignoreLine("*.log # inline") == "*.log # inline"); // Git doesn't strip inline comments
assert(GitignoreFilter::parseGitignoreLine("\\#notacomment") == "#notacomment");
```

---

### Test 11: Complex Real-World `.gitignore`

**`.gitignore`:**
```gitignore
# Node.js
node_modules/
npm-debug.log
.env
!.env.example

# Build outputs
build/
dist/
*.min.js
!vendor.min.js

# IDE
.vscode/
.idea/
*.swp

# OS
.DS_Store
Thumbs.db

# Logs
logs/
*.log
!important.log

# Test coverage
coverage/
*.coverage
.nyc_output/
```

**Expected Behaviors:**
- `node_modules/package/` → Hidden
- `.env` → Hidden, but `.env.example` → Visible
- `build/` → Hidden (all contents)
- `app.min.js` → Hidden, but `vendor.min.js` → Visible
- `logs/debug.txt` → Hidden (parent excluded)
- `important.log` → Visible (negated)
- `.vscode/settings.json` → Hidden

---

### Test 12: Edge Cases

**`.gitignore`:**
```gitignore

   
*.log
   *.txt   
\  

invalid\
```

**Expected Results:**
- Empty lines → Ignored
- Lines with only spaces → Ignored
- `   *.txt   ` → Parsed as `*.txt` (trimmed)
- `\  ` → Parsed as ` ` (escaped space)
- `invalid\` → **INVALID** (rejected)

---

## Integration Test Script

```cpp
void testGitignoreCompliance() {
    ProjectExplorerWidget explorer;
    
    // Create test directory structure
    QTemporaryDir tempDir;
    QString projectPath = tempDir.path();
    
    // Create .gitignore
    QFile gitignore(projectPath + "/.gitignore");
    gitignore.open(QIODevice::WriteOnly);
    gitignore.write("*.log\n");
    gitignore.write("!important.log\n");
    gitignore.write("build/\n");
    gitignore.write("**/temp\n");
    gitignore.close();
    
    // Create test files
    QFile::create(projectPath + "/debug.log");
    QFile::create(projectPath + "/important.log");
    QFile::create(projectPath + "/readme.txt");
    QDir().mkdir(projectPath + "/build");
    QFile::create(projectPath + "/build/output.exe");
    
    // Open project
    explorer.openProject(projectPath);
    
    // Verify filtering
    auto model = explorer.model();
    assert(isFileVisible(model, "readme.txt") == true);
    assert(isFileVisible(model, "debug.log") == false);
    assert(isFileVisible(model, "important.log") == true);
    assert(isFileVisible(model, "build") == false);
    assert(isFileVisible(model, "build/output.exe") == false);
    
    qDebug() << "All tests passed!";
}
```

---

## Validation Against Git

To validate our implementation matches Git's behavior:

```bash
# Create test repo
mkdir test-gitignore
cd test-gitignore
git init

# Create .gitignore
cat > .gitignore << EOF
*.log
!important.log
build/
**/temp
EOF

# Create test files
touch debug.log important.log readme.txt
mkdir build
touch build/output.exe
mkdir -p src/temp
touch src/temp/file.txt

# Check Git's behavior
git status --ignored

# Compare with RawrXD's filtering
# Both should show same hidden/visible files
```

---

## Performance Tests

Test with large `.gitignore` files:

```cpp
void testPerformance() {
    GitignoreFilter filter;
    
    // Load large .gitignore (e.g., GitHub's Node.gitignore)
    filter.loadFromFile("test_data/large.gitignore");
    
    // Benchmark pattern matching
    QElapsedTimer timer;
    timer.start();
    
    for (int i = 0; i < 10000; ++i) {
        filter.shouldIgnore("src/components/test.tsx", "/project");
    }
    
    qint64 elapsed = timer.elapsed();
    qDebug() << "10k matches in" << elapsed << "ms";
    
    // Should be < 100ms for 10k matches
    assert(elapsed < 100);
}
```

---

## Conclusion

These test cases cover all aspects of the `.gitignore` specification:
- ✅ Pattern ordering
- ✅ Trailing space handling
- ✅ Character classes and negation
- ✅ Directory-only patterns
- ✅ Parent directory exclusion
- ✅ Backslash escaping
- ✅ Path anchoring
- ✅ Wildcards (simple and recursive)
- ✅ Comment handling
- ✅ Edge cases

Run all tests to ensure full spec compliance before production deployment.
