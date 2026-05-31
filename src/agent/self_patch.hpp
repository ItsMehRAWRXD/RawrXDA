#pragma once
// self_patch.hpp – Qt-free SelfPatch (C++20 / Win32)
// Pattern: Function pointer callbacks, no exceptions in patch path
#include <cstdint>
#include <string>

class SelfPatch {
public:
    SelfPatch();
    ~SelfPatch() = default;

    // Add Vulkan kernel from template
    bool addKernel(const std::string& name, const std::string& templateName);

    // Add C++ wrapper for kernel
    bool addCpp(const std::string& name, const std::string& deps);

    // Hot-reload the binary (rebuild + restart)
    bool hotReload();

    // Patch existing file
    bool patchFile(const std::string& filename, const std::string& patch);

    // --- Callback registration (replaces Qt signals) ---
    using StringCallback    = void(*)(void* ctx, const char* name);
    using ReloadCallback    = void(*)(void* ctx, bool success);

    void setKernelAddedCb(StringCallback cb, void* ctx)  { m_onKernelAdded = cb; m_kernelCtx = ctx; }
    void setCppAddedCb(StringCallback cb, void* ctx)      { m_onCppAdded = cb; m_cppCtx = ctx; }
    void setReloadStartedCb(ReloadCallback cb, void* ctx) { m_onReloadStarted = cb; m_reloadCtx = ctx; }
    void setReloadDoneCb(ReloadCallback cb, void* ctx)    { m_onReloadDone = cb; m_reloadDoneCtx = ctx; }

private:
    void fireKernelAdded(const std::string& n)  { if (m_onKernelAdded)  m_onKernelAdded(m_kernelCtx, n.c_str()); }
    void fireCppAdded(const std::string& n)      { if (m_onCppAdded)    m_onCppAdded(m_cppCtx, n.c_str()); }
    void fireReloadStarted()                      { if (m_onReloadStarted) m_onReloadStarted(m_reloadCtx, false); }
    void fireReloadDone(bool ok)                  { if (m_onReloadDone)  m_onReloadDone(m_reloadDoneCtx, ok); }

    StringCallback m_onKernelAdded  = nullptr;  void* m_kernelCtx     = nullptr;
    StringCallback m_onCppAdded     = nullptr;  void* m_cppCtx        = nullptr;
    ReloadCallback m_onReloadStarted = nullptr; void* m_reloadCtx     = nullptr;
    ReloadCallback m_onReloadDone   = nullptr;  void* m_reloadDoneCtx = nullptr;
};
