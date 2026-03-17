// ============================================================================
// vscext_registry.h — Core VS Code Extension API registry (SSOT + Win32IDE)
// ============================================================================
// Batch #N: Real implementations for vscext.* (10000–10009).
// Both SSOT handlers and Win32IDE cmdVSCExt* call these; no GUI-only shims.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <string>
#include <cstddef>

namespace VscextRegistry {

// ---- List Commands (10002) ----
// Enumerate registered command IDs; append formatted lines to \a out.
// Returns true if API is initialized and at least one line was written.
bool listCommands(std::string& out);

// ---- List Providers (10003) ----
// Enumerate provider type counts; append to \a out.
bool listProviders(std::string& out);

// ---- Diagnostics (10004) ----
// Host state, loaded count, last error, stats; append to \a out.
bool getDiagnosticsReport(std::string& out);

// ---- Extensions (10005) ----
// Installed extensions (id, version, active); append to \a out.
bool listExtensions(std::string& out);

// ---- Stats (10006) ----
// JSON counters (activations, commands, etc.); append to \a out.
bool getStatsJson(std::string& out);

// ---- Reload (10001) ----
// Call registered reload callback if set (IDE sets it). Returns true if reload
// was invoked, false if no callback or API not initialized.
bool reload();
// Register reload callback (Win32IDE calls this from initVSCodeExtensionAPI).
void setReloadCallback(void (*fn)());

// ---- Export Config (10009) ----
// Write enabled/disabled list, stats snapshot, install roots to .rawrxd/vscext_export.json.
// \a outPath receives the written file path; \a outSummary receives a one-line summary.
// Returns true if file was written.
bool exportConfig(std::string& outPath, std::string& outSummary);

// ---- Status (10000) ----
// Single-line status string; append to \a out.
bool getStatusString(std::string& out);

} // namespace VscextRegistry
