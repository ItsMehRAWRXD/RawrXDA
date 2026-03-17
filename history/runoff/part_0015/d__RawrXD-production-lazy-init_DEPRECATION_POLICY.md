# RawrXD IDE - Deprecation Policy

**Effective:** v1.0.0 (January 22, 2026)  
**Scope:** All stable APIs documented in `FEATURE_FLAGS.md`

---

## 🎯 Policy Goals

1. **Predictability** - Developers know when APIs will be removed
2. **Migration Time** - Sufficient notice (2 minor versions = ~6 months)
3. **Clear Communication** - Deprecation warnings at compile-time and runtime
4. **Graceful Sunset** - Deprecated APIs work until removal

---

## 📜 Deprecation Lifecycle

### Phase 1: Deprecation Announcement (v1.x)

**When:** A stable API needs to be replaced or removed

**Actions:**
1. Mark function/class with C++14 `[[deprecated]]` attribute
2. Add deprecation notice to documentation
3. Create migration guide with examples
4. Announce in release notes

**Example:**

```cpp
// v1.1.0 - Deprecate old function
[[deprecated("Use newFunction() instead. See MIGRATION_v1.1.md")]]
bool oldFunction(const QString &path) {
    // Still works, but logs warning
    qWarning() << "oldFunction() is deprecated, use newFunction()";
    return newFunction(path);
}

// New replacement
bool newFunction(const QString &path, ModelOptions opts = {}) {
    // New implementation
}
```

**Compiler Output:**
```
warning C4996: 'oldFunction': Use newFunction() instead. See MIGRATION_v1.1.md
```

---

### Phase 2: Deprecation Warning Period (v1.x+1 to v1.x+2)

**Duration:** 2 minor versions (~6 months)

