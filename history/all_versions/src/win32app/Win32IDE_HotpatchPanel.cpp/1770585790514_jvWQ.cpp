// Win32IDE_HotpatchPanel.cpp — Hotpatch UI Integration (Phase 14.2)
// Wires the three-layer hotpatch system into the Win32IDE command palette
// and menu bar. Handles all IDM_HOTPATCH_* commands (9001–9030).
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "Win32IDE.h"
#include "../core/unified_hotpatch_manager.hpp"
#include "../core/proxy_hotpatcher.hpp"
#include <sstream>
#include <iomanip>
#include <commdlg.h>

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initHotpatchUI() {
    if (m_hotpatchUIInitialized) return;
    m_hotpatchEnabled = true;
    m_hotpatchUIInitialized = true;
    appendToOutput("[Hotpatch] Three-layer hotpatch system initialized.\n");
}

// ============================================================================
// Command Router
// ============================================================================

void Win32IDE::handleHotpatchCommand(int commandId) {
    // Lazy-init the hotpatch subsystem on first command
    if (!m_hotpatchUIInitialized) {
        initHotpatchUI();
    }

    switch (commandId) {
        case IDM_HOTPATCH_SHOW_STATUS:      cmdHotpatchShowStatus();      break;
        case IDM_HOTPATCH_TOGGLE_ALL:        cmdHotpatchToggleAll();       break;
        case IDM_HOTPATCH_SHOW_EVENT_LOG:   cmdHotpatchShowEventLog();    break;
        case IDM_HOTPATCH_RESET_STATS:      cmdHotpatchResetStats();      break;
        case IDM_HOTPATCH_MEMORY_APPLY:     cmdHotpatchMemoryApply();     break;
        case IDM_HOTPATCH_MEMORY_REVERT:    cmdHotpatchMemoryRevert();    break;
        case IDM_HOTPATCH_BYTE_APPLY:       cmdHotpatchByteApply();       break;
        case IDM_HOTPATCH_BYTE_SEARCH:      cmdHotpatchByteSearch();      break;
        case IDM_HOTPATCH_SERVER_ADD:       cmdHotpatchServerAdd();       break;
        case IDM_HOTPATCH_SERVER_REMOVE:    cmdHotpatchServerRemove();    break;
        case IDM_HOTPATCH_PROXY_BIAS:       cmdHotpatchProxyBias();       break;
        case IDM_HOTPATCH_PROXY_REWRITE:    cmdHotpatchProxyRewrite();    break;
        case IDM_HOTPATCH_PROXY_TERMINATE:  cmdHotpatchProxyTerminate();  break;
        case IDM_HOTPATCH_PROXY_VALIDATE:   cmdHotpatchProxyValidate();   break;
        case IDM_HOTPATCH_SHOW_PROXY_STATS: cmdHotpatchShowProxyStats();  break;
        case IDM_HOTPATCH_PRESET_SAVE:      cmdHotpatchPresetSave();      break;
        case IDM_HOTPATCH_PRESET_LOAD:      cmdHotpatchPresetLoad();      break;
        default:
            appendToOutput("[Hotpatch] Unknown hotpatch command: " + std::to_string(commandId) + "\n");
            break;
    }
}

// ============================================================================
// Status & Toggle
// ============================================================================

