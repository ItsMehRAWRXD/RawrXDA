// ============================================================================
// AGENTIC ENGINE - Bounded Autonomous Agent Loop + Tool Execution
// ============================================================================
// Replaces the old single-shot RunAutonomousMode() with a real agentic loop:
//   1. Build system prompt with tool schemas
//   2. Send user task + conversation history to LLM via Ollama /api/chat
//   3. If LLM returns tool_calls -> execute tool, append result, goto 2
//   4. If LLM returns text -> final answer, stop
//   5. If step >= MAX_AGENT_STEPS -> forced stop
//
// Tools: read_file, write_file, list_dir, execute_command, search_files,
//        replace_in_file, delete_file
// ============================================================================

static const int MAX_AGENT_STEPS = 15;

// -- Minimal JSON helpers (no external deps) --

static std::string JsonEscape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (char c : s) {
        switch (c) {
            case '\"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (unsigned)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

static std::string JsonExtractString(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == ':' || json[pos] == '\t' || json[pos] == '\n' || json[pos] == '\r')) pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        pos++;
        std::string result;
        while (pos < json.size()) {
            if (json[pos] == '\\' && pos + 1 < json.size()) {
                char next = json[pos + 1];
                switch (next) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    default: result += next; break;
                }
                pos += 2;
            } else if (json[pos] == '"') {
                break;
            } else {
                result += json[pos];
                pos++;
            }
        }
        return result;
    }
    size_t start = pos;
    while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']'
           && json[pos] != '\n' && json[pos] != '\r') pos++;
    std::string val = json.substr(start, pos - start);
    while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
    return val;
}

static std::string JsonExtractObject(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\"";
    size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    while (pos < json.size() && json[pos] != '{' && json[pos] != '[') {
        if (json[pos] == '"') return "";
        pos++;
    }
    if (pos >= json.size()) return "";
    char open = json[pos];
    char close = (open == '{') ? '}' : ']';
    int depth = 1;
    size_t start = pos;
    pos++;
    bool inStr = false;
    while (pos < json.size() && depth > 0) {
        if (json[pos] == '\\' && inStr) { pos += 2; continue; }
        if (json[pos] == '"') inStr = !inStr;
        if (!inStr) {
            if (json[pos] == open) depth++;
            else if (json[pos] == close) depth--;
        }
        pos++;
    }
    return json.substr(start, pos - start);
}

static std::string JsonFirstArrayElement(const std::string& arr) {
    if (arr.empty() || arr[0] != '[') return "";
    size_t pos = 1;
    while (pos < arr.size() && (arr[pos] == ' ' || arr[pos] == '\n' || arr[pos] == '\r' || arr[pos] == '\t')) pos++;
    if (pos >= arr.size() || arr[pos] == ']') return "";
    if (arr[pos] == '{') {
        int depth = 1;
        size_t start = pos;
        pos++;
        bool inStr = false;
        while (pos < arr.size() && depth > 0) {
            if (arr[pos] == '\\' && inStr) { pos += 2; continue; }
            if (arr[pos] == '"') inStr = !inStr;
            if (!inStr) {
                if (arr[pos] == '{') depth++;
                else if (arr[pos] == '}') depth--;
            }
            pos++;
        }
        return arr.substr(start, pos - start);
    }
    return "";
}
// -- Agent Tool Implementations --

struct AgentToolResult {
    bool success;
    std::string output;
    std::string error;
};

static AgentToolResult AgentTool_ReadFile(const std::string& argsJson) {
    AgentToolResult r;
    std::string path = JsonExtractString(argsJson, "path");
    if (path.empty()) { r.success = false; r.error = "read_file: missing 'path'"; return r; }
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) { r.success = false; r.error = "Cannot open file: " + path; return r; }
    std::ostringstream ss;
    ss << f.rdbuf();
    f.close();
    std::string content = ss.str();
    if (content.size() > 65536) {
        content = content.substr(0, 65536) + "\n[TRUNCATED at 64KB]";
    }
    r.success = true;
    r.output = content;
    return r;
}