**Actions:**
1. Deprecated API still works but emits warnings
2. New code should not use deprecated APIs
3. Migration guide prominently linked in docs
4. CI warns (but doesn't fail) on deprecated usage

**Timeline Example:**
- v1.1.0: `oldFunction()` deprecated
- v1.2.0: Still works, still warns
- v1.3.0: Still works, still warns
- v2.0.0: Removed

---

### Phase 3: Removal (v2.0.0)

**When:** Next major version

**Actions:**
1. Remove deprecated API completely
2. Compilation fails if code uses old API
3. Release notes list all removals
4. Provide automated migration tool (if feasible)

---

## 🚨 Deprecation Severity Levels

### Critical (Affects Core APIs)

**Examples:**
- `IDirectoryManager` method signature change
- `GitignoreFilter` behavior modification
- Model router protocol change

**Notice Period:** **3 minor versions** (v1.x, v1.x+1, v1.x+2) = ~9 months

**Requirements:**
- Automated migration tool required
- Blog post announcement
- Direct notification to known users (if enterprise)

---

### Major (Affects Common Usage)

**Examples:**
- LSP client API change
- Agent coordinator interface modification
- Configuration file format change

**Notice Period:** **2 minor versions** (v1.x, v1.x+1) = ~6 months

**Requirements:**
- Migration guide with code examples
- Release notes entry
- Deprecation warnings in IDE UI

---

### Minor (Rarely Used Features)

**Examples:**
- Experimental feature removal
- Internal utility function change
- Obscure configuration option

**Notice Period:** **1 minor version** (v1.x) = ~3 months

**Requirements:**
- Deprecation attribute
- Release notes mention

---

## 📋 Deprecation Process Checklist

### 1. Proposal Phase

- [ ] **Identify API to deprecate** - Document reason (performance, security, better alternative)
- [ ] **Check usage** - Search codebase for usage patterns
- [ ] **Determine severity** - Critical / Major / Minor
- [ ] **Design replacement** - Ensure new API is better

### 2. Implementation Phase

- [ ] **Add `[[deprecated]]` attribute** - Include replacement suggestion
- [ ] **Implement warning logs** - Runtime warnings for dynamic usage
- [ ] **Create migration guide** - File: `docs/MIGRATION_v1.x.md`
- [ ] **Write automated migration script** - If feasible (regex/AST transformation)
- [ ] **Update documentation** - Mark as deprecated in API docs

### 3. Announcement Phase

- [ ] **Release notes entry** - Clear explanation of deprecation
- [ ] **Blog post** - For critical/major deprecations
- [ ] **GitHub Discussion** - Allow community feedback
- [ ] **Update CHANGELOG** - List deprecation with timeline

### 4. Monitoring Phase

- [ ] **Track usage metrics** - Telemetry for deprecated API calls (opt-in)
- [ ] **Review migration progress** - Check if users are switching
- [ ] **Extend timeline if needed** - If adoption is slow

### 5. Removal Phase

- [ ] **Remove code** - Delete deprecated API in v2.0.0
- [ ] **Update tests** - Remove tests for old API
- [ ] **Final migration guide** - Confirm all paths covered
- [ ] **Release v2.0.0** - With complete removal

---

## 📚 Documentation Examples

### Deprecation Notice in API Docs

```cpp
/**
 * @brief Load model from path
 * @deprecated Since v1.1.0. Use loadModelWithOptions() instead.
 *             This function will be removed in v2.0.0.
 *             See MIGRATION_v1.1.md for migration guide.
 * @param path Model file path
 * @return true if loaded successfully
 */
[[deprecated("Use loadModelWithOptions() instead")]]
bool loadModel(const QString &path);
```

### Runtime Warning Example

```cpp
bool loadModel(const QString &path) {
    qWarning() << "⚠️  loadModel() is deprecated since v1.1.0";
    qWarning() << "    Use loadModelWithOptions() instead";
    qWarning() << "    This function will be removed in v2.0.0";
    qWarning() << "    See MIGRATION_v1.1.md for details";
    
    // Call new implementation with defaults
    return loadModelWithOptions(path, ModelOptions{});
}
```

### Migration Guide Template

**File:** `docs/MIGRATION_v1.1.md`

```markdown
# Migration Guide: v1.0 → v1.1

## Deprecated APIs

### `loadModel(const QString &path)`

**Deprecated in:** v1.1.0  
**Removal in:** v2.0.0  
**Reason:** New function supports additional options for quantization, context size, etc.

**Old Code:**
```cpp
bool success = modelRouter->loadModel("/path/to/model.gguf");
```

**New Code:**
```cpp
ModelOptions opts;
opts.contextSize = 4096;  // Optional
opts.quantization = Q4_0; // Optional

bool success = modelRouter->loadModelWithOptions("/path/to/model.gguf", opts);
```

**Automated Migration:**
```bash
# Run migration script
python scripts/migrate_v1.0_to_v1.1.py --inplace src/
```
```

---

## 🔄 Version Number Scheme

RawrXD follows **Semantic Versioning 2.0.0** (semver.org):

```
MAJOR.MINOR.PATCH

MAJOR = Breaking changes (API removal, behavior change)
MINOR = New features (backward compatible)
PATCH = Bug fixes (backward compatible)
```

**Examples:**
- v1.0.0 → v1.1.0: Added new feature, no breaking changes
- v1.1.0 → v1.2.0: Deprecated old API, still works
- v1.2.0 → v2.0.0: Removed deprecated API (breaking change)

---

## 🚫 What Will NOT Trigger Deprecation

The following changes are **backward compatible** and do NOT require deprecation:

1. **Adding optional parameters with defaults**
   ```cpp
   // v1.0.0
   void foo(int x);
   
   // v1.1.0 - OK (backward compatible)
   void foo(int x, bool flag = false);
   ```

2. **Adding new overloads**
   ```cpp
   // v1.0.0
   void process(const QString &data);
   
   // v1.1.0 - OK (new overload)
   void process(const QByteArray &data);
   ```

3. **Adding new classes/functions**
   - No deprecation needed, just add

4. **Internal implementation changes**
   - As long as public API behavior is identical

5. **Performance improvements**
   - Faster is always okay (unless it breaks edge cases)

6. **Bug fixes**
   - Fixing incorrect behavior is not a breaking change

---

## 📊 Metrics & Tracking

### Deprecation Dashboard (Telemetry - Opt-In)

Track the following metrics:

| Metric | Description | Target |
|--------|-------------|--------|
| **Deprecated API Usage** | % of calls using deprecated APIs | <5% by v1.x+2 |
| **Migration Rate** | Rate of users switching to new API | >90% by v1.x+1 |
| **Removal Readiness** | Can we safely remove in v2.0? | 100% by v2.0-rc |

### GitHub Issues Labels

Use labels for tracking:
- `deprecation` - API deprecation tracking
- `breaking-change` - Will break in v2.0
- `migration-needed` - User needs to update code

---

## 🤝 Community Input

Before deprecating **critical APIs**, open a GitHub Discussion:

**Template:**
```markdown
# Proposal: Deprecate `oldFunction()` in v1.x

**Reason:** [Explain why]

**Replacement:** `newFunction()` provides [benefits]

**Timeline:**
- v1.x: Deprecation notice
- v1.x+1: Still works, warning phase
- v1.x+2: Final warning
- v2.0: Removed

**Migration Path:** [Link to guide]

**Feedback requested by:** [Date, 2 weeks from proposal]
```

Allow community to:
- Suggest alternatives
- Request timeline extension
- Identify edge cases

---

## 🛡️ Exceptions to Policy

In rare cases, **immediate removal** may be required:

### Security Vulnerabilities

If an API has a critical security flaw that cannot be fixed without breaking compatibility:

1. **Immediate deprecation** - Skip warning period
2. **Emergency patch release** - v1.x.y with removal
3. **Security advisory** - CVE disclosure with mitigation
4. **Aggressive migration push** - Direct outreach to users

**Example:**
```cpp
// v1.2.3 - SECURITY: Removed unsafeFunction() (CVE-2026-XXXX)
// Use safeFunction() instead
// No deprecation period due to critical vulnerability
```

---

## ✅ Compliance Check

Before releasing v2.0.0, verify:

- [ ] All deprecated APIs (from v1.x, v1.x+1) are removed
- [ ] No compilation errors in test suite after removal
- [ ] Migration guides exist for all removals
- [ ] Release notes document all breaking changes
- [ ] Automated migration tool tested (if provided)

---

**Version:** 1.0  
**Last Updated:** January 22, 2026  
**Next Review:** v1.1.0 release planning
