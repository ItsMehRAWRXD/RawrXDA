// Minimal C bridge implementation that is deterministic and non-crashing.
#include <string>
#include <unordered_map>

struct ExecutionResult {
    bool success;
    const char* errorMessage;
};

namespace {
ExecutionResult g_lastRes = {true, "Task accepted"};
std::string g_lastDetail = "Task accepted";

struct SessionState {
    std::string root;
    std::unordered_map<std::string, std::string> stagedEdits;
};

std::unordered_map<void*, SessionState> g_sessions;
}

extern "C" {
void* QuantumOrchestrator_ExecuteTaskAuto(const char* prompt, const char* context) {
    g_lastDetail = "Task accepted";
    if (!prompt || prompt[0] == '\0') {
        g_lastRes.success = false;
        g_lastDetail = "Prompt is empty";
    } else {
        g_lastRes.success = true;
        g_lastDetail = "Task accepted for autonomous execution";
        if (context && context[0] != '\0') {
            g_lastDetail += " with context";
        }
    }
    g_lastRes.errorMessage = g_lastDetail.c_str();
    return &g_lastRes;
}

const char* ExecutionResult_GetDetail(void* res) {
    if (!res) {
        return "ExecutionResult is null";
    }
    return static_cast<ExecutionResult*>(res)->errorMessage;
}

void* MultiFileSessionTracker_CreateSession(const char* root) {
    SessionState* s = new SessionState();
    if (root) {
        s->root = root;
    }
    void* handle = static_cast<void*>(s);
    g_sessions[handle] = *s;
    return handle;
}

void MultiFileSessionTracker_StageEdit(void* session, const char* path, const char* content) {
    if (!session || !path || path[0] == '\0') {
        return;
    }
    auto it = g_sessions.find(session);
    if (it == g_sessions.end()) {
        return;
    }
    it->second.stagedEdits[path] = content ? content : "";
}
}