static AgentToolResult AgentTool_WriteFile(const std::string& argsJson) {
    AgentToolResult r;
    std::string path = JsonExtractString(argsJson, "path");
    std::string content = JsonExtractString(argsJson, "content");
    if (path.empty()) { r.success = false; r.error = "write_file: missing 'path'"; return r; }
    fs::path p(path);
    if (p.has_parent_path()) {
        try { fs::create_directories(p.parent_path()); } catch (...) {}
    }
    if (fs::exists(path)) {
        try { fs::copy_file(path, path + ".agent_bak", fs::copy_options::overwrite_existing); } catch (...) {}
    }
    std::ofstream f(path, std::ios::trunc | std::ios::binary);
    if (!f.is_open()) { r.success = false; r.error = "Cannot write to: " + path; return r; }
    f.write(content.data(), content.size());
    f.close();
    r.success = true;
    r.output = "File written: " + path + " (" + std::to_string(content.size()) + " bytes)";
    return r;
}

static AgentToolResult AgentTool_ReplaceInFile(const std::string& argsJson) {
    AgentToolResult r;
    std::string path = JsonExtractString(argsJson, "path");
    std::string oldStr = JsonExtractString(argsJson, "old_string");
    std::string newStr = JsonExtractString(argsJson, "new_string");
    if (path.empty() || oldStr.empty()) {
        r.success = false; r.error = "replace_in_file: missing 'path' or 'old_string'"; return r;
    }
    std::ifstream inf(path, std::ios::binary);
    if (!inf.is_open()) { r.success = false; r.error = "Cannot open: " + path; return r; }
    std::ostringstream ss;
    ss << inf.rdbuf();
    inf.close();
    std::string content = ss.str();
    size_t pos = content.find(oldStr);
    if (pos == std::string::npos) {
        r.success = false; r.error = "old_string not found in " + path; return r;
    }
    try { fs::copy_file(path, path + ".agent_bak", fs::copy_options::overwrite_existing); } catch (...) {}
    std::string newContent = content.substr(0, pos) + newStr + content.substr(pos + oldStr.size());
    std::ofstream outf(path, std::ios::trunc | std::ios::binary);
    if (!outf.is_open()) { r.success = false; r.error = "Cannot write: " + path; return r; }
    outf.write(newContent.data(), newContent.size());
    outf.close();
    r.success = true;
    r.output = "Replaced " + std::to_string(oldStr.size()) + " bytes with " +
               std::to_string(newStr.size()) + " bytes in " + path;
    return r;
}

static AgentToolResult AgentTool_ListDir(const std::string& argsJson) {
    AgentToolResult r;
    std::string path = JsonExtractString(argsJson, "path");
    if (path.empty()) { r.success = false; r.error = "list_dir: missing 'path'"; return r; }
    if (!fs::exists(path) || !fs::is_directory(path)) {
        r.success = false; r.error = "Not a directory: " + path; return r;
    }
    std::ostringstream listing;
    int count = 0;
    try {
        for (const auto& entry : fs::directory_iterator(path)) {
            std::string name = entry.path().filename().string();
            if (entry.is_directory()) listing << name << "/\n";
            else listing << name << " (" << entry.file_size() << " bytes)\n";
            count++;
            if (count > 500) { listing << "[...truncated at 500 entries]\n"; break; }
        }
    } catch (const std::exception& ex) {
        r.success = false; r.error = std::string("list_dir failed: ") + ex.what(); return r;
    }
    r.success = true;
    r.output = listing.str();
    if (r.output.empty()) r.output = "(empty directory)";
    return r;
}
static AgentToolResult AgentTool_ExecuteCommand(const std::string& argsJson) {
    AgentToolResult r;
    std::string command = JsonExtractString(argsJson, "command");
    if (command.empty()) { r.success = false; r.error = "execute_command: missing 'command'"; return r; }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    HANDLE hReadPipe = nullptr, hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        r.success = false; r.error = "Failed to create pipe"; return r;
    }
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi{};
    std::wstring cmdLine = L"cmd.exe /C " + Utf8ToWide(command);
    BOOL created = CreateProcessW(nullptr, cmdLine.data(), nullptr, nullptr, TRUE,
                                   CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    CloseHandle(hWritePipe);
    if (!created) {
        CloseHandle(hReadPipe);
        r.success = false; r.error = "Failed to execute: " + command; return r;
    }
    std::string output;
    output.reserve(4096);
    DWORD startTick = GetTickCount();
    while (true) {
        DWORD avail = 0;
        if (PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &avail, nullptr) && avail > 0) {
            char buf[4096];
            DWORD bytesRead = 0;
            if (::ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0) {
                buf[bytesRead] = '\0';
                output.append(buf, bytesRead);
                if (output.size() > 32768) { output += "\n[OUTPUT TRUNCATED at 32KB]"; break; }
            }
        }
        DWORD waitRes = WaitForSingleObject(pi.hProcess, 100);
        if (waitRes == WAIT_OBJECT_0) {
            while (true) {
                DWORD a2 = 0;
                if (!PeekNamedPipe(hReadPipe, nullptr, 0, nullptr, &a2, nullptr) || a2 == 0) break;
                char buf[4096];
                DWORD br = 0;
                if (::ReadFile(hReadPipe, buf, sizeof(buf) - 1, &br, nullptr) && br > 0) {
                    buf[br] = '\0';
                    output.append(buf, br);
                } else break;
            }
            break;
        }
        if (GetTickCount() - startTick > 30000) {
            TerminateProcess(pi.hProcess, 1);
            output += "\n[TIMEOUT after 30 seconds]";
            break;
        }
    }
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hReadPipe);
    r.success = true;
    r.output = output;
    if (exitCode != 0) r.error = "Exit code: " + std::to_string(exitCode);
    return r;
}

