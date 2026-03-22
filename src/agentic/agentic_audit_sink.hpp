#pragma once

#include <functional>
#include <mutex>
#include <string>

namespace Agentic {

/// Optional sink for machine-readable approval / execution audit (E07).
/// Win32 registers SQLite persistence; default falls back to Logger.
void setAgenticAuditSink(std::function<void(const std::string& eventKind, const std::string& jsonLine)> fn);
void clearAgenticAuditSink();
void agenticAuditEmit(const std::string& eventKind, const std::string& jsonLine);

}  // namespace Agentic
