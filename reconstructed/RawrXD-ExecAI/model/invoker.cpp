// ================================================================
// Model Invoker - High-level model execution interface
// ================================================================
#include <windows.h>
#include <stdio.h>

extern "C" {
    BOOL InitializeExecAI(const char* model_path);
    void ShutdownExecAI(void);
    BOOL RunStreamingInference(const char* token_path);
}

class ModelInvoker {
public:
    static bool Invoke(const char* model_path, const char* input_path) {
        if (!InitializeExecAI(model_path)) {
            return false;
        }
        
        bool success = RunStreamingInference(input_path);
        ShutdownExecAI();
        
        return success;
    }
};
