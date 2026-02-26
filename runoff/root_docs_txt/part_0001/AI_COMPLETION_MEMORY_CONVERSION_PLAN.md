# AI Completion Engine - Smart Pointer Conversion Plan

## Current State Analysis

### Raw Pointer Members
1. `QNetworkAccessManager *m_networkManager` - Line 15: `new QNetworkAccessManager(this)`
2. `QProcess *m_process` - Lines 147, 169: `new QProcess(this)`

### Ownership Analysis
- **Qt Parent-Child**: Both allocations use `this` as parent, so Qt handles cleanup
- **Risk**: Medium - Manual cleanup in destructor suggests potential issues
- **Benefit**: Converting to smart pointers provides explicit ownership semantics

## Conversion Strategy

### Option 1: Keep Qt Parent-Child (Recommended)
Since these are Qt objects with parent-child relationships, we can:
1. Keep raw pointers but add explicit ownership documentation
2. Remove manual cleanup from destructor (Qt handles it)
3. Add null checks for safety

### Option 2: Convert to unique_ptr
If we want explicit RAII:
1. Use `std::unique_ptr` with custom deleters
2. Ensure Qt parent-child relationship is maintained
3. More complex but explicit ownership

**Recommendation**: Option 1 for Qt objects, Option 2 for non-Qt objects

## Implementation Plan

### Phase 1: Documentation & Safety
1. Add ownership comments to header
2. Remove unnecessary destructor cleanup
3. Add null checks in usage

### Phase 2: Smart Pointer Conversion (if needed)
1. Convert to `std::unique_ptr` with Qt-aware deleters
2. Update all usage sites
3. Test thoroughly

## Code Changes

### Header File Changes
```cpp
// Members
QNetworkAccessManager *m_networkManager;  // Owned by Qt parent-child
QProcess *m_process;                      // Owned by Qt parent-child, lazy-initialized
```

### Source File Changes
```cpp
// Remove manual cleanup from destructor
AICodeAssistant::~AICodeAssistant()
{
    // Qt parent-child handles cleanup automatically
    // No manual cleanup needed for m_networkManager and m_process
}
```

### Usage Safety
```cpp
// Add null checks where appropriate
if (m_networkManager) {
    m_networkManager->post(...);
}
```

## Testing Strategy

1. **Memory Leak Test**: Run with Valgrind/drmemory
2. **Destructor Test**: Verify no double-delete
3. **Null Safety**: Test with null pointers
4. **Performance**: Profile before/after

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|-----------|
| Double-delete | Low | High | Qt parent-child prevents |
| Memory leak | Low | Medium | Qt handles cleanup |
| Performance | None | None | Same memory management |
| API breakage | None | None | Same interface |

**Overall Risk**: LOW

## Next Steps

1. Implement Phase 1 changes
2. Test thoroughly
3. Document ownership semantics
4. Move to next component

## Alternative: Full Smart Pointer Conversion

If we want explicit RAII despite Qt parent-child:

```cpp
// In header
#include <memory>
#include "memory_utils.hpp"

class AICodeAssistant : public QObject
{
    // ...
private:
    std::unique_ptr<QNetworkAccessManager, std::function<void(QNetworkAccessManager*)>> m_networkManager;
    std::unique_ptr<QProcess, std::function<void(QProcess*)>> m_process;
};

// In constructor
AICodeAssistant::AICodeAssistant(QObject *parent) 
    : QObject(parent)
    , m_networkManager(nullptr, [this](QNetworkAccessManager* ptr) { 
        if (ptr) ptr->setParent(nullptr); delete ptr; 
    })
    , m_process(nullptr, [this](QProcess* ptr) { 
        if (ptr) ptr->setParent(nullptr); delete ptr; 
    })
{
    m_networkManager.reset(new QNetworkAccessManager(this));
    // ...
}
```

**Recommendation**: Stick with Qt parent-child for Qt objects, use smart pointers for non-Qt objects.