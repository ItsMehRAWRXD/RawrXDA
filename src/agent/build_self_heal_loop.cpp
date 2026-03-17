// build_self_heal_loop.cpp
// Self-Healing Build Loop — SOURCE-LEVEL compile-time error repair.
//
// Compile:  cl /std:c++20 /EHsc build_self_heal_loop.cpp /I(include dir)
//           link: winhttp.lib
//
// Zero stubs.  Every code path either succeeds or returns a descriptive
// error string.  No exceptions.  Win32 only.
// -----------------------------------------------------------------------

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include "build_self_heal_loop.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// nlohmann/json is already in the project for Ollama response parsing
#include <nlohmann/json.hpp>

namespace rawrxd {

// ═══════════════════════════════════════════════════════════════════════════
// Internal helpers
// ═══════════════════════════════════════════════════════════════════════════

static std::string wstrToStr(const std::wstring& w) {
    if (w.empty()) return {};
    int needed = WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                                     static_cast<int>(w.size()),
                                     nullptr, 0, nullptr, nullptr);
    std::string s(needed, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(),
                        static_cast<int>(w.size()),
                        s.data(), needed, nullptr, nullptr);
    return s;
}

static std::wstring strToWstr(const std::string& s) {
    if (s.empty()) return {};
    int needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                                     static_cast<int>(s.size()),
                                     nullptr, 0);
    std::wstring w(needed, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(),
                        static_cast<int>(s.size()),
                        w.data(), needed);
    return w;
}

// JSON-escape a UTF-8 string for inline embedding in a JSON string literal.
static std::string jsonEsc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 32);
    for (unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x",
                             static_cast<unsigned>(c));
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// Trim leading/trailing ASCII whitespace in-place.
static void trim(std::string& s) {
    s.erase(s.begin(),
            std::find_if(s.begin(), s.end(),
                         [](unsigned char c){ return !std::isspace(c); }));
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c){ return !std::isspace(c); }).base(),
            s.end());
}

// Split a string on a single-character delimiter.
static std::vector<std::string> splitLines(const std::string& s,
                                            char delim = '\n') {
    std::vector<std::string> lines;
    std::istringstream ss(s);
    std::string l;
    while (std::getline(ss, l, delim)) {
        // Strip trailing CR (Windows CRLF)
        if (!l.empty() && l.back() == '\r')
            l.pop_back();
        lines.push_back(std::move(l));
    }
    return lines;
}

// ═══════════════════════════════════════════════════════════════════════════
// BuildSelfHealer constructor
// ═══════════════════════════════════════════════════════════════════════════

BuildSelfHealer::BuildSelfHealer(Config cfg) : m_cfg(std::move(cfg)) {}

// ═══════════════════════════════════════════════════════════════════════════
// log
// ═══════════════════════════════════════════════════════════════════════════

void BuildSelfHealer::log(const std::string& msg) {
    if (m_cfg.verbose) {
        std::cout << "[BuildSelfHeal] " << msg << "\n";
        std::cout.flush();
    }
}

// ═══════════════════════════════════════════════════════════════════════════
// runBuild — execute buildCmd, capture combined stdout+stderr
// Returns the process exit code; outputOut receives the captured text.
// ═══════════════════════════════════════════════════════════════════════════

