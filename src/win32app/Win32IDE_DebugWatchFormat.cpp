// ============================================================================
// Win32IDE_DebugWatchFormat.cpp — Tier 5 Gap #43: Debug Watch Window Formatting
// ============================================================================
//
// PURPOSE:
//   Adds structured object inspection with type visualizers to the native
//   debug panel.  Replaces raw hex/dec/memory-dump display with formatted
//   structured views for common types (std::vector, std::string, structs,
//   arrays, pointers, etc.).
//
// Architecture: C++20 | Win32 | No exceptions | No Qt
// Pattern:      PatchResult-compatible returns
// Rule:         NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "Win32IDE.h"
#include <commctrl.h>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <unordered_map>
#include <functional>
#include <cstdint>

// ============================================================================
// Type Visualizer Framework
// ============================================================================

enum class WatchDisplayFormat {
    Auto,           // Detect and format automatically
    Decimal,
    Hexadecimal,
    Binary,
    Character,
    String,
    Array,
    Struct,
    Pointer,
    Float,
    Boolean
};

struct WatchVariable {
    std::string  name;
    std::string  type;
    std::string  value;         // raw value
    std::string  displayValue;  // formatted for display
    uint64_t     address;
    uint32_t     size;
    WatchDisplayFormat format;
    bool         expanded;
    int          depth;
    std::vector<WatchVariable> children;
};

// ============================================================================
// Type Visualizer Registry
// ============================================================================

struct TypeVisualizer {
    std::string  typeName;   // e.g. "std::vector", "std::string"
    std::string  displayPattern;
    bool (*formatFn)(const WatchVariable& var, std::string& outDisplay);
};

static std::vector<TypeVisualizer> s_typeVisualizers;

static bool formatStdString(const WatchVariable& var, std::string& out) {
    // Display std::string as: "content" (length=N)
    if (var.value.size() >= 2 && var.value.front() == '"' && var.value.back() == '"') {
        out = var.value + " (length=" + std::to_string(var.value.size() - 2) + ")";
    } else {
        out = "\"" + var.value + "\" (length=" + std::to_string(var.value.size()) + ")";
    }
    return true;
}

static bool formatStdVector(const WatchVariable& var, std::string& out) {
    // Display vector as: vector<T> [size=N, capacity=M]
    int childCount = (int)var.children.size();
    out = var.type + " [size=" + std::to_string(childCount) + "]";
    return true;
}

static bool formatPointer(const WatchVariable& var, std::string& out) {
    // Display pointer as: 0xADDRESS → type
    char buf[64];
    snprintf(buf, sizeof(buf), "0x%016llX", (unsigned long long)var.address);
    out = std::string(buf);
    if (!var.type.empty()) {
        out += " → " + var.type;
    }
    if (var.address == 0) {
        out += " (nullptr)";
    }
    return true;
}

static bool formatArray(const WatchVariable& var, std::string& out) {
    // Display array as: type[N] = { elem0, elem1, ... }
    std::ostringstream oss;
    oss << var.type << "[" << var.children.size() << "] = { ";
    for (size_t i = 0; i < var.children.size() && i < 5; ++i) {
        if (i > 0) oss << ", ";
        oss << var.children[i].value;
    }
    if (var.children.size() > 5) oss << ", ...";
    oss << " }";
    out = oss.str();
    return true;
}

static bool formatStruct(const WatchVariable& var, std::string& out) {
    // Display struct as: TypeName { field1=val1, field2=val2, ... }
    std::ostringstream oss;
    oss << var.type << " { ";
    for (size_t i = 0; i < var.children.size() && i < 4; ++i) {
        if (i > 0) oss << ", ";
        oss << var.children[i].name << "=" << var.children[i].value;
    }
    if (var.children.size() > 4) oss << ", ...";
    oss << " }";
    out = oss.str();
    return true;
}

static bool formatBoolean(const WatchVariable& var, std::string& out) {
    if (var.value == "1" || var.value == "true" || var.value == "TRUE") {
        out = "true ✓";
    } else {
        out = "false ✗";
    }
    return true;
}

static bool formatHex(const WatchVariable& var, std::string& out) {
    uint64_t val = 0;
    if (sscanf(var.value.c_str(), "%llu", &val) == 1 ||
        sscanf(var.value.c_str(), "0x%llx", &val) == 1) {
        char buf[32];
        snprintf(buf, sizeof(buf), "0x%llX (%llu)",
                 (unsigned long long)val, (unsigned long long)val);
        out = buf;
        return true;
    }
    return false;
}