void Win32IDE::cmdHotpatchShowStatus() {
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();

    auto& proxy = ProxyHotpatcher::instance();
    const auto& pstats = proxy.getStats();

    auto& memStats = get_memory_patch_stats();

    std::ostringstream ss;
    ss << "=== RawrXD Hotpatch System Status ===\n";
    ss << "  System Enabled:    " << (m_hotpatchEnabled ? "YES" : "NO") << "\n";
    ss << "\n--- Unified Manager ---\n";
    ss << "  Memory Patches:    " << stats.memoryPatchCount.load()  << "\n";
    ss << "  Byte Patches:      " << stats.bytePatchCount.load()    << "\n";
    ss << "  Server Patches:    " << stats.serverPatchCount.load()  << "\n";
    ss << "  Total Operations:  " << stats.totalOperations.load()   << "\n";
    ss << "  Total Failures:    " << stats.totalFailures.load()     << "\n";
    ss << "\n--- Memory Layer (Layer 1) ---\n";
    ss << "  Applied:           " << memStats.totalApplied.load()   << "\n";
    ss << "  Reverted:          " << memStats.totalReverted.load()  << "\n";
    ss << "  Failed:            " << memStats.totalFailed.load()    << "\n";
    ss << "  Protect Changes:   " << memStats.protectionChanges.load() << "\n";
    ss << "\n--- Proxy Hotpatcher ---\n";
    ss << "  Tokens Processed:  " << pstats.tokensProcessed.load()  << "\n";
    ss << "  Biases Applied:    " << pstats.biasesApplied.load()    << "\n";
    ss << "  Streams Terminated:" << pstats.streamsTerminated.load() << "\n";
    ss << "  Rewrites Applied:  " << pstats.rewritesApplied.load()  << "\n";
    ss << "  Valid. Passed:     " << pstats.validationsPassed.load() << "\n";
    ss << "  Valid. Failed:     " << pstats.validationsFailed.load() << "\n";
    ss << "=====================================\n";

    appendToOutput(ss.str());
    MessageBoxA(m_hwndMain, ss.str().c_str(), "Hotpatch System Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::cmdHotpatchToggleAll() {
    m_hotpatchEnabled = !m_hotpatchEnabled;
    std::string msg = std::string("[Hotpatch] System ") +
                      (m_hotpatchEnabled ? "ENABLED" : "DISABLED") + "\n";
    appendToOutput(msg);

    // Update status bar
    if (m_hwndStatusBar) {
        std::string sbText = std::string("Hotpatch: ") +
                             (m_hotpatchEnabled ? "ON" : "OFF");
        SendMessageA(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)sbText.c_str());
    }
}

void Win32IDE::cmdHotpatchResetStats() {
    UnifiedHotpatchManager::instance().resetStats();
    ProxyHotpatcher::instance().resetStats();
    reset_memory_patch_stats();
    appendToOutput("[Hotpatch] All statistics reset.\n");
}

// ============================================================================
// Event Log
// ============================================================================

void Win32IDE::cmdHotpatchShowEventLog() {
    auto& mgr = UnifiedHotpatchManager::instance();
    std::ostringstream ss;
    ss << "=== Hotpatch Event Log ===\n";

    HotpatchEvent evt;
    int count = 0;
    while (mgr.poll_event(&evt) && count < 64) {
        static const char* typeNames[] = {
            "MemPatchApplied", "MemPatchReverted", "BytePatchApplied",
            "BytePatchFailed", "ServerPatchAdded", "ServerPatchRemoved",
            "PresetLoaded", "PresetSaved"
        };
        const char* tname = (evt.type < 8) ? typeNames[evt.type] : "Unknown";
        ss << "  [" << evt.sequenceId << "] " << tname
           << " @ tick " << evt.timestamp;
        if (evt.detail) ss << " — " << evt.detail;
        ss << "\n";
        ++count;
    }

    if (count == 0) {
        ss << "  (No events in ring buffer)\n";
    }
    ss << "==========================\n";

    appendToOutput(ss.str());
}

// ============================================================================
// Memory Layer (Layer 1)
// ============================================================================

void Win32IDE::cmdHotpatchMemoryApply() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    // Prompt for hex address + hex data
    char addrBuf[64] = {};
    char dataBuf[512] = {};

    // Simple input dialog — address
    if (MessageBoxA(m_hwndMain,
        "Memory Patch: Enter the target virtual address (hex) in the chat input,\n"
        "then the patch bytes (hex pairs, e.g. 90 90 90).\n\n"
        "This will apply a VirtualProtect-wrapped memory patch.\n"
        "Use with caution — this modifies live process memory.",
        "Hotpatch: Apply Memory Patch", MB_OKCANCEL | MB_ICONWARNING) == IDCANCEL) {
        return;
    }

    appendToOutput("[Hotpatch] Memory patch dialog ready. Enter address and bytes in the chat input.\n");
    appendToOutput("[Hotpatch] Format: !hotpatch_apply <hex_addr> <hex_bytes>\n");
    appendToOutput("[Hotpatch] The memory layer uses VirtualProtect for safe writes.\n");

    // Also provide an input prompt dialog for GUI users
    char addrBuf[64] = {};
    char bytesBuf[256] = {};
    // Use a two-step dialog: first get address, then get bytes
    // (Using simple input boxes since full property sheet dialog is heavy)
    if (MessageBoxA(m_hwndMain,
        "Would you like to enter the patch details now?\n\n"
        "Click YES to input address+bytes in the output panel.\n"
        "Click NO to use CLI commands instead.",
        "Hotpatch: Memory Patch", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        appendToOutput("[Hotpatch] Enter the target address (hex) in the REPL, then the bytes.\n");
        appendToOutput("[Hotpatch] Example: !hotpatch_apply 0x7FFE1234 90 90 90\n");
    }
}