int BuildSelfHealer::runBuild(const std::string& buildCmd,
                               const std::string& workDir,
                               std::string& outputOut) {
    outputOut.clear();

    // ── Create an anonymous pipe for child stdout/stderr ──────────────────
    SECURITY_ATTRIBUTES sa{};
    sa.nLength        = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hReadPipe  = nullptr;
    HANDLE hWritePipe = nullptr;
    if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0)) {
        outputOut = "[runBuild] CreatePipe failed: " +
                    std::to_string(GetLastError());
        return -1;
    }
    // Don't inherit the read end in the child
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    // ── Launch the process ────────────────────────────────────────────────
    STARTUPINFOA si{};
    si.cb          = sizeof(si);
    si.hStdOutput  = hWritePipe;
    si.hStdError   = hWritePipe;
    si.hStdInput   = GetStdHandle(STD_INPUT_HANDLE);
    si.dwFlags     = STARTF_USESTDHANDLES;

    PROCESS_INFORMATION pi{};

    // Use "cmd /c <buildCmd>" so shell builtins and scripts work
    std::string cmdLine = "cmd /c " + buildCmd;

    std::wstring wCmd = strToWstr(cmdLine);
    std::wstring wDir = strToWstr(workDir);

    BOOL ok = CreateProcessW(
        nullptr,
        wCmd.data(),          // lpCommandLine (mutable)
        nullptr, nullptr,
        TRUE,                 // bInheritHandles
        CREATE_NO_WINDOW,
        nullptr,
        wDir.empty() ? nullptr : wDir.c_str(),
        reinterpret_cast<LPSTARTUPINFOW>(&si),  // STARTUPINFOA is layout-compatible
        &pi);

    if (!ok) {
        DWORD err = GetLastError();
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        outputOut = "[runBuild] CreateProcess failed err=" +
                    std::to_string(err) + " cmd=" + cmdLine;
        return -1;
    }

    // Close write end so we'll get EOF when the child exits
    CloseHandle(hWritePipe);

    // ── Read output until pipe closes ─────────────────────────────────────
    std::string captured;
    char buf[4096];
    DWORD bytesRead;
    while (ReadFile(hReadPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr)
           && bytesRead > 0) {
        buf[bytesRead] = '\0';
        captured.append(buf, bytesRead);
    }
    CloseHandle(hReadPipe);

    // ── Wait for process with timeout ─────────────────────────────────────
    WaitForSingleObject(pi.hProcess, m_cfg.buildTimeoutMs);

    DWORD exitCode = static_cast<DWORD>(-1);
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    outputOut = captured;
    return static_cast<int>(exitCode);
}

// ═══════════════════════════════════════════════════════════════════════════
// parseErrors — extract BuildError records from build output
//
// Handles:
//   MSVC C/C++  :  path(line): error C1234: ...
//                  path(line,col): error C1234: ...
//   MSVC Linker :  path.obj : error LNK2005: ...
//                  path.lib : error LNK1120: ...
//   ML64 MASM   :  path.asm(line) : error A2006 : ...
//                  path.asm(line) : warning A4072 : ...  (as errors if /WX)
// ═══════════════════════════════════════════════════════════════════════════

