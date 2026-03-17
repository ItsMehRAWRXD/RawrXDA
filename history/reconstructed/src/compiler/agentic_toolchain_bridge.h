#pragma once
#include "toolchain_bridge.hpp"
#include "agentic/agentic_executor.h"

namespace RawrXD {
class AgenticToolchainBridge {
    ToolchainBridge* m_toolchain;
    AgenticExecutor* m_executor;
public:
    AgenticToolchainBridge(ToolchainBridge* t, AgenticExecutor* e) : m_toolchain(t), m_executor(e) {
        m_executor->SetCompileCallback([this](const std::wstring& projPath) -> bool {
            ToolchainBridge::CompileRequest req;
            req.projectPath = projPath;
            req.config = ToolchainBridge::Config::Release;
            bool ok = false;
            m_toolchain->CompileAsync(req, [&ok](const ToolchainBridge::Result& r){ ok = r.success; });
            return ok;
        });
    }
};
} // namespace RawrXD
