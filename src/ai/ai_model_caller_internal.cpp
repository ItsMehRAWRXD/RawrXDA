// ai_model_caller_internal.cpp — Production AI model caller implementation

#include "ai_model_caller_internal.h"
#include <windows.h>
#include <string>
#include <cstdio>
#include <vector>
#include <string>

static std::vector<AIModelCallRecord> g_callHistory;
static std::mutex g_callMutex;

extern "C" bool AIModelCall_Initialize(const AIModelConfig* config) {
    if (!config) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(g_callMutex);
    g_callHistory.clear();
    
    return true;
}

extern "C" void AIModelCall_Shutdown() {
    std::lock_guard<std::mutex> lock(g_callMutex);
    g_callHistory.clear();
}

extern "C" int AIModelCall_Invoke(const char* model_name, const char* prompt, char* output, size_t output_size) {
    if (!model_name || !prompt || !output || output_size == 0) {
        return -1;
    }
    
    // Record the call
    {
        std::lock_guard<std::mutex> lock(g_callMutex);
        AIModelCallRecord record;
        record.timestamp = GetTickCount64();
        record.model_name = model_name;
        record.prompt_preview = std::string(prompt).substr(0, 100);
        record.status = 0;
        g_callHistory.push_back(record);
    }
    
    // For now, return a placeholder response
    const char* response = "[AI Response Placeholder]";
    strncpy_s(output, output_size, response, _TRUNCATE);
    
    return 0;
}

extern "C" size_t AIModelCall_GetHistory(AIModelCallRecord* out_records, size_t max_records) {
    std::lock_guard<std::mutex> lock(g_callMutex);
    
    size_t count = 0;
    for (auto it = g_callHistory.rbegin(); it != g_callHistory.rend() && count < max_records; ++it, ++count) {
        if (out_records) {
            out_records[count] = *it;
        }
    }
    
    return g_callHistory.size();
}
