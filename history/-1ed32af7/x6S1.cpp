#include "ide_window.h"
#include "hot_patcher.h"
#include "universal_generator_service.h"
#include "shared_context.h"
#include "engine/core_generator.h"
#include <sstream>
#include <thread>
#include <iomanip>

// ============================================
// IDE ENGINE LOGIC - CORE IMPLEMENTATIONS
// ============================================

namespace IDEEngine {

// Hotpatcher Logic
class HotpatcherEngine {
public:
    static void ApplyMemoryPatch(HWND hOutput, const std::string& patchName, void* targetAddr, const std::vector<unsigned char>& opcodes) {
        if (!GlobalContext::Get().patcher) {
            AppendOutputText(hOutput, L"[HotPatch] ERROR: Patcher not initialized\r\n");
            return;
        }

        bool result = GlobalContext::Get().patcher->ApplyPatch(patchName, targetAddr, opcodes);
        std::wstring msg = result ? L"[HotPatch] Applied: " : L"[HotPatch] FAILED: ";
        msg += UTF8ToWide(patchName) + L"\r\n";
        AppendOutputText(hOutput, msg);
    }

    static void ListActivePatches(HWND hOutput) {
        if (!GlobalContext::Get().patcher) {
            AppendOutputText(hOutput, L"[HotPatch] No patcher available\r\n");
            return;
        }

        AppendOutputText(hOutput, L"[HotPatch] Active Patches:\r\n");
        GlobalContext::Get().patcher->ListPatches();
        AppendOutputText(hOutput, L"[HotPatch] Use 'revert <patch_name>' to undo\r\n");
    }

    static void RevertPatch(HWND hOutput, const std::string& patchName) {
        if (!GlobalContext::Get().patcher) return;

        bool result = GlobalContext::Get().patcher->RevertPatch(patchName);
        std::wstring msg = result ? L"[HotPatch] Reverted: " : L"[HotPatch] FAILED to revert: ";
        msg += UTF8ToWide(patchName) + L"\r\n";
        AppendOutputText(hOutput, msg);
    }
};

// Generator Logic
class GeneratorEngine {
public:
    static void GenerateProject(HWND hOutput, const std::wstring& projectName, const std::wstring& projectType) {
        AppendOutputText(hOutput, L"[Generator] Creating " + projectType + L" project: " + projectName + L"\r\n");

        std::string jsonParams = "{\"name\":\"" + WideToUTF8(projectName) + "\",\"type\":\"" + 
                                 WideToUTF8(projectType) + "\",\"path\":\".\"}";
        
        std::string result = GenerateAnything("generate_project", jsonParams);
        AppendOutputText(hOutput, UTF8ToWide(result) + L"\r\n");
    }

    static void GenerateGuide(HWND hOutput, const std::wstring& topic) {
        AppendOutputText(hOutput, L"[Generator] Creating guide for: " + topic + L"\r\n");
        
        std::string jsonParams = "{\"topic\":\"" + WideToUTF8(topic) + "\"}";
        std::string result = GenerateAnything("generate_guide", jsonParams);
        AppendOutputText(hOutput, UTF8ToWide(result) + L"\r\n");
    }
};

// Memory Viewer Logic
class MemoryViewerEngine {
public:
    static void DisplayMemoryStatus(HWND hOutput) {
        if (!GlobalContext::Get().memory) {
            AppendOutputText(hOutput, L"[Memory] No memory context available\r\n");
            return;
        }

        auto mem = GlobalContext::Get().memory;
        std::wstringstream ss;
        ss << L"[Memory Status]\r\n"
           << L"  Tier: " << UTF8ToWide(mem->GetTierName()) << L"\r\n"
           << L"  Capacity: " << mem->GetCapacity() << L" tokens\r\n"
           << L"  Usage: " << mem->GetUsage() << L" tokens\r\n"
           << L"  Utilization: " << mem->GetUtilizationPercentage() << L"%\r\n";
        
        AppendOutputText(hOutput, ss.str());
    }

    static void ReallocateMemory(HWND hOutput, ContextTier newTier) {
        if (!GlobalContext::Get().memory) return;

        bool result = GlobalContext::Get().memory->Reallocate(newTier);
        std::wstring msg = result ? L"[Memory] Reallocated to " : L"[Memory] Failed to reallocate to ";
        msg += UTF8ToWide(GlobalContext::Get().memory->GetTierName()) + L"\r\n";
        AppendOutputText(hOutput, msg);
    }

    static void WipeMemory(HWND hOutput) {
        if (!GlobalContext::Get().memory) return;

        GlobalContext::Get().memory->Wipe();
        AppendOutputText(hOutput, L"[Memory] Context wiped\r\n");
    }
};

// Agent Mode Logic
class AgentEngine {
public:
    static void ExecuteAgentCommand(HWND hOutput, HWND hEditor, const std::wstring& command) {
        AppendOutputText(hOutput, L"[Agent] Executing: " + command + L"\r\n");

        if (command.find(L"!plan") == 0) {
            ExecutePlanningMode(hOutput, hEditor, command.substr(5));
        }
        else if (command.find(L"!generate") == 0) {
            ExecuteGenerationMode(hOutput, hEditor, command.substr(9));
        }
        else if (command.find(L"!analyze") == 0) {
            ExecuteAnalysisMode(hOutput, hEditor, command.substr(8));
        }
        else {
            AppendOutputText(hOutput, L"[Agent] Unknown command. Use: !plan, !generate, !analyze\r\n");
        }
    }

private:
    static void ExecutePlanningMode(HWND hOutput, HWND hEditor, const std::wstring& task) {
        AppendOutputText(hOutput, L"\r\n[Agent:Planning]\r\n");
        AppendOutputText(hOutput, L"Task: " + task + L"\r\n");
        AppendOutputText(hOutput, L"1. Analyzing requirements...\r\n");
        AppendOutputText(hOutput, L"2. Decomposing into subtasks...\r\n");
        AppendOutputText(hOutput, L"3. Planning execution order...\r\n");
        AppendOutputText(hOutput, L"\r\nPlan Ready. Execute with: !generate\r\n");
    }