static bool formatBinary(const WatchVariable& var, std::string& out) {
    uint64_t val = 0;
    if (sscanf(var.value.c_str(), "%llu", &val) == 1 ||
        sscanf(var.value.c_str(), "0x%llx", &val) == 1) {
        std::string bits;
        bool started = false;
        for (int i = 63; i >= 0; --i) {
            if (val & (1ULL << i)) {
                started = true;
                bits += '1';
            } else if (started) {
                bits += '0';
            }
        }
        if (bits.empty()) bits = "0";
        out = "0b" + bits + " (" + std::to_string(val) + ")";
        return true;
    }
    return false;
}

// ============================================================================
// Initialize visualizers
// ============================================================================

static void ensureVisualizersRegistered() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;

    s_typeVisualizers.push_back({"std::string",        "{s}", formatStdString});
    s_typeVisualizers.push_back({"std::basic_string",  "{s}", formatStdString});
    s_typeVisualizers.push_back({"std::vector",        "[{size}]", formatStdVector});
    s_typeVisualizers.push_back({"*",                  "0x{addr}", formatPointer});
    s_typeVisualizers.push_back({"[]",                 "[{N}]", formatArray});
    s_typeVisualizers.push_back({"bool",               "{?}", formatBoolean});
    s_typeVisualizers.push_back({"BOOL",               "{?}", formatBoolean});
}

// ============================================================================
// Apply type visualizer to a watch variable
// ============================================================================

static std::string applyVisualizer(const WatchVariable& var) {
    ensureVisualizersRegistered();

    // Try specific type matches first
    for (auto& vis : s_typeVisualizers) {
        if (var.type.find(vis.typeName) != std::string::npos) {
            std::string result;
            if (vis.formatFn(var, result)) return result;
        }
    }

    // Format by explicit display format
    switch (var.format) {
        case WatchDisplayFormat::Hexadecimal: {
            std::string r;
            if (formatHex(var, r)) return r;
            break;
        }
        case WatchDisplayFormat::Binary: {
            std::string r;
            if (formatBinary(var, r)) return r;
            break;
        }
        case WatchDisplayFormat::Boolean: {
            std::string r;
            if (formatBoolean(var, r)) return r;
            break;
        }
        case WatchDisplayFormat::Struct: {
            std::string r;
            if (formatStruct(var, r)) return r;
            break;
        }
        case WatchDisplayFormat::Array: {
            std::string r;
            if (formatArray(var, r)) return r;
            break;
        }
        default:
            break;
    }

    // Default: value + type annotation
    if (var.type.empty()) return var.value;
    return var.value + "  (" + var.type + ")";
}

// ============================================================================
// Watch window state
// ============================================================================

static std::vector<WatchVariable> s_watchVars;

// ============================================================================
// Initialization
// ============================================================================

void Win32IDE::initDebugWatchFormat() {
    if (m_debugWatchFormatInitialized) return;
    ensureVisualizersRegistered();
    OutputDebugStringA("[DebugWatch] Tier 5 — Type visualizer support initialized.\n");
    m_debugWatchFormatInitialized = true;
    appendToOutput("[DebugWatch] Structured object inspection enabled.\n");
}

// ============================================================================
// Command Router
// ============================================================================

bool Win32IDE::handleDebugWatchFormatCommand(int commandId) {
    if (!m_debugWatchFormatInitialized) initDebugWatchFormat();
    switch (commandId) {
        case IDM_DBGWATCH_SHOW:       cmdDbgWatchShow();       return true;
        case IDM_DBGWATCH_ADD:        cmdDbgWatchAdd();        return true;
        case IDM_DBGWATCH_FORMAT_HEX: cmdDbgWatchFormatHex();  return true;
        case IDM_DBGWATCH_FORMAT_DEC: cmdDbgWatchFormatDec();  return true;
        case IDM_DBGWATCH_FORMAT_BIN: cmdDbgWatchFormatBin();  return true;
        case IDM_DBGWATCH_FORMAT_STRUCT: cmdDbgWatchFormatStruct(); return true;
        case IDM_DBGWATCH_CLEAR:      cmdDbgWatchClear();      return true;
        default: return false;
    }
}

// ============================================================================
// Show formatted watch variables
// ============================================================================