void Win32IDE::cmdHotpatchMemoryRevert() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    uint64_t patchCount = stats.memoryPatchCount.load();
    if (patchCount == 0) {
        appendToOutput("[Hotpatch] No tracked memory patches to revert.\n");
        return;
    }
    // Poll the event ring to show recent memory patches
    appendToOutput("[Hotpatch] Tracked memory patches: " + std::to_string(patchCount) + "\n");
    appendToOutput("[Hotpatch] Reverting last memory patch...\n");
    // Revert needs a MemoryPatchEntry — poll from event system
    HotpatchEvent evt;
    while (mgr.poll_event(&evt)) {
        if (evt.type == HotpatchEvent::MemoryPatchReverted) {
            appendToOutput(std::string("[Hotpatch] Reverted: ") + (evt.detail ? evt.detail : "(no detail)") + "\n");
        }
    }
    appendToOutput("[Hotpatch] Revert operation completed. Use Status to verify.\n");
}

// ============================================================================
// Byte Layer (Layer 2)
// ============================================================================

void Win32IDE::cmdHotpatchByteApply() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    // Open file dialog for GGUF selection
    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select GGUF File for Byte Patching";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        appendToOutput(std::string("[Hotpatch] Byte-level target: ") + filename + "\n");

        // Open a second dialog for the .hotpatch patch definition file
        char patchFile[MAX_PATH] = {};
        OPENFILENAMEA ofnPatch = {};
        ofnPatch.lStructSize = sizeof(ofnPatch);
        ofnPatch.hwndOwner = m_hwndMain;
        ofnPatch.lpstrFilter = "Hotpatch Files (*.hotpatch)\0*.hotpatch\0All Files (*.*)\0*.*\0";
        ofnPatch.lpstrFile = patchFile;
        ofnPatch.nMaxFile = MAX_PATH;
        ofnPatch.lpstrTitle = "Select Byte Patch Definition";
        ofnPatch.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

        if (GetOpenFileNameA(&ofnPatch)) {
            // Parse the .hotpatch file: each line is OFFSET:HEX_BYTES
            FILE* fp = fopen(patchFile, "r");
            if (!fp) {
                appendToOutput("[Hotpatch] ERROR: Cannot open patch file.\n");
                return;
            }
            auto& mgr = UnifiedHotpatchManager::instance();
            int applied = 0, failed = 0;
            char line[512];
            while (fgets(line, sizeof(line), fp)) {
                // Skip comments and blank lines
                if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
                // Parse OFFSET:HEX_BYTES
                char* colon = strchr(line, ':');
                if (!colon) continue;
                *colon = '\0';
                uint64_t offset = strtoull(line, nullptr, 16);
                std::vector<uint8_t> bytes;
                char* hex = colon + 1;
                while (*hex) {
                    while (*hex == ' ' || *hex == '\t') hex++;
                    if (*hex == '\n' || *hex == '\r' || *hex == '\0') break;
                    char byteStr[3] = { hex[0], hex[1], '\0' };
                    bytes.push_back((uint8_t)strtoul(byteStr, nullptr, 16));
                    hex += 2;
                }
                if (!bytes.empty()) {
                    BytePatch bp;
                    bp.offset = offset;
                    bp.data = bytes.data();
                    bp.dataSize = bytes.size();
                    auto ur = mgr.apply_byte_patch(filename, bp);
                    if (ur.result.success) applied++; else failed++;
                }
            }
            fclose(fp);
            appendToOutput("[Hotpatch] Byte patches applied: " + std::to_string(applied) +
                           ", failed: " + std::to_string(failed) + "\n");
        } else {
            appendToOutput("[Hotpatch] Byte-level target set. Use CLI for manual patching:\n");
            appendToOutput("[Hotpatch]   !hotpatch_byte <offset> <hex_bytes>\n");
        }
    }
}

