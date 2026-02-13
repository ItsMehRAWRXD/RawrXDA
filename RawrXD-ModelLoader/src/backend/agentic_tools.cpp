#define NOMINMAX
#include "backend/agentic_tools.h"
#include "tools/file_ops.h"
#include "tools/git_client.h"
#include "backend/ollama_client.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <sstream>
#include <filesystem>

namespace fs = std::filesystem;
namespace RawrXD { namespace Backend {

using json = nlohmann::json;

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

// toolToString / stringToTool -------------------------------------------------
std::string AgenticToolExecutor::toolToString(AgenticTool tool) const {
	switch (tool) {
		case AgenticTool::FILE_READ: return "file_read";
		case AgenticTool::FILE_WRITE: return "file_write";
		case AgenticTool::FILE_APPEND: return "file_append";
		case AgenticTool::FILE_DELETE: return "file_delete";
		case AgenticTool::FILE_RENAME: return "file_rename";
		case AgenticTool::FILE_COPY: return "file_copy";
		case AgenticTool::FILE_MOVE: return "file_move";
		case AgenticTool::FILE_LIST: return "file_list";
		case AgenticTool::FILE_EXISTS: return "file_exists";
		case AgenticTool::DIR_CREATE: return "dir_create";
		case AgenticTool::GIT_STATUS: return "git_status";
		case AgenticTool::GIT_ADD: return "git_add";
		case AgenticTool::GIT_COMMIT: return "git_commit";
		case AgenticTool::GIT_PUSH: return "git_push";
		case AgenticTool::GIT_PULL: return "git_pull";
		case AgenticTool::GIT_BRANCH: return "git_branch";
		case AgenticTool::GIT_CHECKOUT: return "git_checkout";
		case AgenticTool::GIT_DIFF: return "git_diff";
		case AgenticTool::GIT_STASH_SAVE: return "git_stash_save";
		case AgenticTool::GIT_STASH_POP: return "git_stash_pop";
		case AgenticTool::GIT_FETCH: return "git_fetch";
		default: return "unknown";
	}
}

AgenticTool AgenticToolExecutor::stringToTool(const std::string& name) const {
	std::string n = name; std::transform(n.begin(), n.end(), n.begin(), ::tolower);
	if (n == "file_read") return AgenticTool::FILE_READ;
	if (n == "file_write") return AgenticTool::FILE_WRITE;
	if (n == "file_append") return AgenticTool::FILE_APPEND;
	if (n == "file_delete") return AgenticTool::FILE_DELETE;
	if (n == "file_rename") return AgenticTool::FILE_RENAME;
	if (n == "file_copy") return AgenticTool::FILE_COPY;
	if (n == "file_move") return AgenticTool::FILE_MOVE;
	if (n == "file_list") return AgenticTool::FILE_LIST;
	if (n == "file_exists") return AgenticTool::FILE_EXISTS;
	if (n == "dir_create") return AgenticTool::DIR_CREATE;
	if (n == "git_status") return AgenticTool::GIT_STATUS;
	if (n == "git_add") return AgenticTool::GIT_ADD;
	if (n == "git_commit") return AgenticTool::GIT_COMMIT;
	if (n == "git_push") return AgenticTool::GIT_PUSH;
	if (n == "git_pull") return AgenticTool::GIT_PULL;
	if (n == "git_branch") return AgenticTool::GIT_BRANCH;
	if (n == "git_checkout") return AgenticTool::GIT_CHECKOUT;
	if (n == "git_diff") return AgenticTool::GIT_DIFF;
	if (n == "git_stash_save") return AgenticTool::GIT_STASH_SAVE;
	if (n == "git_stash_pop") return AgenticTool::GIT_STASH_POP;
	if (n == "git_fetch") return AgenticTool::GIT_FETCH;
	return AgenticTool::UNKNOWN;
}

// Tool schemas -----------------------------------------------------------------
std::vector<ToolSchema> AgenticToolExecutor::getToolSchemas() const {
	std::vector<ToolSchema> v;
	auto add = [&](const std::string& name, const std::string& desc, std::map<std::string,std::string> params, std::vector<std::string> req){
		v.push_back(ToolSchema{ name, desc, std::move(params), std::move(req) });
	};
	add("file_read", "Read text content of a file", {{"path","Relative or absolute file path"}}, {"path"});
	add("file_write", "Write (overwrite) text content to a file", {{"path","File path"},{"content","Text to write"}}, {"path","content"});
	add("file_append", "Append text to end of a file", {{"path","File path"},{"content","Text to append"}}, {"path","content"});
	add("file_delete", "Delete a file", {{"path","File path to remove"}}, {"path"});
	add("file_rename", "Rename a file", {{"from","Existing path"},{"to","New path"}}, {"from","to"});
	add("file_copy", "Copy file", {{"from","Source"},{"to","Destination"},{"overwrite","true/false"}}, {"from","to"});
	add("file_move", "Move (rename across dirs) a file", {{"from","Source"},{"to","Destination"},{"overwrite","true/false"}}, {"from","to"});
	add("file_list", "List directory contents", {{"path","Directory path"},{"recursive","true/false"}}, {"path"});
	add("file_exists", "Check if path exists", {{"path","Path to check"}}, {"path"});
	add("dir_create", "Ensure directory exists (create if missing)", {{"path","Directory path"}}, {"path"});
	add("git_status", "Get git status", {{"short","true for short format"}}, {});
	add("git_add", "Stage files for commit", {{"paths","Comma separated list of paths"}}, {"paths"});
	add("git_commit", "Commit staged changes", {{"message","Commit message"},{"sign_off","true/false"}}, {"message"});
	add("git_push", "Push current branch", {{"remote","Remote name"},{"branch","Branch name (optional)"}}, {});
	add("git_pull", "Pull current branch", {{"remote","Remote name"},{"branch","Branch name (optional)"}}, {});
	add("git_branch", "Create new branch", {{"name","Branch name"}}, {"name"});
	add("git_checkout", "Checkout branch or commit", {{"target","Branch or commit sha"}}, {"target"});
	add("git_diff", "Show diff", {{"spec","Diff spec (optional)"}}, {});
	add("git_stash_save", "Stash changes", {{"message","Optional stash message"}}, {});
	add("git_stash_pop", "Apply and drop latest stash", {}, {});
	add("git_fetch", "Fetch from remote", {{"remote","Remote name"}}, {});
	return v;
}

std::string AgenticToolExecutor::getAvailableTools() const {
	json arr = json::array();
	for (auto& s : getToolSchemas()) {
		json o;
		o["name"] = s.name;
		o["description"] = s.description;
		json params = json::object();
		for (auto& kv : s.parameters) params[kv.first] = kv.second;
		o["parameters"] = params;
		o["required"] = s.required_params;
		arr.push_back(o);
	}
	return arr.dump();
}

// JSON helpers -----------------------------------------------------------------
std::string AgenticToolExecutor::paramsToJson(const std::map<std::string, std::string>& params) const {
	json o; for (auto& kv : params) o[kv.first] = kv.second; return o.dump();
}

bool AgenticToolExecutor::parseJson(const std::string& json_str, json& out, std::string& error) const {
	try { out = json::parse(json_str.empty() ? "{}" : json_str); return true; } catch (const std::exception& e) { error = e.what(); return false; }
}

// Dispatch ---------------------------------------------------------------------
ToolResult AgenticToolExecutor::executeTool(const std::string& tool_name, const std::string& params_json) {
	return executeTool(stringToTool(tool_name), params_json);
}

ToolResult AgenticToolExecutor::executeTool(AgenticTool tool, const std::string& params_json) {
	std::string err; json params;
	if (!parseJson(params_json, params, err)) {
		return ToolResult::Fail(toolToString(tool), "Invalid JSON parameters: " + err);
	}
	ToolResult result = ToolResult::Fail(toolToString(tool), "Unimplemented tool");
	switch (tool) {
		case AgenticTool::FILE_READ: result = executeFileRead(params); break;
		case AgenticTool::FILE_WRITE: result = executeFileWrite(params); break;
		case AgenticTool::FILE_APPEND: result = executeFileAppend(params); break;
		case AgenticTool::FILE_DELETE: result = executeFileDelete(params); break;
		case AgenticTool::FILE_RENAME: result = executeFileRename(params); break;
		case AgenticTool::FILE_COPY: result = executeFileCopy(params); break;
		case AgenticTool::FILE_MOVE: result = executeFileMove(params); break;
		case AgenticTool::FILE_LIST: result = executeFileList(params); break;
		case AgenticTool::FILE_EXISTS: result = executeFileExists(params); break;
		case AgenticTool::DIR_CREATE: result = executeDirCreate(params); break;
		case AgenticTool::GIT_STATUS: result = executeGitStatus(params); break;
		case AgenticTool::GIT_ADD: result = executeGitAdd(params); break;
		case AgenticTool::GIT_COMMIT: result = executeGitCommit(params); break;
		case AgenticTool::GIT_PUSH: result = executeGitPush(params); break;
		case AgenticTool::GIT_PULL: result = executeGitPull(params); break;
		case AgenticTool::GIT_BRANCH: result = executeGitBranch(params); break;
		case AgenticTool::GIT_CHECKOUT: result = executeGitCheckout(params); break;
		case AgenticTool::GIT_DIFF: result = executeGitDiff(params); break;
		case AgenticTool::GIT_STASH_SAVE: result = executeGitStashSave(params); break;
		case AgenticTool::GIT_STASH_POP: result = executeGitStashPop(params); break;
		case AgenticTool::GIT_FETCH: result = executeGitFetch(params); break;
		default: return ToolResult::Fail("unknown", "Unknown agentic tool name");
	}
	m_stats.total_tool_calls++;
	if (result.success) {
		m_stats.successful_calls++;
	} else {
		m_stats.failed_calls++;
	}
	m_stats.tool_usage_count[toolToString(tool)]++;
	return result;
}

// File operation implementations -----------------------------------------------
ToolResult AgenticToolExecutor::executeFileRead(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_read", "Missing required parameter: path");
    std::string path = params["path"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_read", "Path outside workspace not allowed");
    
    std::string content;
    auto result = RawrXD::Tools::FileOps::readText(fullPath, content);
    if (!result.success) return ToolResult::Fail("file_read", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["content"] = content;
    resultJson["size"] = content.length();
    return ToolResult::Ok("file_read", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileWrite(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_write", "Missing required parameter: path");
    if (!params.contains("content")) return ToolResult::Fail("file_write", "Missing required parameter: content");
    std::string path = params["path"];
    std::string content = params["content"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_write", "Path outside workspace not allowed");
    
    auto result = RawrXD::Tools::FileOps::writeText(fullPath, content, true);
    if (!result.success) return ToolResult::Fail("file_write", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["bytes_written"] = content.length();
    return ToolResult::Ok("file_write", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileAppend(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_append", "Missing required parameter: path");
    if (!params.contains("content")) return ToolResult::Fail("file_append", "Missing required parameter: content");
    std::string path = params["path"];
    std::string content = params["content"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_append", "Path outside workspace not allowed");
    
    auto result = RawrXD::Tools::FileOps::appendText(fullPath, content);
    if (!result.success) return ToolResult::Fail("file_append", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["bytes_appended"] = content.length();
    return ToolResult::Ok("file_append", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileDelete(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_delete", "Missing required parameter: path");
    std::string path = params["path"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_delete", "Path outside workspace not allowed");
    
    auto result = RawrXD::Tools::FileOps::remove(fullPath);
    if (!result.success) return ToolResult::Fail("file_delete", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["deleted"] = true;
    return ToolResult::Ok("file_delete", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileRename(const json& params) {
    if (!params.contains("from")) return ToolResult::Fail("file_rename", "Missing required parameter: from");
    if (!params.contains("to")) return ToolResult::Fail("file_rename", "Missing required parameter: to");
    std::string fromPath = params["from"];
    std::string toPath = params["to"];
    std::string fullFrom = normalizePath(fromPath);
    std::string fullTo = normalizePath(toPath);
    if (!isPathSafe(fullFrom) || !isPathSafe(fullTo)) return ToolResult::Fail("file_rename", "Path outside workspace not allowed");
    
    auto result = RawrXD::Tools::FileOps::rename(fullFrom, fullTo, true);
    if (!result.success) return ToolResult::Fail("file_rename", result.message);
    
    json resultJson;
    resultJson["from"] = fullFrom;
    resultJson["to"] = fullTo;
    resultJson["renamed"] = true;
    return ToolResult::Ok("file_rename", resultJson.dump());
}
ToolResult AgenticToolExecutor::executeFileCopy(const json& params) {
    if (!params.contains("from")) return ToolResult::Fail("file_copy", "Missing required parameter: from");
    if (!params.contains("to")) return ToolResult::Fail("file_copy", "Missing required parameter: to");
    std::string fromPath = params["from"];
    std::string toPath = params["to"];
    std::string fullFrom = normalizePath(fromPath);
    std::string fullTo = normalizePath(toPath);
    if (!isPathSafe(fullFrom) || !isPathSafe(fullTo)) return ToolResult::Fail("file_copy", "Path outside workspace not allowed");
    
    RawrXD::Tools::CopyOptions opts;
    if (params.contains("overwrite") && params["overwrite"].is_boolean()) {
        opts.overwrite = params["overwrite"];
    }
    
    auto result = RawrXD::Tools::FileOps::copy(fullFrom, fullTo, opts);
    if (!result.success) return ToolResult::Fail("file_copy", result.message);
    
    json resultJson;
    resultJson["from"] = fullFrom;
    resultJson["to"] = fullTo;
    resultJson["copied"] = true;
    return ToolResult::Ok("file_copy", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileMove(const json& params) {
    if (!params.contains("from")) return ToolResult::Fail("file_move", "Missing required parameter: from");
    if (!params.contains("to")) return ToolResult::Fail("file_move", "Missing required parameter: to");
    std::string fromPath = params["from"];
    std::string toPath = params["to"];
    std::string fullFrom = normalizePath(fromPath);
    std::string fullTo = normalizePath(toPath);
    if (!isPathSafe(fullFrom) || !isPathSafe(fullTo)) return ToolResult::Fail("file_move", "Path outside workspace not allowed");
    
    bool overwrite = false;
    if (params.contains("overwrite") && params["overwrite"].is_boolean()) {
        overwrite = params["overwrite"];
    }
    
    auto result = RawrXD::Tools::FileOps::move(fullFrom, fullTo, overwrite);
    if (!result.success) return ToolResult::Fail("file_move", result.message);
    
    json resultJson;
    resultJson["from"] = fullFrom;
    resultJson["to"] = fullTo;
    resultJson["moved"] = true;
    return ToolResult::Ok("file_move", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileList(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_list", "Missing required parameter: path");
    std::string path = params["path"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_list", "Path outside workspace not allowed");
    
    bool recursive = false;
    if (params.contains("recursive") && params["recursive"].is_boolean()) {
        recursive = params["recursive"];
    }
    
    std::vector<std::string> files;
    auto result = RawrXD::Tools::FileOps::list(fullPath, files, recursive);
    if (!result.success) return ToolResult::Fail("file_list", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["recursive"] = recursive;
    resultJson["files"] = files;
    resultJson["count"] = files.size();
    return ToolResult::Ok("file_list", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeFileExists(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("file_exists", "Missing required parameter: path");
    std::string path = params["path"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("file_exists", "Path outside workspace not allowed");
    
    bool exists = RawrXD::Tools::FileOps::exists(fullPath);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["exists"] = exists;
    return ToolResult::Ok("file_exists", resultJson.dump());
}

ToolResult AgenticToolExecutor::executeDirCreate(const json& params) {
    if (!params.contains("path")) return ToolResult::Fail("dir_create", "Missing required parameter: path");
    std::string path = params["path"];
    std::string fullPath = normalizePath(path);
    if (!isPathSafe(fullPath)) return ToolResult::Fail("dir_create", "Path outside workspace not allowed");
    
    auto result = RawrXD::Tools::FileOps::ensureDir(fullPath);
    if (!result.success) return ToolResult::Fail("dir_create", result.message);
    
    json resultJson;
    resultJson["path"] = fullPath;
    resultJson["created"] = true;
    return ToolResult::Ok("dir_create", resultJson.dump());
}
// Git operation implementations -----------------------------------------------
ToolResult AgenticToolExecutor::executeGitStatus(const json& params) {
    bool short_format = false;
    if (params.contains("short") && params["short"].is_boolean()) {
        short_format = params["short"];
    }
    
    auto result = m_git_client->status(short_format);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["short_format"] = short_format;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_status", resultJson.dump());
    } else {
        return ToolResult::Fail("git_status", "Git status failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitAdd(const json& params) {
    if (!params.contains("paths")) return ToolResult::Fail("git_add", "Missing required parameter: paths");
    std::string pathsStr = params["paths"];
    
    // Parse comma-separated paths
    std::vector<std::string> paths;
    std::stringstream ss(pathsStr);
    std::string path;
    while (std::getline(ss, path, ',')) {
        // Trim whitespace
        path.erase(0, path.find_first_not_of(" \t"));
        path.erase(path.find_last_not_of(" \t") + 1);
        if (!path.empty()) paths.push_back(path);
    }
    
    auto result = m_git_client->add(paths);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["paths"] = paths;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_add", resultJson.dump());
    } else {
        return ToolResult::Fail("git_add", "Git add failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitCommit(const json& params) {
    if (!params.contains("message")) return ToolResult::Fail("git_commit", "Missing required parameter: message");
    std::string message = params["message"];
    
    bool sign_off = false;
    if (params.contains("sign_off") && params["sign_off"].is_boolean()) {
        sign_off = params["sign_off"];
    }
    
    auto result = m_git_client->commit(message, sign_off);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["message"] = message;
    resultJson["sign_off"] = sign_off;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_commit", resultJson.dump());
    } else {
        return ToolResult::Fail("git_commit", "Git commit failed: " + result.stderr_text, result.exit_code);
    }
}
ToolResult AgenticToolExecutor::executeGitPush(const json& params) {
    std::string remote = "origin";
    std::string branch = "";
    
    if (params.contains("remote") && params["remote"].is_string()) {
        remote = params["remote"];
    }
    if (params.contains("branch") && params["branch"].is_string()) {
        branch = params["branch"];
    }
    
    auto result = m_git_client->push(remote, branch);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["remote"] = remote;
    resultJson["branch"] = branch;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_push", resultJson.dump());
    } else {
        return ToolResult::Fail("git_push", "Git push failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitPull(const json& params) {
    std::string remote = "origin";
    std::string branch = "";
    
    if (params.contains("remote") && params["remote"].is_string()) {
        remote = params["remote"];
    }
    if (params.contains("branch") && params["branch"].is_string()) {
        branch = params["branch"];
    }
    
    auto result = m_git_client->pull(remote, branch);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["remote"] = remote;
    resultJson["branch"] = branch;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_pull", resultJson.dump());
    } else {
        return ToolResult::Fail("git_pull", "Git pull failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitBranch(const json& params) {
    if (!params.contains("name")) return ToolResult::Fail("git_branch", "Missing required parameter: name");
    std::string name = params["name"];
    
    auto result = m_git_client->createBranch(name);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["branch_name"] = name;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_branch", resultJson.dump());
    } else {
        return ToolResult::Fail("git_branch", "Git branch creation failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitCheckout(const json& params) {
    if (!params.contains("target")) return ToolResult::Fail("git_checkout", "Missing required parameter: target");
    std::string target = params["target"];
    
    auto result = m_git_client->checkout(target);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["target"] = target;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_checkout", resultJson.dump());
    } else {
        return ToolResult::Fail("git_checkout", "Git checkout failed: " + result.stderr_text, result.exit_code);
    }
}
ToolResult AgenticToolExecutor::executeGitDiff(const json& params) {
    std::string spec = "";
    if (params.contains("spec") && params["spec"].is_string()) {
        spec = params["spec"];
    }
    
    auto result = m_git_client->diff(spec);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["spec"] = spec;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_diff", resultJson.dump());
    } else {
        return ToolResult::Fail("git_diff", "Git diff failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitStashSave(const json& params) {
    std::string message = "";
    if (params.contains("message") && params["message"].is_string()) {
        message = params["message"];
    }
    
    auto result = m_git_client->stashSave(message);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["message"] = message;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_stash_save", resultJson.dump());
    } else {
        return ToolResult::Fail("git_stash_save", "Git stash save failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitStashPop(const json& params) {
    auto result = m_git_client->stashPop();
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_stash_pop", resultJson.dump());
    } else {
        return ToolResult::Fail("git_stash_pop", "Git stash pop failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeGitFetch(const json& params) {
    std::string remote = "origin";
    if (params.contains("remote") && params["remote"].is_string()) {
        remote = params["remote"];
    }
    
    auto result = m_git_client->fetch(remote);
    
    json resultJson;
    resultJson["exit_code"] = result.exit_code;
    resultJson["stdout"] = result.stdout_text;
    resultJson["stderr"] = result.stderr_text;
    resultJson["remote"] = remote;
    
    if (result.exit_code == 0) {
        return ToolResult::Ok("git_fetch", resultJson.dump());
    } else {
        return ToolResult::Fail("git_fetch", "Git fetch failed: " + result.stderr_text, result.exit_code);
    }
}

ToolResult AgenticToolExecutor::executeToolFromAI(const std::string& ai_tool_request) {
	std::string toolName, toolParams;
	if (!extractToolCall(ai_tool_request, toolName, toolParams)) {
		return ToolResult::Fail("parse_error", "Could not parse tool call from AI request: " + ai_tool_request);
	}
	return executeTool(toolName, toolParams);
}

std::string AgenticToolExecutor::generateToolPrompt(const std::vector<ToolSchema>& tools) const {
	std::ostringstream oss;
	oss << "You can call tools using the format: TOOL:<name>:<json parameters>\n";
	oss << "Available tools (name -> description & required params):\n";
	for (auto& t : tools) {
		oss << " - " << t.name << ": " << t.description;
		if (!t.required_params.empty()) {
			oss << " (required: ";
			for (size_t i=0;i<t.required_params.size();++i) {
				if (i) oss << ",";
				oss << t.required_params[i];
			}
			oss << ")";
		}
		oss << "\n";
	}
	return oss.str();
}

bool AgenticToolExecutor::extractToolCall(const std::string& ai_response, std::string& tool_name, std::string& params_json) const {
	// Simple heuristic: look for line starting with TOOL:
	// Format: TOOL:name:{...}
	auto pos = ai_response.find("TOOL:");
	if (pos == std::string::npos) return false;
	auto after = ai_response.substr(pos + 5);
	auto colon = after.find(':');
	if (colon == std::string::npos) return false;
	tool_name = after.substr(0, colon);
	auto brace = after.find('{', colon);
	if (brace == std::string::npos) return false;
	auto endBrace = after.rfind('}');
	if (endBrace == std::string::npos || endBrace < brace) return false;
	params_json = after.substr(brace, endBrace - brace + 1);
	return true;
}

// AI Integration Implementation ------------------------------------------------
std::string AgenticToolExecutor::chatWithTools(const std::string& user_message, std::vector<OllamaChatMessage>& conversation_history, const ChatConfig& config) {
    // Add user message to history
    OllamaChatMessage userMsg;
    userMsg.role = "user";
    userMsg.content = user_message;
    conversation_history.push_back(userMsg);
    
    // Generate system prompt with tool info
    auto toolPrompt = generateToolPrompt(getToolSchemas());
    
    // Main agentic loop
    int iterations = 0;
    std::string finalAnswer;
    
    while (iterations < config.max_tool_iterations) {
        // Create chat request with current history
        OllamaChatRequest request;
        request.model = config.model;
        request.stream = false;
        request.options["temperature"] = config.temperature;
        
        // Add system message with tool instructions
        if (iterations == 0) {
            OllamaChatMessage sysMsg;
            sysMsg.role = "system";
            sysMsg.content = toolPrompt + "\nYou can call tools to help answer the user's question. When you need to use a tool, respond with TOOL:<name>:<json>. Otherwise, provide your final answer.";
            request.messages.push_back(sysMsg);
        }
        
        // Add conversation history
        request.messages.insert(request.messages.end(), conversation_history.begin(), conversation_history.end());
        
        // Get AI response
        auto response = m_ollama_client->chatSync(request);
        
        if (!response.error && !response.response.empty()) {
            // Check if AI wants to use a tool
            std::string toolName, toolParams;
            if (extractToolCall(response.response, toolName, toolParams)) {
                // AI wants to call a tool
                if (config.on_message) {
                    config.on_message("AI calling tool: " + toolName);
                }
                
                auto toolResult = executeTool(toolName, toolParams);
                
                if (config.on_tool_call) {
                    config.on_tool_call(toolResult);
                }
                
                // Add AI's tool request to history
                OllamaChatMessage aiToolMsg;
                aiToolMsg.role = "assistant";
                aiToolMsg.content = response.response;
                conversation_history.push_back(aiToolMsg);
                
                // Add tool result to history
                OllamaChatMessage toolResultMsg;
                toolResultMsg.role = "user";
                toolResultMsg.content = "Tool result: " + (toolResult.success ? toolResult.result_data : "Error: " + toolResult.error_message);
                conversation_history.push_back(toolResultMsg);
                
                iterations++;
                continue;
            } else {
                // AI provided final answer
                finalAnswer = response.response;
                
                // Add AI's final response to history
                OllamaChatMessage aiMsg;
                aiMsg.role = "assistant";
                aiMsg.content = response.response;
                conversation_history.push_back(aiMsg);
                break;
            }
        } else {
            finalAnswer = "Error communicating with AI: " + response.error_message;
            break;
        }
        
        iterations++;
    }
    
    if (iterations >= config.max_tool_iterations) {
        finalAnswer = "Maximum tool iterations reached. Last AI response: " + finalAnswer;
    }
    
    return finalAnswer;
}

} } // namespace RawrXD::Backend

