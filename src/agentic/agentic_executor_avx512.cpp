#include "agentic_executor_avx512.h"

#include <algorithm>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#endif

namespace rawrxd::agentic::avx512 {
namespace {
using KernelFn = void(*)(void*);

#if defined(RAWRXD_AGENTIC_KERNEL_LINKED)
extern "C" void agent_context_load(void*);
extern "C" void agent_step_execute(void*);
extern "C" void agent_context_store(void*);
#endif

KernelFn resolveKernelFn(const char* name) {
#ifdef _WIN32
    const HMODULE mod = ::GetModuleHandleA(nullptr);
    if (!mod) {
        return nullptr;
    }
    return reinterpret_cast<KernelFn>(::GetProcAddress(mod, name));
#else
    (void)name;
    return nullptr;
#endif
}

struct KernelFns {
    KernelFn load = nullptr;
    KernelFn step = nullptr;
    KernelFn store = nullptr;
};

KernelFns getKernelFns() {
#if defined(RAWRXD_AGENTIC_KERNEL_LINKED)
    static const KernelFns fns {
        &agent_context_load,
        &agent_step_execute,
        &agent_context_store
    };
    return fns;
#else
    static const KernelFns fns {
        resolveKernelFn("agent_context_load"),
        resolveKernelFn("agent_step_execute"),
        resolveKernelFn("agent_context_store")
    };
    return fns;
#endif
}

void fallbackLoad(AgentRegisterHotState& state) {
    state.reserved ^= 0x1u;
}

void fallbackStep(AgentRegisterHotState& state) {
    state.lastStepSuccess = 1;
    state.reserved ^= 0x2u;
}

void fallbackStore(AgentRegisterHotState& state) {
    state.reserved ^= 0x4u;
}
} // namespace

void initializeHotState(AgentRegisterHotState& state, const std::string& goal, const std::string& workspace) {
    std::memset(&state, 0, sizeof(state));

    const size_t gLen = std::min(goal.size(), sizeof(state.workingState));
    if (gLen > 0) {
        std::memcpy(state.workingState, goal.data(), gLen);
    }

    const size_t wLen = std::min(workspace.size(), sizeof(state.toolScratch));
    if (wLen > 0) {
        std::memcpy(state.toolScratch, workspace.data(), wLen);
    }
}

void loadContext(AgentRegisterHotState& state) {
    const KernelFns fns = getKernelFns();
    if (fns.load) {
        fns.load(&state);
        return;
    }
    fallbackLoad(state);
}

void executeStep(AgentRegisterHotState& state) {
    const KernelFns fns = getKernelFns();
    if (fns.step) {
        fns.step(&state);
        return;
    }
    fallbackStep(state);
}

void storeContext(AgentRegisterHotState& state) {
    const KernelFns fns = getKernelFns();
    if (fns.store) {
        fns.store(&state);
        return;
    }
    fallbackStore(state);
}

bool kernelExportsAvailable() {
    const KernelFns fns = getKernelFns();
    return fns.load && fns.step && fns.store;
}

} // namespace rawrxd::agentic::avx512