static AgentToolResult AgentTool_SearchFiles(const std::string& argsJson) {
    AgentToolResult r;
    std::string query = JsonExtractString(argsJson, "query");
    std::string dir = JsonExtractString(argsJson, "directory");
    std::string pattern = JsonExtractString(argsJson, "file_pattern");
    if (query.empty()) { r.success = false; r.error = "search_files: missing 'query'"; return r; }
    if (dir.empty()) dir = WideToUtf8(g_workspaceRoot);
    if (dir.empty()) { r.success = false; r.error = "No workspace root set"; return r; }
    if (pattern.empty()) pattern = "*.*";
    std::ostringstream results;
    int hits = 0;
    const int maxHits = 50;
    try {
        for (auto it = fs::recursive_directory_iterator(dir, fs::directory_options::skip_permission_denied);
             it != fs::recursive_directory_iterator(); ++it) {
            if (hits >= maxHits) break;
            if (!it->is_regular_file()) continue;
            if (it->file_size() > 2 * 1024 * 1024 || it->file_size() == 0) continue;
            if (pattern != "*.*" && pattern != "*") {
                std::string ext = it->path().extension().string();
                if (pattern.size() > 1 && pattern[0] == '*') {
                    if (ext != pattern.substr(1)) continue;
                }
            }
            std::ifstream f(it->path(), std::ios::binary);
            if (!f.is_open()) continue;
            std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            f.close();
            size_t pos = 0;
            while (pos < content.size() && hits < maxHits) {
                size_t found = content.find(query, pos);
                if (found == std::string::npos) break;
                int lineNum = 1;
                for (size_t i = 0; i < found; i++) { if (content[i] == '\n') lineNum++; }
                size_t lineStart = content.rfind('\n', found);
                lineStart = (lineStart == std::string::npos) ? 0 : lineStart + 1;
                size_t lineEnd = content.find('\n', found);
                if (lineEnd == std::string::npos) lineEnd = content.size();
                std::string line = content.substr(lineStart, std::min(lineEnd - lineStart, (size_t)200));
                std::string relPath = fs::relative(it->path(), dir).string();
                results << relPath << ":" << lineNum << ": " << line << "\n";
                hits++;
                pos = found + query.size();
            }
        }
    } catch (...) {}
    r.success = true;
    r.output = hits > 0 ? results.str() : "No matches found for: " + query;
    if (hits >= maxHits) r.output += "\n[Truncated at " + std::to_string(maxHits) + " results]";
    return r;
}

static AgentToolResult AgentTool_DeleteFile(const std::string& argsJson) {
    AgentToolResult r;
    std::string path = JsonExtractString(argsJson, "path");
    if (path.empty()) { r.success = false; r.error = "delete_file: missing 'path'"; return r; }
    if (!fs::exists(path)) { r.success = false; r.error = "File not found: " + path; return r; }
    try { fs::copy_file(path, path + ".agent_deleted_bak", fs::copy_options::overwrite_existing); } catch (...) {}
    try {
        fs::remove(path);
        r.success = true;
        r.output = "Deleted: " + path;
    } catch (const std::exception& ex) {
        r.success = false;
        r.error = std::string("delete_file failed: ") + ex.what();
    }
    return r;
}
// -- Tool dispatch --

