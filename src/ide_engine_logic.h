#pragma once
#include <windows.h>
#include <vector>
#include <string>

// Core IDE Engine interfaces
extern "C" {
    void IDE_ApplyHotpatch(HWND hOutput, const char* patchName, void* addr, const unsigned char* opcodes, size_t size);
    void IDE_GenerateProject(HWND hOutput, const wchar_t* name, const wchar_t* type);
    void IDE_ShowMemoryStatus(HWND hOutput);
    void IDE_ExecuteAgentCmd(HWND hOutput, HWND hEditor, const wchar_t* cmd);
    void IDE_AnalyzePatches(HWND hOutput);
}