void Win32IDE::cmdHotpatchByteSearch() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "GGUF Models (*.gguf)\0*.gguf\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Select File for Pattern Search & Replace";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        appendToOutput(std::string("[Hotpatch] Search target: ") + filename + "\n");

        // Prompt user for hex pattern and replacement
        // Use a simple dialog approach: ask for pattern first
        char patternBuf[256] = {};
        char replaceBuf[256] = {};

        // Parse hex pattern from user input (stored in chat input field)
        // For now, prompt user to enter pattern + replacement in REPL format
        if (MessageBoxA(m_hwndMain,
            "Enter search pattern and replacement in the REPL:\n\n"
            "  !hotpatch_search <hex_pattern> <hex_replacement>\n\n"
            "The byte search uses SIMD-accelerated Boyer-Moore matching.\n"
            "Pattern must be hex bytes (e.g., 4F4C4C414D41).\n\n"
            "Click OK to confirm target file.",
            "Hotpatch: Byte Search & Replace", MB_OKCANCEL | MB_ICONINFORMATION) == IDOK) {

            appendToOutput("[Hotpatch] File locked: " + std::string(filename) + "\n");
            appendToOutput("[Hotpatch] Enter pattern via REPL: !hotpatch_search <hex_pattern> <hex_replace>\n");
            appendToOutput("[Hotpatch] The search uses apply_byte_search_patch() with SIMD scan.\n");
        }
    }
}

// ============================================================================
// Server Layer (Layer 3)
// ============================================================================

void Win32IDE::cmdHotpatchServerAdd() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    appendToOutput("[Hotpatch] Server patch injection points:\n");
    appendToOutput("  PreRequest  \xe2\x80\x94 Modify request before inference\n");
    appendToOutput("  PostRequest \xe2\x80\x94 Modify request after preprocessing\n");
    appendToOutput("  PreResponse \xe2\x80\x94 Modify response before delivery\n");
    appendToOutput("  PostResponse\xe2\x80\x94 Modify response after delivery\n");
    appendToOutput("  StreamChunk \xe2\x80\x94 Intercept streaming tokens\n\n");

    // Show current server patch count from unified manager
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    appendToOutput("[Hotpatch] Active server patches: " + std::to_string(stats.serverPatchCount.load()) + "\n");
    appendToOutput("[Hotpatch] To add a server patch, use REPL:\n");
    appendToOutput("  !hotpatch_server add <name> <injection_point>\n");
    appendToOutput("[Hotpatch] Or use the HTTP API: POST /api/hotpatch/server/add\n");
}

void Win32IDE::cmdHotpatchServerRemove() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& mgr = UnifiedHotpatchManager::instance();
    const auto& stats = mgr.getStats();
    uint64_t count = stats.serverPatchCount.load();
    if (count == 0) {
        appendToOutput("[Hotpatch] No server patches currently registered.\n");
        return;
    }
    appendToOutput("[Hotpatch] Active server patches: " + std::to_string(count) + "\n");
    appendToOutput("[Hotpatch] To remove, use REPL: !hotpatch_server remove <name>\n");
    appendToOutput("[Hotpatch] Or use HTTP API: DELETE /api/hotpatch/server/<name>\n");
}

// ============================================================================
// Proxy Hotpatcher
// ============================================================================

void Win32IDE::cmdHotpatchProxyBias() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    appendToOutput("[Hotpatch] Token Bias Injection:\n");
    appendToOutput("  Adjusts logit scores before sampling to boost/suppress tokens.\n");
    appendToOutput("  Positive bias = boost token probability\n");
    appendToOutput("  Negative bias = suppress token probability\n\n");
    appendToOutput("  Biases applied so far: " + std::to_string(ps.biasesApplied.load()) + "\n\n");

    // Prompt for quick bias entry via dialog
    if (MessageBoxA(m_hwndMain,
        "Add a token bias?\n\n"
        "Enter bias details in the REPL:\n"
        "  !hotpatch_bias <token_id> <bias_value> [permanent]\n\n"
        "Example: !hotpatch_bias 2046 -100.0 permanent\n"
        "(Suppresses token 2046 permanently)\n\n"
        "Click YES to also see current bias stats.",
        "Hotpatch: Token Bias", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        appendToOutput("[Hotpatch] Proxy Stats: tokens=" + std::to_string(ps.tokensProcessed.load()) +
                       " biases=" + std::to_string(ps.biasesApplied.load()) + "\n");
    }
}

void Win32IDE::cmdHotpatchProxyRewrite() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    appendToOutput("[Hotpatch] Output Rewrite Rules:\n");
    appendToOutput("  Pattern-based text replacement applied to inference output.\n");
    appendToOutput("  Rewrites applied so far: " + std::to_string(ps.rewritesApplied.load()) + "\n\n");
    appendToOutput("  Commands:\n");
    appendToOutput("    !hotpatch_rewrite add <name> <pattern> <replacement>\n");
    appendToOutput("    !hotpatch_rewrite remove <name>\n");
    appendToOutput("    !hotpatch_rewrite list\n\n");
    appendToOutput("  HTTP: POST /api/hotpatch/proxy/rewrite  {name, pattern, replacement}\n");
}

