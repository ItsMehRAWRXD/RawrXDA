#pragma once
#include <windows.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <iostream>

struct PatchRecord {
    void* target_address;
    std::vector<unsigned char> original_bytes;
    std::vector<unsigned char> new_bytes;
    size_t size;
    bool active;
};

class HotPatcher {
private:
    std::unordered_map<std::string, PatchRecord> patches_;

public:
    HotPatcher() = default;
    ~HotPatcher(); // Restores all patches on destruction for stability

    // Core Operations
    bool ApplyPatch(const std::string& patch_name, void* target_address, const std::vector<unsigned char>& new_opcodes);
    bool RevertPatch(const std::string& patch_name);
    
    // Utilities
    void* GetFunctionAddress(const std::string& module_name, const std::string& function_name);
    
    // Dangerous: Live Code Rewriting
    // Scans memory for a signature and replaces it
    bool ScanAndPatch(const std::string& patch_name, const std::vector<unsigned char>& signature, const std::vector<unsigned char>& replacement);

    void ListPatches();
};