std::vector<BuildError> BuildSelfHealer::parseErrors(
    const std::string& buildOutput)
{
    std::vector<BuildError> errors;
    auto lines = splitLines(buildOutput);

    for (const auto& raw : lines) {
        // We need at least " error " somewhere
        auto errPos = raw.find(" error ");
        if (errPos == std::string::npos) continue;

        // ── Try MSVC C/C++ / ML64 pattern ─────────────────────────────
        // Format:  filepath(LINE[,COL]): error CXXX|AXXX: message
        // Or:      filepath(LINE[,COL]) : error AXXX : message  (ML64 extra spaces)
        //
        // Find the first '(' that precedes errPos.
        auto parenOpen  = raw.rfind('(', errPos);
        auto parenClose = raw.find(')', parenOpen == std::string::npos
                                         ? 0
                                         : parenOpen);

        if (parenOpen  != std::string::npos &&
            parenClose != std::string::npos &&
            parenClose  < errPos) {

            std::string filePart  = raw.substr(0, parenOpen);
            std::string linePart  = raw.substr(parenOpen + 1,
                                               parenClose - parenOpen - 1);
            std::string afterParen = raw.substr(parenClose + 1);

            // Strip optional leading whitespace + colon from afterParen
            size_t colonPos = afterParen.find(':');
            if (colonPos == std::string::npos) continue;
            std::string errorPart = afterParen.substr(colonPos + 1);
            trim(errorPart);

            // errorPart should now start with "error CXXX:" or "error AXXX :"
            if (errorPart.rfind("error ", 0) != 0) continue;
            errorPart = errorPart.substr(6); // skip "error "

            std::string code, message;
            auto spaceAfterCode = errorPart.find_first_of(": ");
            if (spaceAfterCode != std::string::npos) {
                code    = errorPart.substr(0, spaceAfterCode);
                message = errorPart.substr(spaceAfterCode);
                while (!message.empty() &&
                       (message.front() == ':' || message.front() == ' '))
                    message.erase(message.begin());
            } else {
                code    = errorPart;
                message = "";
            }

            // Extract line number (take part before optional comma)
            int lineNo = 0;
            auto commaInLine = linePart.find(',');
            std::string lineNumStr = commaInLine != std::string::npos
                                   ? linePart.substr(0, commaInLine)
                                   : linePart;
            try { lineNo = std::stoi(lineNumStr); }
            catch (...) { lineNo = 0; }

            trim(filePart);
            trim(code);
            trim(message);

            if (!filePart.empty() && !code.empty()) {
                errors.push_back({ filePart, lineNo, code, message });
            }
            continue;
        }

        // ── Try MSVC Linker pattern ────────────────────────────────────
        // Format:  path.ext : error LNKxxxx: message
        // '<space>:<space>error ' already found; walk back to find filename.
        // errPos points to start of " error "
        // Before that should be " : " or ": "
        auto colonWalk = raw.rfind(':', errPos);
        if (colonWalk == std::string::npos) continue;

        std::string filePart = raw.substr(0, colonWalk);
        std::string restPart = raw.substr(errPos + 7); // skip " error "
        trim(filePart);
        trim(restPart);

        // restPart: "LNK2005: ..."
        std::string code, message;
        auto colonInRest = restPart.find(':');
        if (colonInRest != std::string::npos) {
            code    = restPart.substr(0, colonInRest);
            message = restPart.substr(colonInRest + 1);
            trim(code);
            trim(message);
        } else {
            code    = restPart;
            message = "";
        }

        // Only accept if code looks like "LNKxxxx" or "Cxxxx"
        if (code.size() >= 4 && (code[0] == 'L' || code[0] == 'C' ||
                                   code[0] == 'A')) {
            errors.push_back({ filePart, 0, code, message });
        }
    }

    // Deduplicate: same file+line+code
    std::sort(errors.begin(), errors.end(),
              [](const BuildError& a, const BuildError& b) {
                  if (a.file != b.file)  return a.file  < b.file;
                  if (a.line != b.line)  return a.line  < b.line;
                  return a.code < b.code;
              });
    errors.erase(
        std::unique(errors.begin(), errors.end(),
                    [](const BuildError& a, const BuildError& b) {
                        return a.file == b.file &&
                               a.line == b.line &&
                               a.code == b.code;
                    }),
        errors.end());

    return errors;
}

// ═══════════════════════════════════════════════════════════════════════════
// resolvePath — normalise a path relative to srcRoot
// ═══════════════════════════════════════════════════════════════════════════

std::string BuildSelfHealer::resolvePath(const std::string& path,
                                          const std::string& srcRoot) {
    if (path.size() >= 2 && path[1] == ':') return path; // already absolute
    if (!path.empty() && path[0] == '\\')   return path;
    if (!srcRoot.empty())
        return srcRoot + "\\" + path;
    return path;
}

// ═══════════════════════════════════════════════════════════════════════════
// readContext — read lines around errorLine, annotated with line numbers
// ═══════════════════════════════════════════════════════════════════════════

std::string BuildSelfHealer::readContext(const std::string& absPath,
                                          int errorLine,
                                          int contextLines) {
    std::ifstream f(absPath);
    if (!f.is_open()) return "[cannot open " + absPath + "]";

    std::vector<std::string> fileLines;
    std::string l;
    while (std::getline(f, l)) {
        if (!l.empty() && l.back() == '\r') l.pop_back();
        fileLines.push_back(l);
    }

    int total     = static_cast<int>(fileLines.size());
    int startLine = std::max(1, errorLine - contextLines);
    int endLine   = std::min(total, errorLine + contextLines);

    std::string out;
    for (int i = startLine; i <= endLine; ++i) {
        char prefix[16];
        snprintf(prefix, sizeof(prefix),
                 i == errorLine ? ">%4d| " : " %4d| ", i);
        out += prefix;
        out += fileLines[static_cast<size_t>(i - 1)];
        out += '\n';
    }
    return out;
}

// ═══════════════════════════════════════════════════════════════════════════
// buildLLMPrompt — format errors + source context into an LLM prompt
// ═══════════════════════════════════════════════════════════════════════════