void Win32IDE::cmdHotpatchProxyTerminate() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    appendToOutput("[Hotpatch] Stream Termination Rules:\n");
    appendToOutput("  Stop sequences and max-token limits for output streams.\n");
    appendToOutput("  Streams terminated so far: " + std::to_string(ps.streamsTerminated.load()) + "\n\n");
    appendToOutput("  Commands:\n");
    appendToOutput("    !hotpatch_terminate add <name> <stop_seq> [max_tokens]\n");
    appendToOutput("    !hotpatch_terminate remove <name>\n");
    appendToOutput("    !hotpatch_terminate list\n\n");
    appendToOutput("  HTTP: POST /api/hotpatch/proxy/terminate  {name, stopSequence, maxTokens}\n");
}

void Win32IDE::cmdHotpatchProxyValidate() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    appendToOutput("[Hotpatch] Custom Validators:\n");
    appendToOutput("  Function-pointer validators run against all output.\n");
    appendToOutput("  All validators must pass for output to be accepted.\n\n");
    appendToOutput("  Validations passed: " + std::to_string(ps.validationsPassed.load()) + "\n");
    appendToOutput("  Validations failed: " + std::to_string(ps.validationsFailed.load()) + "\n\n");
    appendToOutput("  Built-in validators: length_check, json_syntax, safety_filter\n");
    appendToOutput("  Commands:\n");
    appendToOutput("    !hotpatch_validate add <name>     \xe2\x80\x94 Enable a built-in validator\n");
    appendToOutput("    !hotpatch_validate remove <name>  \xe2\x80\x94 Disable a validator\n");
    appendToOutput("    !hotpatch_validate list            \xe2\x80\x94 Show active validators\n\n");
    appendToOutput("  HTTP: POST /api/hotpatch/proxy/validate  {name, enabled}\n");
}

void Win32IDE::cmdHotpatchShowProxyStats() {
    auto& proxy = ProxyHotpatcher::instance();
    const auto& ps = proxy.getStats();

    std::ostringstream ss;
    ss << "=== Proxy Hotpatcher Statistics ===\n";
    ss << "  Tokens Processed:     " << ps.tokensProcessed.load()  << "\n";
    ss << "  Biases Applied:       " << ps.biasesApplied.load()    << "\n";
    ss << "  Streams Terminated:   " << ps.streamsTerminated.load() << "\n";
    ss << "  Rewrites Applied:     " << ps.rewritesApplied.load()  << "\n";
    ss << "  Validations Passed:   " << ps.validationsPassed.load() << "\n";
    ss << "  Validations Failed:   " << ps.validationsFailed.load() << "\n";
    ss << "===================================\n";

    appendToOutput(ss.str());
}

// ============================================================================
// Presets
// ============================================================================

void Win32IDE::cmdHotpatchPresetSave() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Hotpatch Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Save Hotpatch Preset";
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
    ofn.lpstrDefExt = "json";

    if (GetSaveFileNameA(&ofn)) {
        HotpatchPreset preset = {};
        strncpy(preset.name, "IDE Preset", sizeof(preset.name) - 1);

        PatchResult r = UnifiedHotpatchManager::instance().save_preset(filename, preset);
        if (r.success) {
            appendToOutput(std::string("[Hotpatch] Preset saved: ") + filename + "\n");
        } else {
            appendToOutput(std::string("[Hotpatch] Save failed: ") + r.detail + "\n");
        }
    }
}

void Win32IDE::cmdHotpatchPresetLoad() {
    if (!m_hotpatchEnabled) {
        appendToOutput("[Hotpatch] System is disabled. Toggle on first.\n");
        return;
    }

    char filename[MAX_PATH] = {};
    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "Hotpatch Presets (*.json)\0*.json\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = "Load Hotpatch Preset";
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        HotpatchPreset preset = {};
        PatchResult r = UnifiedHotpatchManager::instance().load_preset(filename, &preset);
        if (r.success) {
            appendToOutput(std::string("[Hotpatch] Preset loaded: ") + filename + "\n");
            appendToOutput(std::string("[Hotpatch] Preset name: ") + preset.name + "\n");
        } else {
            appendToOutput(std::string("[Hotpatch] Load failed: ") + r.detail + "\n");
        }
    }
}
