// ============================================================================
// dispatch_thread_safety_audit.h — Re-entrancy & Thread-Safety Analysis
// ============================================================================
//
// Audit Target:  SharedFeatureRegistry dispatch paths
//                UnifiedHotpatchManager mutation paths
//                EditorEngineFactory switches
//                PluginIdAllocator slot management
//
// Methodology:   Static analysis of lock ordering, re-entrancy surfaces,
//                deadlock potential, and memory ordering correctness.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

/*
╔══════════════════════════════════════════════════════════════════════════════╗
║                    THREAD-SAFETY AUDIT REPORT                              ║
║                    RawrXD Command Dispatch System                          ║
║                    Date: 2026-02-11                                        ║
╚══════════════════════════════════════════════════════════════════════════════╝

═══════════════════════════════════════════════════════════════════════════════
 1. LOCK INVENTORY
═══════════════════════════════════════════════════════════════════════════════

 Component                    │ Lock Type              │ Granularity
 ─────────────────────────────┼────────────────────────┼───────────────────
 SharedFeatureRegistry        │ std::mutex m_mutex      │ Global (all ops)
 UnifiedHotpatchManager       │ std::mutex m_mutex      │ Global (all ops)
 EditorEngineFactory          │ std::mutex m_engineMutex│ Engine switches
 PluginIdAllocator            │ std::mutex m_mutex      │ Slot alloc/free

 Total mutex instances: 4 (no recursive mutexes, no spinlocks)

═══════════════════════════════════════════════════════════════════════════════
 2. RE-ENTRANCY ANALYSIS
═══════════════════════════════════════════════════════════════════════════════

 CONCERN: Can a FeatureHandler trigger another dispatch while holding the
          registry mutex?

 Current dispatch flow:
   dispatchByCommandId(cmdId, ctx)
     → lock(m_mutex)
     → find feature
     → call feat.handler(ctx)    ◄── HANDLER RUNS UNDER LOCK
     → unlock(m_mutex)

 VERDICT: ██ CRITICAL ██

   If any FeatureHandler calls back into dispatch() / dispatchByCli() /
   dispatchByCommandId(), it will deadlock on m_mutex (non-recursive).

   Known safe handlers: All current handlers in feature_handlers.cpp are
   leaf functions (they don't re-dispatch). But this is FRAGILE.

 RECOMMENDATION: Release lock before calling handler.

   CommandResult dispatchByCommandId(uint32_t cmdId, const CommandContext& ctx) {
       FeatureHandler handler = nullptr;
       {
           std::lock_guard<std::mutex> lock(m_mutex);
           auto it = m_byCommandId.find(cmdId);
           if (it == m_byCommandId.end())
               return CommandResult::error("Command ID not registered");
           handler = m_features[it->second].handler;
           if (!handler)
               return CommandResult::error("Feature has no handler");
       }
       // Handler runs OUTSIDE lock — re-entrancy safe
       m_dispatchCount.fetch_add(1, std::memory_order_relaxed);
       return handler(ctx);
   }

   Same pattern for dispatch() and dispatchByCli().

═══════════════════════════════════════════════════════════════════════════════
 3. LOCK ORDERING (Deadlock Prevention)
═══════════════════════════════════════════════════════════════════════════════

 If multiple mutexes are ever held simultaneously, a strict ordering is
 required. Current architecture:

   Level 0: PluginIdAllocator::m_mutex       (acquired first)
   Level 1: SharedFeatureRegistry::m_mutex    (acquired second)
   Level 2: UnifiedHotpatchManager::m_mutex   (acquired third)
   Level 3: EditorEngineFactory::m_engineMutex(acquired last)

 Current state: No code path holds more than one mutex simultaneously.
   → SharedFeatureRegistry never calls into UnifiedHotpatchManager under lock
   → PluginIdAllocator is standalone (no dependencies)
   → EditorEngineFactory is standalone (no dispatch under lock)

 VERDICT: ██ SAFE ██ (No deadlock risk currently)

 HOWEVER: If the re-entrancy fix above is applied, handlers could
 theoretically acquire UnifiedHotpatchManager::m_mutex while the
 SharedFeatureRegistry lock is released. This is safe because the
 ordering constraint is maintained.

═══════════════════════════════════════════════════════════════════════════════
 4. ATOMIC OPERATIONS AUDIT
═══════════════════════════════════════════════════════════════════════════════

 Variable                                  │ Order             │ Safe?
 ──────────────────────────────────────────┼───────────────────┼───────
 SharedFeatureRegistry::m_totalRegistered   │ relaxed           │ YES
 SharedFeatureRegistry::m_dispatchCount     │ relaxed           │ YES
 UnifiedHotpatchManager::m_sequence         │ (implied relaxed) │ YES(*)
 UnifiedHotpatchManager::Stats::*           │ relaxed           │ YES
 UnifiedHotpatchManager::m_eventHead/Tail   │ relaxed           │ REVIEW
 UnifiedHotpatchManager::m_*Init            │ (default)         │ YES
 PluginIdAllocator::m_activeSlots           │ relaxed           │ YES
 EditorEngineFactory — none used            │ N/A               │ N/A
 MonacoCoreEngine::m_ready/m_visible        │ (default)         │ YES

 (*) m_sequence: used for monotonic numbering. relaxed is correct here
     because sequenceId ordering only matters within a single thread's
     perspective (the manager is already under m_mutex when incrementing).

 CONCERN: m_eventHead/m_eventTail

   The ring buffer uses relaxed ordering for head/tail. This is a
   classic SPSC (single-producer single-consumer) pattern.
   
   IF multiple producers emit events concurrently (e.g., memory patch
   + byte patch in parallel), relaxed is INSUFFICIENT — events could
   be overwritten before consumers read them.

   Current state: All emit_event() calls happen under m_mutex, so this
   is effectively single-producer. SAFE as-is.

   If you ever remove m_mutex from emit_event(), upgrade to:
     m_eventHead: memory_order_acquire (consumer reads)
     m_eventTail: memory_order_release (producer writes)

═══════════════════════════════════════════════════════════════════════════════
 5. WM_COMMAND THREAD SAFETY
═══════════════════════════════════════════════════════════════════════════════

 Win32 WM_COMMAND messages arrive on the UI thread (the thread that called
 CreateWindowEx and runs the message pump). This is the ONLY thread that
 should call dispatchByCommandId().

 CLI dispatch (dispatchByCli) may be called from:
   - The main thread (interactive shell)
   - A separate thread (if /server endpoint dispatches CLI commands)
   - Agent threads (if autonomous agents issue CLI commands)

 The registry mutex protects against concurrent reads/writes, BUT:

 CONCERN: GUI state mutation from non-UI thread

   If dispatchByCli("!find", ctx) is called from a server thread,
   and the handler calls into Win32 APIs (SendMessage, SetDlgItemText),
   this is UNSAFE — Win32 GUI calls from non-UI threads cause UB.

 SAFEGUARD: The CommandContext::isGui flag should be checked.
   Handlers that touch HWND/HMENU MUST verify ctx.isGui == true
   and that they're on the UI thread. CLI-only invocations should
   never touch GUI state.

 RECOMMENDATION: Add a thread-ID check:

   bool isOnUIThread() {
       static DWORD s_uiThread = GetCurrentThreadId(); // Set in WinMain
       return GetCurrentThreadId() == s_uiThread;
   }

   // In handlers that touch GUI:
   if (ctx.isGui && !isOnUIThread()) {
       PostMessage(hwnd, WM_APP_DISPATCH, cmdId, 0);
       return CommandResult::ok("Deferred to UI thread");
   }

═══════════════════════════════════════════════════════════════════════════════
 6. RECOMMENDATIONS SUMMARY
═══════════════════════════════════════════════════════════════════════════════

 Priority │ Issue                             │ Fix
 ─────────┼───────────────────────────────────┼──────────────────────────────
 P0       │ Handler runs under registry lock  │ Release lock before handler()
 P1       │ No UI thread assertion in GUI     │ Add isOnUIThread() guard
          │   handlers                        │
 P2       │ Event ring relaxed ordering       │ Document SPSC constraint or
          │   assumptions                     │   upgrade to acquire/release
 P3       │ No lock ordering documentation    │ Add comment block (this file)
 P4       │ Static init order (g_registrar)   │ Already singleton-guarded —
          │                                   │   no action needed

 OVERALL THREAD-SAFETY GRADE: B+
   - No deadlocks possible (single-lock-per-path pattern)
   - Re-entrancy is the primary risk (P0)
   - Atomic usage is correct for current access patterns
   - Win32 GUI thread safety needs hardening (P1)

*/

// ============================================================================
// Thread-safety helpers (implement in dispatch_thread_safety.cpp)
// ============================================================================

#include <cstdint>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace ThreadSafety {

#ifdef _WIN32
    // Set once in WinMain/wWinMain before any dispatch
    inline DWORD g_uiThreadId = 0;
    
    inline void setUIThread() {
        g_uiThreadId = GetCurrentThreadId();
    }
    
    inline bool isOnUIThread() {
        return GetCurrentThreadId() == g_uiThreadId;
    }
    
    // Post a deferred command dispatch to the UI thread
    // Uses WM_APP + 0x100 to avoid collision with other WM_APP messages
    constexpr UINT WM_DEFERRED_DISPATCH = WM_APP + 0x100;
    
    inline bool deferToUIThread(HWND hwnd, uint32_t commandId) {
        if (isOnUIThread()) return false;  // Already on UI thread
        PostMessage(hwnd, WM_DEFERRED_DISPATCH, 
                    static_cast<WPARAM>(commandId), 0);
        return true;
    }
#else
    inline void setUIThread() {}
    inline bool isOnUIThread() { return true; }
#endif

} // namespace ThreadSafety

#endif // Header guard handled by pragma once