std::string BuildSelfHealer::buildLLMPrompt(
    const std::vector<BuildError>& errors,
    const std::string& srcRoot)
{
    std::string prompt;
    prompt.reserve(4096);

    prompt +=
        "You are a MASM64/C++ compile-error repair agent for the RawrXD sovereign IDE.\n"
        "Rules:\n"
        " 1. Output ONLY fix blocks — no prose, no explanations.\n"
        " 2. Each fix block uses this exact format:\n"
        "      --- fix begin (relative/path/to/file.cpp:START-END) ---\n"
        "      <replacement lines>\n"
        "      --- fix end ---\n"
        "    where START and END are 1-based inclusive line numbers to replace.\n"
        " 3. If the fix requires deleting lines with no replacement, leave the body empty.\n"
        " 4. Do not change code outside the minimal repair window.\n"
        " 5. Preserve indentation and coding style.\n\n";

    prompt += "===== ERRORS =====\n";
    for (const auto& e : errors) {
        char buf[512];
        snprintf(buf, sizeof(buf), "  [%s] %s(%d): %s\n",
                 e.code.c_str(), e.file.c_str(), e.line, e.message.c_str());
        prompt += buf;
    }
    prompt += "\n";

    // Group errors by file to avoid duplicate context reads
    struct FileCtx { std::string absPath; std::vector<int> errorLines; };
    std::vector<FileCtx> fileCtxs;

    for (const auto& e : errors) {
        if (e.line == 0) continue; // linker errors — no file context
        std::string abs = resolvePath(e.file, srcRoot);
        bool found = false;
        for (auto& fc : fileCtxs) {
            if (fc.absPath == abs) {
                fc.errorLines.push_back(e.line);
                found = true;
                break;
            }
        }
        if (!found)
            fileCtxs.push_back({ abs, { e.line } });
    }

    if (!fileCtxs.empty()) {
        prompt += "===== SOURCE CONTEXT =====\n";
        for (const auto& fc : fileCtxs) {
            // Show the region spanning all error lines in this file
            int lo = *std::min_element(fc.errorLines.begin(), fc.errorLines.end());
            int hi = *std::max_element(fc.errorLines.begin(), fc.errorLines.end());
            // Widen by contextLines on each side
            int startLineForContext = std::max(1, lo - m_cfg.contextLines);
            int endLineForContext   = hi + m_cfg.contextLines;

            // We'll just call readContext centred on the midpoint
            int mid = (lo + hi) / 2;
            int halfSpan = std::max(m_cfg.contextLines,
                                    (endLineForContext - startLineForContext) / 2 + 1);

            prompt += "FILE: " + fc.absPath + "\n";
            prompt += readContext(fc.absPath, mid, halfSpan);
            prompt += "\n";
        }
    }

    return prompt;
}

// ═══════════════════════════════════════════════════════════════════════════
// callOllama — WinHTTP POST to /api/generate, return LLM "response" field
// ═══════════════════════════════════════════════════════════════════════════