static AgentToolResult DispatchAgentTool(const std::string& name, const std::string& argsJson) {
    if (name == "read_file")        return AgentTool_ReadFile(argsJson);
    if (name == "write_file")       return AgentTool_WriteFile(argsJson);
    if (name == "replace_in_file")  return AgentTool_ReplaceInFile(argsJson);
    if (name == "list_dir")         return AgentTool_ListDir(argsJson);
    if (name == "execute_command")  return AgentTool_ExecuteCommand(argsJson);
    if (name == "search_files")     return AgentTool_SearchFiles(argsJson);
    if (name == "search_code")      return AgentTool_SearchFiles(argsJson);
    if (name == "delete_file")      return AgentTool_DeleteFile(argsJson);
    AgentToolResult r;
    r.success = false;
    r.error = "Unknown tool: " + name;
    return r;
}

// -- Tool schema JSON (OpenAI function-calling format for Ollama) --

static std::string GetAgentToolSchemas() {
    return R"([
  {"type":"function","function":{"name":"read_file","description":"Read the content of a file.","parameters":{"type":"object","properties":{"path":{"type":"string","description":"Absolute path to the file"}},"required":["path"]}}},
  {"type":"function","function":{"name":"write_file","description":"Create or overwrite a file. Backup is created automatically.","parameters":{"type":"object","properties":{"path":{"type":"string","description":"Absolute path for the file"},"content":{"type":"string","description":"Complete file content to write"}},"required":["path","content"]}}},
  {"type":"function","function":{"name":"replace_in_file","description":"Search and replace a block of text in a file. Include 3+ lines of context in old_string.","parameters":{"type":"object","properties":{"path":{"type":"string","description":"Absolute path to the file"},"old_string":{"type":"string","description":"Exact text to find"},"new_string":{"type":"string","description":"Replacement text"}},"required":["path","old_string","new_string"]}}},
  {"type":"function","function":{"name":"list_dir","description":"List directory contents.","parameters":{"type":"object","properties":{"path":{"type":"string","description":"Absolute path to the directory"}},"required":["path"]}}},
  {"type":"function","function":{"name":"execute_command","description":"Run a command in cmd.exe. Use for builds, tests, git, etc.","parameters":{"type":"object","properties":{"command":{"type":"string","description":"Command to execute"}},"required":["command"]}}},
  {"type":"function","function":{"name":"search_files","description":"Search the codebase for a text pattern. Returns file:line: matches.","parameters":{"type":"object","properties":{"query":{"type":"string","description":"Text pattern to search for"},"directory":{"type":"string","description":"Directory to search in (default: workspace root)"},"file_pattern":{"type":"string","description":"File extension filter e.g. *.cpp (default: *.*)"}},"required":["query"]}}},
  {"type":"function","function":{"name":"delete_file","description":"Delete a file (backup is created first).","parameters":{"type":"object","properties":{"path":{"type":"string","description":"Absolute path to the file to delete"}},"required":["path"]}}}
])";
}

// -- System prompt builder --

static std::string BuildAgentSystemPrompt() {
    std::string cwd = WideToUtf8(g_workspaceRoot);
    if (cwd.empty()) cwd = ".";
    std::string openFiles;
    for (const auto& tab : g_tabs) {
        openFiles += "- " + WideToUtf8(tab.filePath) + "\n";
    }
    if (openFiles.empty()) openFiles = "  (none)\n";
    std::string prompt =
        "You are RawrXD Agent, a high-performance autonomous coding assistant with direct filesystem and terminal access.\n\n"
        "Current Directory: " + cwd + "\n"
        "Open Files:\n" + openFiles + "\n"
        "Rules:\n"
        "1. Always read_file before editing to verify current content.\n"
        "2. Use replace_in_file for surgical edits; write_file for new files only.\n"
        "3. Include 3+ lines of context in old_string for uniqueness.\n"
        "4. Explain your reasoning before executing each tool.\n"
        "5. Use search_files to find relevant code before making assumptions.\n"
        "6. Do not modify files outside the workspace unless asked.\n"
        "7. When your task is complete, provide a final summary of what you did.\n";
    return prompt;
}
// -- Ollama /api/chat with tool support --

struct OllamaChatMessage {
    std::string role;
    std::string content;
    std::string toolCallsJson;
    std::string toolCallId;
};

