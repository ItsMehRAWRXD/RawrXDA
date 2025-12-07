#define NOMINMAX
#include "backend/agentic_tools.h"
#include "tools/file_ops.h"
#include "tools/git_client.h"
#include "backend/ollama_client.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <sstream>
#include <algorithm>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace RawrXD {
namespace Backend {

// ToolResult Implementation
ToolResult ToolResult::Ok(const std::string& tool, const std::string& data) {
    ToolResult r;
    r.success = true;
    r.tool_name = tool;
    r.result_data = data;
    r.exit_code = 0;
    return r;
}

ToolResult ToolResult::Fail(const std::string& tool, const std::string& error, int code) {
    ToolResult r;
    r.success = false;
    r.tool_name = tool;
    r.error_message = error;
    r.exit_code = code;
    return r;
}

// AgenticToolExecutor Implementation
AgenticToolExecutor::AgenticToolExecutor(const std::string& workspace_root) 
    : m_workspace_root(workspace_root)
    , m_allow_outside_workspace(false)
{
    m_git_client = std::make_unique<Tools::GitClient>(workspace_root);
    m_ollama_client = std::make_unique<OllamaClient>("http://localhost:11434");
}

AgenticToolExecutor::~AgenticToolExecutor() = default;

void AgenticToolExecutor::setWorkspaceRoot(const std::string& root) {
    m_workspace_root = root;
    m_git_client = std::make_unique<Tools::GitClient>(root);
}

bool AgenticToolExecutor::isPathSafe(const std::string& path) const {
    if (m_allow_outside_workspace) return true;
    try {
        fs::path requested = fs::absolute(path);
        fs::path workspace = fs::absolute(m_workspace_root);
        auto rel = fs::relative(requested, workspace);
        return !rel.empty() && rel.native()[0] != '.';
    } catch (...) {
        return false;
    }
}

std::string AgenticToolExecutor::normalizePath(const std::string& path) const {
    try {
        if (fs::path(path).is_absolute()) return path;
        return (fs::path(m_workspace_root) / path).string();
    } catch (...) {
        return path;
    }
}
