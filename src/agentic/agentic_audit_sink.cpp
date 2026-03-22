#include "agentic_audit_sink.hpp"

#include <cstdio>

namespace Agentic
{

namespace
{

std::mutex g_auditMutex;
std::function<void(const std::string&, const std::string&)> g_auditSink;

}  // namespace

void setAgenticAuditSink(std::function<void(const std::string& eventKind, const std::string& jsonLine)> fn)
{
    std::lock_guard<std::mutex> lock(g_auditMutex);
    g_auditSink = std::move(fn);
}

void clearAgenticAuditSink()
{
    std::lock_guard<std::mutex> lock(g_auditMutex);
    g_auditSink = nullptr;
}

void agenticAuditEmit(const std::string& eventKind, const std::string& jsonLine)
{
    std::function<void(const std::string&, const std::string&)> copy;
    {
        std::lock_guard<std::mutex> lock(g_auditMutex);
        copy = g_auditSink;
    }
    if (copy)
    {
        copy(eventKind, jsonLine);
        return;
    }
    std::fprintf(stderr, "[AgenticAudit] %s %s\n", eventKind.c_str(), jsonLine.c_str());
}

}  // namespace Agentic
