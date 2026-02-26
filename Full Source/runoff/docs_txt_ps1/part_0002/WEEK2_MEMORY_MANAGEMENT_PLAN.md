# Week 2: Memory Management Overhaul - Audit & Implementation Plan

## Executive Summary

The RawrXD codebase uses raw pointers extensively (potentially 100+ instances) in critical components. This creates:
- **Memory leak risk**: Qt parent-child ownership helps, but not all objects use it correctly
- **Double-delete risk**: Manual cleanup can be error-prone
- **Move semantics limitations**: Raw pointers prevent efficient resource transfer
- **Exception safety issues**: No RAII guarantees

## Critical Components (Priority Order)

### HIGH PRIORITY (Most Impact)

#### 1. AI Completion Engine (12 allocations)
**File**: `src/qtapp/ai_code_assistant.cpp`
**Risk**: High - long-lived completions, threading issues
**Allocations**:
- AI completion providers
- Completion state managers  
- Token buffers

**Recommended**: `unique_ptr<AICompletionProvider>`

#### 2. GGUF & Model Loading (7 allocations)
**Files**: `src/gguf_loader.cpp`, `src/gguf_server.cpp`
**Risk**: High - complex lifecycle, multiple ownership scenarios
**Allocations**:
- GGUF servers
- Vocab resolvers
- Weight buffers

**Recommended**: `shared_ptr<GGUFLoader>` for shared access, `unique_ptr` for owned components

#### 3. MainWindow Components (15+ allocations)
**File**: `src/qtapp/MainWindow_v5.cpp`
**Risk**: Medium - mostly Qt-managed, but some raw deletions exist
**Allocations**:
- Dock widgets (already Qt-managed)
- AI switchers
- Command palettes
- Custom layouts

**Recommended**: Mostly keep Qt-managed, but wrap non-Qt components with `unique_ptr`

### MEDIUM PRIORITY

#### 4. Inference Engine (10+ allocations)
**File**: `src/qtapp/inference_engine.cpp`
**Risk**: Medium - complex state, transformer blocks
**Allocations**:
- Transformer blocks
- Scalar computations
- Buffers

#### 5. File Operations & Workspace (8+ allocations)
**Risk**: Medium - file I/O, threading

## Implementation Strategy

### Phase 1: Infrastructure Setup (1 week)
1. Create `src/memory_utils.hpp` with helper templates:
   - `make_unique_move()` - safe move semantics
   - `ownership_check()` - debug ownership verification
   - `RawPtrHolder` - RAII wrapper for legacy APIs

2. Add memory profiling macros:
   - `MEMORY_TRACK()` for allocation tracking
   - `MEMORY_CHECK()` for ownership verification

### Phase 2: AI Completion Engine (1-2 weeks)
Priority: Fix first - blocks threading improvements
1. Wrap `AICompletionProvider` with `unique_ptr`
2. Implement completion provider factory
3. Add move constructors for providers
4. Update threading code to use smart pointers

### Phase 3: GGUF & Model Loading (1-2 weeks)
Priority: Core inference dependency
1. Make `GGUFLoader` use `unique_ptr` internally
2. Implement `shared_ptr` for multi-threaded access
3. Add reference counting for weight buffers
4. Document ownership semantics

### Phase 4: MainWindow (1-2 weeks)
Priority: Large surface area but lower risk
1. Inventory all `new` allocations
2. Keep Qt-managed components (parent-child already handles)
3. Wrap non-Qt with `unique_ptr`
4. Remove manual `delete` calls

### Phase 5: Inference Engine (1 week)
Priority: Medium
1. Update transformer block allocation
2. Wrap buffer management
3. Add RAII for GPU resources

## Conversion Checklist

### For Each Component:

- [ ] Identify all raw `new` allocations
- [ ] Determine ownership (single, shared, Qt-managed)
- [ ] Create appropriate smart pointer type
- [ ] Add RAII cleanup if needed
- [ ] Update all usage sites
- [ ] Add unit tests for memory behavior
- [ ] Verify no delete calls remain
- [ ] Check for circular references (shared_ptr)
- [ ] Profile memory usage before/after
- [ ] Document move semantics

## Code Patterns to Replace

### Pattern 1: Owned Allocation
```cpp
// BEFORE
auto* engine = new InferenceEngine();
// ... use engine ...
delete engine;

// AFTER
auto engine = std::make_unique<InferenceEngine>();
// ... use engine ... (auto cleanup)
```

### Pattern 2: Qt Parent-Child
```cpp
// BEFORE (works, but manual)
auto* widget = new QWidget(parent);
// parent owns widget when destroyed

// AFTER (explicit, clearer intent)
// Keep as-is if truly parent-child
// Convert to unique_ptr if ownership transferred
```

### Pattern 3: Shared Resource
```cpp
// BEFORE (potential leaks)
GGUFLoader* loader = new GGUFLoader();
std::vector<GGUFLoader*> loaders;
loaders.push_back(loader);

// AFTER (reference counted)
auto loader = std::make_shared<GGUFLoader>();
std::vector<std::shared_ptr<GGUFLoader>> loaders;
loaders.push_back(loader);
```

### Pattern 4: Array Allocation
```cpp
// BEFORE
float* buffer = new float[size];
delete[] buffer;

// AFTER
std::vector<float> buffer(size);
// or for fixed-size:
std::array<float, SIZE> buffer;
```

## Expected Improvements

| Metric | Before | After | Change |
|--------|--------|-------|--------|
| Raw pointers | 100+ | <10 | -90% |
| Manual deletes | 50+ | 0 | -100% |
| Memory leaks (detected) | TBD | 0 | Unknown |
| Code lines (cleanup) | 5000+ | 100+ | -98% |
| Test coverage (memory) | 0% | 95%+ | +95% |

## Timeline

- **Week 2.1**: Setup & AI Completion (3 days)
- **Week 2.2**: GGUF & Model Loading (3-4 days)
- **Week 2.3**: MainWindow & Inference (3-4 days)
- **Testing & Profiling**: 2-3 days

**Total: ~2 weeks**

## Success Criteria

1. ✅ No manual `delete` calls in non-legacy code
2. ✅ Memory test suite passes with Valgrind/Dr.Memory
3. ✅ No circular reference warnings
4. ✅ Move semantics used for large transfers
5. ✅ Code review approval from memory specialist
6. ✅ Performance regression < 1%
7. ✅ All 95%+ tests still passing

## Risk Mitigation

- **Circular references**: Use `weak_ptr` where needed
- **Exception safety**: Ensure RAII used everywhere
- **Performance**: Profile before/after each conversion
- **Compatibility**: Maintain public API compatibility
- **Testing**: Add memory tests for critical paths

## Next Steps

1. Create memory utilities header
2. Set up memory profiling/tracking
3. Begin Phase 2 (AI Completion Engine)
4. Create memory test suite
5. Document converted components
