struct ExecutionResult {
    bool success;
    const char* errorMessage;
};

static ExecutionResult g_lastRes = {true, (const char*)"Feature generated successfully (Pure C Bridge Stub)"};

extern "C" {
void* QuantumOrchestrator_ExecuteTaskAuto(const char* prompt, const char* context) { return &g_lastRes; }
const char* ExecutionResult_GetDetail(void* res) { return ((ExecutionResult*)res)->errorMessage; }
void* MultiFileSessionTracker_CreateSession(const char* root) { return (void*)0x1234; }
void MultiFileSessionTracker_StageEdit(void* session, const char* path, const char* content) {}
}