struct OllamaChatResponse {
    bool success = false;
    std::string content;
    bool hasToolCall = false;
    std::string toolName;
    std::string toolArgsJson;
    std::string toolCallId;
    std::string error;
    std::string rawResponse;
};

static std::string BuildOllamaChatBody(const std::string& model,
                                        const std::vector<OllamaChatMessage>& messages,
                                        const std::string& toolSchemas) {
    std::string body = "{\"model\":\"" + JsonEscape(model) + "\",\"messages\":[";
    for (size_t i = 0; i < messages.size(); i++) {
        if (i > 0) body += ",";
        const auto& m = messages[i];
        body += "{\"role\":\"" + m.role + "\"";
        if (!m.content.empty()) {
            body += ",\"content\":\"" + JsonEscape(m.content) + "\"";
        } else {
            body += ",\"content\":\"\"";
        }
        if (m.role == "assistant" && !m.toolCallsJson.empty()) {
            body += ",\"tool_calls\":" + m.toolCallsJson;
        }
        body += "}";
    }
    body += "],\"stream\":false,\"options\":{\"temperature\":0.1,\"num_predict\":4096}";
    if (!toolSchemas.empty()) {
        body += ",\"tools\":" + toolSchemas;
    }
    body += "}";
    return body;
}

static OllamaChatResponse OllamaAgentChat(const std::string& model,
                                            const std::vector<OllamaChatMessage>& messages,
                                            const std::string& toolSchemas) {
    OllamaChatResponse resp;
    const auto& cfg = g_backendConfigs[(size_t)AIBackendType::Ollama];
    std::string endpoint = cfg.endpoint;
    if (endpoint.empty()) endpoint = "http://localhost:11434";
    std::string body = BuildOllamaChatBody(model, messages, toolSchemas);
    std::string rawResp = HttpPost(endpoint + "/api/chat", body, {"Content-Type: application/json"}, 300000);
    resp.rawResponse = rawResp;
    if (rawResp.empty()) {
        resp.error = "Ollama not responding at " + endpoint;
        return resp;
    }
    std::string errMsg = JsonExtractString(rawResp, "error");
    if (!errMsg.empty()) {
        resp.error = errMsg;
        return resp;
    }
    resp.success = true;
    std::string msgObj = JsonExtractObject(rawResp, "message");
    if (msgObj.empty()) {
        resp.content = rawResp;
        return resp;
    }
    resp.content = JsonExtractString(msgObj, "content");
    std::string toolCallsArr = JsonExtractObject(msgObj, "tool_calls");
    if (!toolCallsArr.empty() && toolCallsArr[0] == '[') {
        std::string firstTc = JsonFirstArrayElement(toolCallsArr);
        if (!firstTc.empty()) {
            resp.hasToolCall = true;
            std::string funcObj = JsonExtractObject(firstTc, "function");
            if (!funcObj.empty()) {
                resp.toolName = JsonExtractString(funcObj, "name");
                resp.toolArgsJson = JsonExtractObject(funcObj, "arguments");
                if (resp.toolArgsJson.empty()) {
                    std::string argsStr = JsonExtractString(funcObj, "arguments");
                    if (!argsStr.empty()) resp.toolArgsJson = argsStr;
                }
            }
            resp.toolCallId = JsonExtractString(firstTc, "id");
            if (resp.toolCallId.empty()) {
                resp.toolCallId = "call_" + std::to_string(GetTickCount());
            }
        }
    }
    return resp;
}
// -- Bounded Agentic Loop --

static std::atomic<bool> g_agentRunning{false};
static std::atomic<bool> g_agentCancelRequested{false};

static void AgentLogToOutput(const std::wstring& msg) {
    if (g_hwndOutput) AppendWindowText(g_hwndOutput, msg.c_str());
}

static void AgentLogToChat(const std::wstring& msg) {
    if (g_hwndChatHistory) AppendWindowText(g_hwndChatHistory, msg.c_str());
}