void Win32IDE::cmdDbgWatchShow() {
    std::ostringstream oss;
    oss << "╔══════════════════════════════════════════════════════════════╗\n"
        << "║              DEBUG WATCH WINDOW (Formatted)                ║\n"
        << "╠══════════════════════════════════════════════════════════════╣\n";

    if (s_watchVars.empty()) {
        oss << "║  (no watch variables)                                      ║\n";
    } else {
        for (size_t i = 0; i < s_watchVars.size(); ++i) {
            auto& v = s_watchVars[i];
            std::string formatted = applyVisualizer(v);

            char line[256];
            std::string indent(v.depth * 2, ' ');
            snprintf(line, sizeof(line), "║  %s%-20s = %-35s ║\n",
                     indent.c_str(), v.name.c_str(), formatted.c_str());
            oss << line;

            // Show children if expanded
            if (v.expanded) {
                for (auto& child : v.children) {
                    std::string childFormatted = applyVisualizer(child);
                    snprintf(line, sizeof(line), "║    ├─ %-18s = %-33s ║\n",
                             child.name.c_str(), childFormatted.c_str());
                    oss << line;
                }
            }
        }
    }

    oss << "╚══════════════════════════════════════════════════════════════╝\n";
    appendToOutput(oss.str());
}

// ============================================================================
// Add sample watch variables (for demo/testing)
// ============================================================================

void Win32IDE::cmdDbgWatchAdd() {
    // Add sample variables demonstrating type visualizers
    WatchVariable v1;
    v1.name = "greeting"; v1.type = "std::string";
    v1.value = "Hello, World!"; v1.format = WatchDisplayFormat::Auto;
    v1.address = 0x00007FF6A1234000; v1.depth = 0;
    s_watchVars.push_back(v1);

    WatchVariable v2;
    v2.name = "items"; v2.type = "std::vector<int>";
    v2.value = ""; v2.format = WatchDisplayFormat::Auto;
    v2.address = 0x00007FF6A1235000; v2.depth = 0;
    v2.expanded = true;
    // Add children
    for (int i = 0; i < 5; ++i) {
        WatchVariable elem;
        elem.name = "[" + std::to_string(i) + "]";
        elem.type = "int";
        elem.value = std::to_string((i + 1) * 10);
        elem.format = WatchDisplayFormat::Decimal;
        elem.depth = 1;
        v2.children.push_back(elem);
    }
    s_watchVars.push_back(v2);

    WatchVariable v3;
    v3.name = "ptr"; v3.type = "void*";
    v3.value = "0x00007FF6A1236000";
    v3.format = WatchDisplayFormat::Hexadecimal;
    v3.address = 0x00007FF6A1236000; v3.depth = 0;
    s_watchVars.push_back(v3);

    WatchVariable v4;
    v4.name = "flags"; v4.type = "uint32_t";
    v4.value = "255"; v4.format = WatchDisplayFormat::Auto;
    v4.address = 0; v4.depth = 0;
    s_watchVars.push_back(v4);

    WatchVariable v5;
    v5.name = "isReady"; v5.type = "bool";
    v5.value = "true"; v5.format = WatchDisplayFormat::Boolean;
    v5.address = 0; v5.depth = 0;
    s_watchVars.push_back(v5);

    appendToOutput("[DebugWatch] Added 5 sample watch variables.\n");
    cmdDbgWatchShow();
}

// ============================================================================
// Format commands
// ============================================================================

void Win32IDE::cmdDbgWatchFormatHex() {
    for (auto& v : s_watchVars) v.format = WatchDisplayFormat::Hexadecimal;
    appendToOutput("[DebugWatch] Display format: Hexadecimal\n");
    cmdDbgWatchShow();
}

void Win32IDE::cmdDbgWatchFormatDec() {
    for (auto& v : s_watchVars) v.format = WatchDisplayFormat::Decimal;
    appendToOutput("[DebugWatch] Display format: Decimal\n");
    cmdDbgWatchShow();
}

void Win32IDE::cmdDbgWatchFormatBin() {
    for (auto& v : s_watchVars) v.format = WatchDisplayFormat::Binary;
    appendToOutput("[DebugWatch] Display format: Binary\n");
    cmdDbgWatchShow();
}

void Win32IDE::cmdDbgWatchFormatStruct() {
    for (auto& v : s_watchVars) v.format = WatchDisplayFormat::Struct;
    appendToOutput("[DebugWatch] Display format: Struct\n");
    cmdDbgWatchShow();
}

void Win32IDE::cmdDbgWatchClear() {
    s_watchVars.clear();
    appendToOutput("[DebugWatch] Watch variables cleared.\n");
}
