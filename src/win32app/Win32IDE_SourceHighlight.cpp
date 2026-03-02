// Win32IDE_SourceHighlight.cpp - Source-Line Highlight Wiring
// Bridges debugger stepping/breakpoints with editor UI (gutter + scroll)
#include <windows.h>
#include <string>
#include <vector>
#include <mutex>
#include <map>
#include <algorithm>

// Forward declarations from debugger
extern "C" {
    // From native_debugger_source_step.cpp
    int RawrXD_Debugger_SourceLineFromAddress(unsigned long long address);
    const char* RawrXD_Debugger_SourceFileFromAddress(unsigned long long address);
}

namespace {
    std::mutex g_highlightMutex;
    
    // Unified debug location struct (as recommended)
    struct DebugLocation {
        std::string filePath;
        int line = 0;
        int column = 0;
        void* instructionPtr = nullptr;
        DWORD threadId = 0;
        int frameId = 0;
    };
    
    DebugLocation g_currentLocation;
    
    struct BreakpointMarker {
        std::string filePath;
        int line = 0;
        bool enabled = true;
        bool verified = false; // set to true once debugger confirms
        void* address = nullptr;
    };
    
    std::vector<BreakpointMarker> g_breakpoints;
    
    // Callback function pointers (set by UI layer)
    using HighlightCallback = void (*)(const char* filePath, int line, bool scrollIntoView);
    using GutterUpdateCallback = void (*)(const char* filePath, int line, bool hasBreakpoint, bool enabled);
    
    HighlightCallback g_highlightCallback = nullptr;
    GutterUpdateCallback g_gutterCallback = nullptr;
    
    void notifyHighlight(const DebugLocation& loc, bool scroll) {
        if (g_highlightCallback && !loc.filePath.empty() && loc.line > 0) {
            g_highlightCallback(loc.filePath.c_str(), loc.line, scroll);
        }
    }
    
    void notifyGutterUpdate(const std::string& file, int line, bool hasBp, bool enabled) {
        if (g_gutterCallback && !file.empty() && line > 0) {
            g_gutterCallback(file.c_str(), line, hasBp, enabled);
        }
    }
}

extern "C" {

// Register UI callbacks (called during IDE init)
void RawrXD_SourceHighlight_SetCallbacks(
    void (*highlightFn)(const char* filePath, int line, bool scroll),
    void (*gutterFn)(const char* filePath, int line, bool hasBp, bool enabled)
) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    g_highlightCallback = highlightFn;
    g_gutterCallback = gutterFn;
}

// Update current debug location (called by debugger on step/break)
void RawrXD_SourceHighlight_UpdateLocation(
    const char* filePath,
    int line,
    int column,
    void* instructionPtr,
    DWORD threadId,
    int frameId
) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    g_currentLocation.filePath = filePath ? filePath : "";
    g_currentLocation.line = line;
    g_currentLocation.column = column;
    g_currentLocation.instructionPtr = instructionPtr;
    g_currentLocation.threadId = threadId;
    g_currentLocation.frameId = frameId;
    
    // Notify UI to highlight + scroll
    notifyHighlight(g_currentLocation, true);
}

// Update location from instruction pointer (reverse lookup)
void RawrXD_SourceHighlight_UpdateFromIP(void* instructionPtr, DWORD threadId, int frameId) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    // Reverse lookup: IP → file + line
    const unsigned long long ip = static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(instructionPtr));
    const char* file = RawrXD_Debugger_SourceFileFromAddress(ip);
    int line = RawrXD_Debugger_SourceLineFromAddress(ip);
    
    if (file && line > 0) {
        g_currentLocation.filePath = file;
        g_currentLocation.line = line;
        g_currentLocation.column = 1; // default
        g_currentLocation.instructionPtr = instructionPtr;
        g_currentLocation.threadId = threadId;
        g_currentLocation.frameId = frameId;
        
        notifyHighlight(g_currentLocation, true);
    }
}

// Get current debug location (for UI queries)
const char* RawrXD_SourceHighlight_GetCurrentLocation() {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    static std::string json;
    json = "{\"file\": \"" + g_currentLocation.filePath + 
           "\", \"line\": " + std::to_string(g_currentLocation.line) +
           ", \"column\": " + std::to_string(g_currentLocation.column) +
           ", \"threadId\": " + std::to_string(g_currentLocation.threadId) +
           ", \"frameId\": " + std::to_string(g_currentLocation.frameId) + "}";
    return json.c_str();
}