static DWORD WINAPI AgentLoopThread(LPVOID param) {
    std::string* pTask = reinterpret_cast<std::string*>(param);
    std::string userTask = *pTask;
    delete pTask;
    g_agentRunning.store(true);
    g_agentCancelRequested.store(false);
    const auto& cfg = g_backendConfigs[(size_t)AIBackendType::Ollama];
    std::string model = cfg.model;
    if (model.empty()) model = "qwen2.5-coder:14b";
    std::string toolSchemas = GetAgentToolSchemas();
    std::vector<OllamaChatMessage> messages;
    OllamaChatMessage sysMsg;
    sysMsg.role = "system";
    sysMsg.content = BuildAgentSystemPrompt();
    messages.push_back(sysMsg);
    OllamaChatMessage userMsg;
    userMsg.role = "user";
    userMsg.content = userTask;
    messages.push_back(userMsg);
    AgentLogToOutput(L"[Agent] Starting bounded agentic loop (max " +
                     std::to_wstring(MAX_AGENT_STEPS) + L" steps)...\r\n");
    AgentLogToOutput(L"[Agent] Model: " + Utf8ToWide(model) + L"\r\n");
    AgentLogToOutput(L"[Agent] Task: " + Utf8ToWide(userTask.substr(0, 200)) + L"\r\n");
    AgentLogToChat(L"\r\n=== AGENT MODE ===\r\nTask: " + Utf8ToWide(userTask.substr(0, 500)) + L"\r\n\r\n");
    std::string finalAnswer;
    int step = 0;
    while (step < MAX_AGENT_STEPS) {
        if (g_agentCancelRequested.load()) {
            AgentLogToOutput(L"[Agent] Cancelled by user.\r\n");
            AgentLogToChat(L"\r\n[Agent cancelled by user]\r\n");
            break;
        }
        step++;
        AgentLogToOutput(L"[Agent] Step " + std::to_wstring(step) + L"/" +
                         std::to_wstring(MAX_AGENT_STEPS) + L" - Sending to model...\r\n");
        OllamaChatResponse resp = OllamaAgentChat(model, messages, toolSchemas);
        if (!resp.success) {
            AgentLogToOutput(L"[Agent] Error: " + Utf8ToWide(resp.error) + L"\r\n");
            AgentLogToChat(L"\r\n[Agent error: " + Utf8ToWide(resp.error) + L"]\r\n");
            break;
        }
        if (resp.hasToolCall) {
            AgentLogToOutput(L"[Agent] Step " + std::to_wstring(step) +
                             L" -> Tool: " + Utf8ToWide(resp.toolName) + L"\r\n");
            AgentLogToChat(L"[Step " + std::to_wstring(step) + L"] Using tool: " +
                          Utf8ToWide(resp.toolName) + L"\r\n");
            if (!resp.content.empty()) {
                AgentLogToChat(L"Reasoning: " + Utf8ToWide(resp.content.substr(0, 500)) + L"\r\n");
            }
            AgentToolResult toolResult = DispatchAgentTool(resp.toolName, resp.toolArgsJson);
            std::string resultStr = toolResult.success ? toolResult.output : ("Error: " + toolResult.error);
            std::wstring displayResult = Utf8ToWide(resultStr.size() > 500 ?
                resultStr.substr(0, 500) + "..." : resultStr);
            AgentLogToOutput(L"[Agent]   Result: " + displayResult + L"\r\n");
            AgentLogToChat(L"  -> " + displayResult + L"\r\n");
            std::string tcJson = "[{\"id\":\"" + JsonEscape(resp.toolCallId) +
                                 "\",\"type\":\"function\",\"function\":{\"name\":\"" +
                                 JsonEscape(resp.toolName) + "\",\"arguments\":" +
                                 resp.toolArgsJson + "}}]";
            OllamaChatMessage asstMsg;
            asstMsg.role = "assistant";
            asstMsg.content = resp.content;
            asstMsg.toolCallsJson = tcJson;
            messages.push_back(asstMsg);
            OllamaChatMessage toolMsg;
            toolMsg.role = "tool";
            toolMsg.content = resultStr.size() > 16384 ?
                resultStr.substr(0, 16384) + "\n[TRUNCATED at 16KB]" : resultStr;
            messages.push_back(toolMsg);
        } else {
            finalAnswer = resp.content;
            AgentLogToOutput(L"[Agent] Step " + std::to_wstring(step) +
                             L" -> Final answer received.\r\n");
            AgentLogToChat(L"\r\n[Agent Answer]\r\n" + Utf8ToWide(finalAnswer) + L"\r\n");
            break;
        }
    }
    if (step >= MAX_AGENT_STEPS && finalAnswer.empty()) {
        AgentLogToOutput(L"[Agent] Step limit reached (" + std::to_wstring(MAX_AGENT_STEPS) +
                         L" steps). Stopping.\r\n");
        AgentLogToChat(L"\r\n[Agent reached step limit - " + std::to_wstring(MAX_AGENT_STEPS) +
                      L" steps used]\r\n");
    }
    AgentLogToOutput(L"[Agent] Agentic loop finished. Steps used: " +
                     std::to_wstring(step) + L"\r\n");
    AgentLogToChat(L"\r\n=== AGENT COMPLETE (" + std::to_wstring(step) + L" steps) ===\r\n");
    g_agentRunning.store(false);
    return 0;
}
void RunAutonomousMode() {
    if (g_agentRunning.load()) {
        int res = MessageBoxW(g_hwndMain,
            L"Agent is already running. Cancel current agent?",
            L"RawrXD - Autonomous Mode", MB_YESNO | MB_ICONQUESTION);
        if (res == IDYES) {
            g_agentCancelRequested.store(true);
        }
        return;
    }
    static wchar_t taskBuf[4096] = {};
    HWND hDlg = CreateWindowExW(WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"STATIC", L"RawrXD Agent - Enter Task",
        WS_POPUP | WS_VISIBLE | WS_CAPTION | WS_SYSMENU,
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 300,
        g_hwndMain, nullptr, g_hInstance, nullptr);
    CreateWindowExW(0, L"STATIC",
        L"Describe what you want the agent to do.\n"
        L"It will autonomously read files, write code, run commands, and more.",
        WS_CHILD | WS_VISIBLE, 15, 10, 560, 40, hDlg, nullptr, g_hInstance, nullptr);
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        15, 55, 560, 140, hDlg, (HMENU)100, g_hInstance, nullptr);
    int edLen = GetWindowTextLengthW(g_hwndEditor);
    if (edLen > 0 && edLen < 2000) {
        SetWindowTextW(hEdit, L"Analyze and improve the code in the current editor");
    }
    HWND hOk = CreateWindowExW(0, L"BUTTON", L"Start Agent",
        WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON, 350, 210, 110, 32,
        hDlg, (HMENU)IDOK, g_hInstance, nullptr);
    HWND hCancel = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, 470, 210, 100, 32,
        hDlg, (HMENU)IDCANCEL, g_hInstance, nullptr);
    if (g_hFontUI) {
        SendMessage(hEdit, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessage(hOk, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
        SendMessage(hCancel, WM_SETFONT, (WPARAM)g_hFontUI, TRUE);
    }
    SetFocus(hEdit);
    MSG msg;
    bool done = false;
    bool accepted = false;
    while (!done && GetMessageW(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND) {
            if (LOWORD(msg.wParam) == IDOK) {
                GetWindowTextW(hEdit, taskBuf, 4095);
                accepted = true;
                done = true;
            } else if (LOWORD(msg.wParam) == IDCANCEL) {
                done = true;
            }
        }
        if (msg.hwnd == hDlg && msg.message == WM_CLOSE) done = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    DestroyWindow(hDlg);
    if (!accepted || wcslen(taskBuf) == 0) return;
    std::string task = WideToUtf8(taskBuf);
    if (edLen > 0) {
        std::wstring edContent(edLen + 1, L'\0');
        GetWindowTextW(g_hwndEditor, &edContent[0], edLen + 1);
        edContent.resize(edLen);
        std::string currentFile;
        if (!g_tabs.empty() && g_activeTab >= 0 && g_activeTab < (int)g_tabs.size()) {
            currentFile = WideToUtf8(g_tabs[g_activeTab].filePath);
        }
        if (!currentFile.empty()) task += "\n\nCurrent file: " + currentFile;
        task += "\n\nCurrent editor content:\n```\n" + WideToUtf8(edContent) + "\n```";
    }
    std::string* pTask = new std::string(task);
    HANDLE hThread = CreateThread(nullptr, 0, AgentLoopThread, pTask, 0, nullptr);
    if (hThread) {
        CloseHandle(hThread);
        AppendWindowText(g_hwndOutput, L"[Agent] Autonomous agent launched in background.\r\n");
    } else {
        delete pTask;
        MessageBoxW(g_hwndMain, L"Failed to start agent thread.", L"Error", MB_OK | MB_ICONERROR);
    }
}