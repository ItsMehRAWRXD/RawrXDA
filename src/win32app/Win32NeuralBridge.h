// Win32NeuralBridge.h — Stub for build compatibility
// Provides minimal NeuralBridge namespace for Win32IDE compilation

#pragma once

namespace NeuralBridge {
    inline bool IsInitialized() { return false; }
    inline bool Initialize(const char*) { return false; }
    inline bool RunSmokeTest(std::string* outReport) { if(outReport) *outReport = "stub"; return false; }
    inline void Shutdown() {}
}