// Clear current highlight (on debug stop)
void RawrXD_SourceHighlight_Clear() {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    g_currentLocation.filePath.clear();
    g_currentLocation.line = 0;
    g_currentLocation.column = 0;
    g_currentLocation.instructionPtr = nullptr;
    g_currentLocation.threadId = 0;
    g_currentLocation.frameId = 0;
    
    // Notify UI to clear highlight
    if (g_highlightCallback) {
        g_highlightCallback("", 0, false);
    }
}

// Breakpoint management: toggle at file:line
void RawrXD_SourceHighlight_ToggleBreakpoint(const char* filePath, int line) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    if (!filePath || line <= 0) return;
    
    // Find existing BP
    auto it = std::find_if(g_breakpoints.begin(), g_breakpoints.end(),
        [&](const BreakpointMarker& bm) {
            return bm.filePath == filePath && bm.line == line;
        });
    
    if (it != g_breakpoints.end()) {
        // Remove breakpoint
        g_breakpoints.erase(it);
        notifyGutterUpdate(filePath, line, false, false);
    } else {
        // Add breakpoint
        BreakpointMarker bm;
        bm.filePath = filePath;
        bm.line = line;
        bm.enabled = true;
        bm.verified = false; // will be verified by debugger
        g_breakpoints.push_back(bm);
        notifyGutterUpdate(filePath, line, true, true);
    }
}

// Set breakpoint enabled/disabled
void RawrXD_SourceHighlight_SetBreakpointEnabled(const char* filePath, int line, bool enabled) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    if (!filePath || line <= 0) return;
    
    auto it = std::find_if(g_breakpoints.begin(), g_breakpoints.end(),
        [&](const BreakpointMarker& bm) {
            return bm.filePath == filePath && bm.line == line;
        });
    
    if (it != g_breakpoints.end()) {
        it->enabled = enabled;
        notifyGutterUpdate(filePath, line, true, enabled);
    }
}

// Mark breakpoint as verified (debugger confirmed address)
void RawrXD_SourceHighlight_VerifyBreakpoint(const char* filePath, int line, void* address) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    if (!filePath || line <= 0) return;
    
    auto it = std::find_if(g_breakpoints.begin(), g_breakpoints.end(),
        [&](const BreakpointMarker& bm) {
            return bm.filePath == filePath && bm.line == line;
        });
    
    if (it != g_breakpoints.end()) {
        it->verified = true;
        it->address = address;
    }
}

// Get all breakpoints for a file (for gutter rendering)
const char* RawrXD_SourceHighlight_GetBreakpoints(const char* filePath) {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    if (!filePath) {
        static std::string empty = "[]";
        return empty.c_str();
    }
    
    static std::string json;
    json = "[";
    
    bool first = true;
    for (const auto& bm : g_breakpoints) {
        if (bm.filePath == filePath) {
            if (!first) json += ", ";
            json += "{\"line\": " + std::to_string(bm.line) +
                   ", \"enabled\": " + (bm.enabled ? "true" : "false") +
                   ", \"verified\": " + (bm.verified ? "true" : "false") + "}";
            first = false;
        }
    }
    
    json += "]";
    return json.c_str();
}

// Get all breakpoints (for debugger consumption)
const char* RawrXD_SourceHighlight_GetAllBreakpoints() {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    static std::string json;
    json = "[";
    
    for (size_t i = 0; i < g_breakpoints.size(); ++i) {
        const auto& bm = g_breakpoints[i];
        if (i > 0) json += ", ";
        json += "{\"file\": \"" + bm.filePath + 
               "\", \"line\": " + std::to_string(bm.line) +
               ", \"enabled\": " + (bm.enabled ? "true" : "false") +
               ", \"verified\": " + (bm.verified ? "true" : "false") + "}";
    }
    
    json += "]";
    return json.c_str();
}

// Remove all breakpoints (for cleanup)
void RawrXD_SourceHighlight_ClearAllBreakpoints() {
    std::lock_guard<std::mutex> lock(g_highlightMutex);
    
    // Notify UI for each removed BP
    for (const auto& bm : g_breakpoints) {
        notifyGutterUpdate(bm.filePath, bm.line, false, false);
    }
    
    g_breakpoints.clear();
}

// Scroll to location without highlighting (for "Go to Definition" etc.)
void RawrXD_SourceHighlight_ScrollTo(const char* filePath, int line) {
    if (!filePath || line <= 0) return;
    
    // Lightweight scroll (no debugger state change)
    if (g_highlightCallback) {
        g_highlightCallback(filePath, line, true);
    }
}

} // extern "C"
