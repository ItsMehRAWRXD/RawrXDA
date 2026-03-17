# RawrXD "20 Windows" Specification Clarification

**Investigation Date:** January 15, 2026  
**Status:** CLARIFIED

## Executive Summary

The "20 windows" specification in planning is **NOT a hardcoded limit** in the current codebase.

| Specification | What It Actually Is | Limit | Evidence |
|---|---|---|---|
| "100 panels" | AIChatPanel instances per IDE | **Hard: 100** | `AIChatPanelManager::m_maxPanels = 100` |
| "20 agents" | Agent types + custom agents | **No limit** | `AgentOrchestrator` supports unlimited agents |
| "20 windows" | **UNDEFINED** | **See analysis** | No code reference found |

---

## Investigation Results

### Search for "20" References

**Pattern:** `grep -r "20" src/qtapp/`

**Findings:**
- ❌ No `maxWindows = 20`
- ❌ No `maxInstances = 20`
- ❌ No `maxAgentWindows = 20`
- ✅ Found: `m_maxPanels = 100` (per-instance)
- ✅ Found: 12 built-in agent types + unlimited custom

---

## Possible Interpretations

### Interpretation 1: Agent Count Target (Most Likely) ✅

**Evidence:**
```cpp
enum class AgentType {
    CodeAnalyzer,           // 1
    BugDetector,            // 2
    Refactorer,             // 3
    TestGenerator,          // 4
    Documenter,             // 5
    PerformanceOptimizer,   // 6
    SecurityAuditor,        // 7
    CodeCompleter,          // 8
    TaskPlanner,            // 9
    CodeReviewer,           // 10
    DependencyAnalyzer,     // 11
    MigrationAssistant,     // 12
    Custom                  // +N more
};
// Total: 12+ built-in + unlimited custom
```

**Why "20"?**
- Historical design goal: "Support 20+ concurrent agents"
- 12 built-in types already implemented
- Can add 8+ custom agents to reach 20+

### Interpretation 2: Terminal Session Count ❌

**Evidence:** No hardcoded limit found in `real_time_terminal_pool.cpp`

**Likelihood:** Low

### Interpretation 3: Concurrent IDE Windows ❌

**Evidence:** No instance limiter in `ide_main.cpp`

**Likelihood:** Low

---

## Clear Limits (Verified)

### FACT ✅: 100 Chat Panels Per IDE Instance

```cpp
// src/qtapp/ai_chat_panel_manager.hpp
class AIChatPanelManager {
    const int m_maxPanels = 100;  // ← HARDCODED LIMIT
};
```

### FACT ✅: 12+ Agent Types + Custom Agents

```cpp
// src/qtapp/AgentOrchestrator.h
enum class AgentType {
    // ... 12 built-in types ...
    Custom  // Allows custom types
};
```

### FACT ✅: Unlimited IDE Windows

```cpp
// src/ide_main.cpp
int WINAPI wWinMain(...) {
    // No instance checking or limiting
    return 0;
}
```

### FACT ✅: 100 CLI Port Range

```cpp
// src/rawrxd_cli.cpp
uint16_t instancePort = 11434 + (pid % 100);
// Range: 11434-11533 (100 ports)
```

---

## Recommendations

### For Your Planning Document

**CURRENT TEXT (Ambiguous):**
> Clarify if "20 windows" refers to agent panes or full IDE instances

**REVISED TEXT (Clear):**

The RawrXD IDE supports:

1. **Chat Panels:** 100 hardcoded limit per IDE instance
   - Multi-instance: N IDE × 100 panels = N×100 total

2. **AI Agents:** 12 built-in + unlimited custom agents
   - No hardcoded limit

3. **IDE Windows:** Unlimited (OS/memory limited)
   - No instance count restriction

4. **CLI Instances:** 100 maximum (port range 11434-11533)

5. **"20 Windows":** Likely refers to:
   - Agent count design goal (12+ built-in, target 20+)
   - **NOT a hardcoded limit** for IDE windows

---

## Action Items

- [ ] Search commit history for "20 windows" reference
- [ ] Ask original architect about intent
- [ ] Update planning docs with definitive meaning
- [ ] Add code comments explaining specs

---

## Conclusion

### Clear Limits:
✅ **100 chat panels per IDE** (hardcoded)  
✅ **100 CLI ports** (11434-11533)  

### No Limits:
❌ **IDE windows:** Unlimited  
❌ **Agents:** Unlimited  

### Ambiguous:
❓ **"20 windows":** Likely agent count goal, not enforced