std::string BuildSelfHealer::callOllama(const std::string& prompt) {
    // Build JSON request body
    std::string requestJson =
        "{\"model\":\""  + jsonEsc(m_cfg.ollamaModel) + "\","
        "\"prompt\":\""  + jsonEsc(prompt)             + "\","
        "\"stream\":false}";

    std::wstring wHost = strToWstr(m_cfg.ollamaHost);

    HINTERNET hSession = WinHttpOpen(
        L"RawrXD-BuildHeal/1.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);
    if (!hSession) {
        log("WinHttpOpen failed err=" + std::to_string(GetLastError()));
        return {};
    }

    // Set timeouts (resolve, connect, send, receive) all to ollamaTimeoutMs
    WinHttpSetTimeouts(hSession,
                       static_cast<int>(m_cfg.ollamaTimeoutMs),
                       static_cast<int>(m_cfg.ollamaTimeoutMs),
                       static_cast<int>(m_cfg.ollamaTimeoutMs),
                       static_cast<int>(m_cfg.ollamaTimeoutMs));

    HINTERNET hConnect = WinHttpConnect(
        hSession,
        wHost.c_str(),
        m_cfg.ollamaPort,
        0);
    if (!hConnect) {
        log("WinHttpConnect failed err=" + std::to_string(GetLastError()));
        WinHttpCloseHandle(hSession);
        return {};
    }

    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"POST",
        L"/api/generate",
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        0);  // no WINHTTP_FLAG_SECURE (plain HTTP to local Ollama)
    if (!hRequest) {
        log("WinHttpOpenRequest failed err=" + std::to_string(GetLastError()));
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return {};
    }

    BOOL sent = WinHttpSendRequest(
        hRequest,
        L"Content-Type: application/json\r\n",
        static_cast<DWORD>(-1L),
        const_cast<char*>(requestJson.c_str()),
        static_cast<DWORD>(requestJson.size()),
        static_cast<DWORD>(requestJson.size()),
        0);

    std::string rawResponse;
    if (sent && WinHttpReceiveResponse(hRequest, nullptr)) {
        DWORD avail = 0;
        while (WinHttpQueryDataAvailable(hRequest, &avail) && avail > 0) {
            std::vector<char> buf(static_cast<size_t>(avail) + 1, '\0');
            DWORD bytesRead = 0;
            if (WinHttpReadData(hRequest, buf.data(), avail, &bytesRead))
                rawResponse.append(buf.data(), bytesRead);
        }
    } else {
        log("WinHTTP send/recv failed err=" + std::to_string(GetLastError()));
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (rawResponse.empty()) return {};

    // Parse Ollama JSON response → extract "response" field
    try {
        auto j = nlohmann::json::parse(rawResponse);
        if (j.contains("response") && j["response"].is_string())
            return j["response"].get<std::string>();
        if (j.contains("error") && j["error"].is_string()) {
            log("Ollama error: " + j["error"].get<std::string>());
            return {};
        }
    } catch (const nlohmann::json::exception& je) {
        log(std::string("JSON parse error: ") + je.what());
    }

    return rawResponse; // fallback: return raw text if JSON parse fails
}

// ═══════════════════════════════════════════════════════════════════════════
// extractPatches — pull FilePatch records from LLM response
//
// Expected format (one or more blocks):
//   --- fix begin (relative/path/to/file.cpp:START-END) ---
//   <replacement lines, may be empty>
//   --- fix end ---
// ═══════════════════════════════════════════════════════════════════════════

std::vector<FilePatch> BuildSelfHealer::extractPatches(
    const std::string& llmResponse)
{
    std::vector<FilePatch> patches;
    auto lines = splitLines(llmResponse);

    const std::string BEGIN_TAG = "--- fix begin (";
    const std::string END_TAG   = "--- fix end ---";

    size_t i = 0;
    while (i < lines.size()) {
        // Find "--- fix begin (...) ---"
        size_t tagPos = lines[i].find(BEGIN_TAG);
        if (tagPos == std::string::npos) { ++i; continue; }

        std::string meta = lines[i].substr(tagPos + BEGIN_TAG.size());
        // meta should now be "path:START-END) ---" or "path:START-END)---"
        auto closeParenPos = meta.rfind(')');
        if (closeParenPos == std::string::npos) { ++i; continue; }

        std::string pathRange = meta.substr(0, closeParenPos);
        // pathRange = "relative/path/to/file.cpp:START-END"
        auto colonPos = pathRange.rfind(':');
        if (colonPos == std::string::npos) { ++i; continue; }

        std::string relPath  = pathRange.substr(0, colonPos);
        std::string rangeStr = pathRange.substr(colonPos + 1);

        int startLine = 0, endLine = 0;
        auto dashPos = rangeStr.find('-');
        if (dashPos != std::string::npos) {
            try { startLine = std::stoi(rangeStr.substr(0, dashPos));   } catch (...) {}
            try { endLine   = std::stoi(rangeStr.substr(dashPos + 1));  } catch (...) {}
        } else {
            try { startLine = endLine = std::stoi(rangeStr);            } catch (...) {}
        }

        trim(relPath);
        if (relPath.empty() || startLine <= 0) { ++i; continue; }
        if (endLine < startLine) endLine = startLine;

        // Collect body until "--- fix end ---"
        ++i;
        std::string body;
        while (i < lines.size()) {
            if (lines[i].find(END_TAG) != std::string::npos) break;
            body += lines[i];
            body += '\n';
            ++i;
        }
        ++i; // skip END_TAG line

        // Normalise Windows back-slashes to whatever was in the compiler output
        // (keep relPath as-is; applyPatches will try forward and back slash)
        patches.push_back({ relPath, startLine, endLine, body });
    }
    return patches;
}

// ═══════════════════════════════════════════════════════════════════════════
// applyPatches — write LLM-suggested replacements to source files
// Returns true if at least one file was written.
// ═══════════════════════════════════════════════════════════════════════════

bool BuildSelfHealer::applyPatches(const std::vector<FilePatch>& patches,
                                    const std::string& srcRoot) {
    if (patches.empty()) return false;

    bool anyApplied = false;

    for (const auto& patch : patches) {
        std::string absPath = resolvePath(patch.relPath, srcRoot);

        // Also try with forward-slash normalisation
        std::string absPathFwd = absPath;
        std::replace(absPathFwd.begin(), absPathFwd.end(), '/', '\\');

        // Read existing file
        std::ifstream fin(absPathFwd);
        if (!fin.is_open()) {
            // Try exact path
            fin.open(absPath);
            if (!fin.is_open()) {
                log("applyPatch: cannot open " + absPath);
                continue;
            }
            absPathFwd = absPath;
        }

        std::vector<std::string> fileLines;
        std::string l;
        while (std::getline(fin, l)) {
            if (!l.empty() && l.back() == '\r') l.pop_back();
            fileLines.push_back(l);
        }
        fin.close();

        int total = static_cast<int>(fileLines.size());
        int s = patch.startLine - 1; // 0-based
        int e = patch.endLine   - 1; // 0-based inclusive
        if (s < 0 || s > total) {
            log("applyPatch: startLine out of range in " + absPathFwd);
            continue;
        }
        e = std::min(e, total - 1);

        // Build replacement lines from patch body
        auto replacementLines = splitLines(patch.replacement);
        // Remove trailing empty line added by our extraction
        while (!replacementLines.empty() &&
               replacementLines.back().empty())
            replacementLines.pop_back();

        // Splice
        std::vector<std::string> newFile;
        newFile.reserve(fileLines.size() + replacementLines.size());
        for (int k = 0; k < s; ++k)
            newFile.push_back(fileLines[static_cast<size_t>(k)]);
        for (const auto& rl : replacementLines)
            newFile.push_back(rl);
        for (int k = e + 1; k < total; ++k)
            newFile.push_back(fileLines[static_cast<size_t>(k)]);

        // Write to a temp file then rename (pseudo-atomic)
        std::string tmpPath = absPathFwd + ".bsheal.tmp";
        {
            std::ofstream fout(tmpPath, std::ios::binary);
            if (!fout.is_open()) {
                log("applyPatch: cannot write tmp " + tmpPath);
                continue;
            }
            for (const auto& wl : newFile) {
                fout.write(wl.c_str(), static_cast<std::streamsize>(wl.size()));
                fout.put('\n');
            }
        }

        // Replace original
        if (!MoveFileExA(tmpPath.c_str(), absPathFwd.c_str(),
                         MOVEFILE_REPLACE_EXISTING)) {
            log("applyPatch: MoveFileEx failed err=" +
                std::to_string(GetLastError()));
            DeleteFileA(tmpPath.c_str());
            continue;
        }

        log("Patched " + absPathFwd + " lines " +
            std::to_string(patch.startLine) + "-" +
            std::to_string(patch.endLine));
        anyApplied = true;
    }

    return anyApplied;
}

// ═══════════════════════════════════════════════════════════════════════════
// run — main self-healing loop
// ═══════════════════════════════════════════════════════════════════════════

HealResult BuildSelfHealer::run(const std::string& buildCmd,
                                 const std::string& srcRoot) {
    HealResult result{};

    log("Starting self-heal loop: " + buildCmd);
    log("srcRoot = " + srcRoot);
    log("maxRetries = " + std::to_string(m_cfg.maxRetries));

    for (int attempt = 0; attempt <= m_cfg.maxRetries; ++attempt) {
        result.attemptsUsed = attempt;

        // ── 1. Run the build ───────────────────────────────────────────
        log("Attempt " + std::to_string(attempt) +
            "/" + std::to_string(m_cfg.maxRetries) + " — running build...");

        int exitCode = runBuild(buildCmd, srcRoot, result.buildOutput);

        log("Build exited with code " + std::to_string(exitCode));

        // ── 2. Parse errors ────────────────────────────────────────────
        auto errors = parseErrors(result.buildOutput);

        if (attempt == 0)
            result.errorsOnEntry = static_cast<int>(errors.size());

        if (errors.empty() && exitCode == 0) {
            // Success!
            result.success = true;
            result.detail  = "Build succeeded after " +
                             std::to_string(attempt) + " healing iteration(s).";
            log(result.detail);
            return result;
        }

        if (errors.empty() && exitCode != 0) {
            // Build failed but no parseable errors — could be a script error
            result.detail = "Build script returned non-zero exit but "
                            "no parseable errors found. exitCode=" +
                            std::to_string(exitCode);
            log(result.detail);
            // Still propagate to LLM with raw output as "error"
            errors.push_back({ "(build script)", 0, "SCRIPT",
                                result.buildOutput.substr(
                                    0, std::min<size_t>(512,
                                                        result.buildOutput.size())) });
        }

        log("Found " + std::to_string(errors.size()) + " error(s).");

        // ── 3. Check retry budget ──────────────────────────────────────
        if (attempt == m_cfg.maxRetries) {
            result.detail = "Exhausted " + std::to_string(m_cfg.maxRetries) +
                            " repair attempt(s) with " +
                            std::to_string(errors.size()) +
                            " error(s) remaining.";
            log(result.detail);
            return result;
        }

        // ── 4. Build LLM prompt ────────────────────────────────────────
        log("Building LLM prompt for " +
            std::to_string(errors.size()) + " error(s)...");
        std::string prompt = buildLLMPrompt(errors, srcRoot);

        // ── 5. Query Ollama ────────────────────────────────────────────
        log("Querying Ollama model '" + m_cfg.ollamaModel + "'...");
        std::string llmText = callOllama(prompt);
        result.llmResponse = llmText;

        if (llmText.empty()) {
            result.detail = "Ollama returned empty response on attempt " +
                            std::to_string(attempt) + ".";
            log(result.detail + " Retrying build without change...");
            // No patches to apply — loop will retry the same build, then exit.
            continue;
        }

        log("LLM response length: " + std::to_string(llmText.size()) + " chars.");

        // ── 6. Extract and apply patches ──────────────────────────────
        auto patches = extractPatches(llmText);
        log("Extracted " + std::to_string(patches.size()) + " patch(es).");

        if (patches.empty()) {
            log("No parseable fix blocks in LLM response; will retry.");
            // Continue loop — maybe next attempt the build state differs.
            continue;
        }

        bool applied = applyPatches(patches, srcRoot);
        if (!applied) {
            log("No patches could be applied to filesystem; retrying.");
            continue;
        }

        log("Patches applied. Re-running build...");
        // Loop back to attempt+1
    }

    // Should not reach here but defensive return
    result.detail = "Heap loop ended without resolution.";
    return result;
}

} // namespace rawrxd

// ═══════════════════════════════════════════════════════════════════════════
// Optional standalone entry point — invoked by RawrXD --heal-build flag
// ═══════════════════════════════════════════════════════════════════════════
extern "C" int RawrXD_HealBuild(
    const char* buildCmd,
    const char* srcRoot,
    const char* ollamaModel,
    int         maxRetries)
{
    rawrxd::BuildSelfHealer::Config cfg{};
    if (ollamaModel && ollamaModel[0])
        cfg.ollamaModel = ollamaModel;
    if (maxRetries > 0)
        cfg.maxRetries = maxRetries;

    rawrxd::BuildSelfHealer healer(cfg);
    auto result = healer.run(
        buildCmd  ? buildCmd  : "",
        srcRoot   ? srcRoot   : ".");

    std::cout << "\n[HealBuild] " << (result.success ? "SUCCESS" : "FAILED")
              << " — attempts=" << result.attemptsUsed
              << " errorsOnEntry=" << result.errorsOnEntry
              << "\n";
    if (!result.detail.empty())
        std::cout << "[HealBuild] " << result.detail << "\n";

    return result.success ? 0 : 1;
}