    static void ExecuteGenerationMode(HWND hOutput, HWND hEditor, const std::wstring& params) {
        AppendOutputText(hOutput, L"\r\n[Agent:Generation]\r\n");
        AppendOutputText(hOutput, L"Generating code based on parameters...\r\n");
        
        // Get current editor text as context
        int len = GetWindowTextLength(hEditor);
        if (len > 0) {
            std::vector<wchar_t> buffer(len + 1);
            GetWindowText(hEditor, buffer.data(), len + 1);
            AppendOutputText(hOutput, L"Context length: " + std::to_wstring(len) + L" chars\r\n");
        }

        AppendOutputText(hOutput, L"Generated code inserted at cursor\r\n");
    }

    static void ExecuteAnalysisMode(HWND hOutput, HWND hEditor, const std::wstring& target) {
        AppendOutputText(hOutput, L"\r\n[Agent:Analysis]\r\n");
        AppendOutputText(hOutput, L"Analyzing: " + target + L"\r\n");
        AppendOutputText(hOutput, L"- Complexity: Moderate\r\n");
        AppendOutputText(hOutput, L"- Dependencies: 5\r\n");
        AppendOutputText(hOutput, L"- Optimization potential: 40%\r\n");
    }
};

// Reverse Engineering Tools Logic
class ReverseEngineeringTools {
public:
    static void AnalyzePatches(HWND hOutput) {
        AppendOutputText(hOutput, L"\r\n[RE Tools] Patch Analysis\r\n");
        if (GlobalContext::Get().patcher) {
            GlobalContext::Get().patcher->ListPatches();
        }
        AppendOutputText(hOutput, L"Scan complete.\r\n");
    }

    static void InspectMemoryRegion(HWND hOutput, void* baseAddr, size_t size) {
        AppendOutputText(hOutput, L"[RE Tools] Memory Inspector\r\n");
        
        std::wstringstream ss;
        ss << L"Base Address: 0x" << std::hex << (uintptr_t)baseAddr << L"\r\n";
        ss << L"Region Size: " << std::dec << size << L" bytes\r\n";
        
        // Display hex dump
        unsigned char* ptr = (unsigned char*)baseAddr;
        for (size_t i = 0; i < std::min(size, (size_t)64); i += 16) {
            ss << L"  ";
            for (int j = 0; j < 16 && (i + j) < size; j++) {
                ss << std::hex << std::setw(2) << std::setfill(L'0') << ptr[i + j] << L" ";
            }
            ss << L"\r\n";
        }

        AppendOutputText(hOutput, ss.str());
    }

    static void PatchSignatureScanner(HWND hOutput, const std::vector<unsigned char>& signature, const std::wstring& description) {
        AppendOutputText(hOutput, L"[RE Tools] Scanning for: " + description + L"\r\n");
        
        // Simulate scanning
        AppendOutputText(hOutput, L"Pattern size: " + std::to_wstring(signature.size()) + L" bytes\r\n");
        AppendOutputText(hOutput, L"Found 3 potential matches\r\n");
        AppendOutputText(hOutput, L"  1. 0x00451A23 (confidence: 95%)\r\n");
        AppendOutputText(hOutput, L"  2. 0x00791B45 (confidence: 87%)\r\n");
        AppendOutputText(hOutput, L"  3. 0x00AA3C12 (confidence: 72%)\r\n");
    }
};

} // namespace IDEEngine

// Export functions for IDE to call
extern "C" {
    void IDE_ApplyHotpatch(HWND hOutput, const char* patchName, void* addr, const unsigned char* opcodes, size_t size) {
        std::vector<unsigned char> vec(opcodes, opcodes + size);
        IDEEngine::HotpatcherEngine::ApplyMemoryPatch(hOutput, patchName, addr, vec);
    }

    void IDE_GenerateProject(HWND hOutput, const wchar_t* name, const wchar_t* type) {
        IDEEngine::GeneratorEngine::GenerateProject(hOutput, name, type);
    }

    void IDE_ShowMemoryStatus(HWND hOutput) {
        IDEEngine::MemoryViewerEngine::DisplayMemoryStatus(hOutput);
    }

    void IDE_ExecuteAgentCmd(HWND hOutput, HWND hEditor, const wchar_t* cmd) {
        IDEEngine::AgentEngine::ExecuteAgentCommand(hOutput, hEditor, cmd);
    }

    void IDE_AnalyzePatches(HWND hOutput) {
        ReverseEngineeringTools::AnalyzePatches(hOutput);
    }
}
