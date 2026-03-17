// crash_handler.cpp — Qt-free crash handler using Win32 + STL
// Purged: QDir, QFileInfo, QDebug, QObject (partial prior purge was broken)
// Replaced with: Win32 MiniDump API, std::filesystem, fprintf
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "dbghelp.lib")

#include <string>
#include <unordered_map>
#include <mutex>
#include <filesystem>
#include <cstdio>

// ---------------------------------------------------------------------------
// CrashHandler — pure Win32/C++20, zero Qt
// ---------------------------------------------------------------------------
class CrashHandler {
public:
    CrashHandler() = default;
    ~CrashHandler() = default;

    CrashHandler(const CrashHandler&) = delete;
    CrashHandler& operator=(const CrashHandler&) = delete;

    bool initialize(const std::string& reporterPath,
                    const std::string& databasePath,
                    const std::string& url)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(reporterPath)) {
            fprintf(stderr, "[CrashHandler] Reporter path does not exist: %s\n",
                    reporterPath.c_str());
            return false;
        }

        std::error_code ec;
        if (!fs::exists(databasePath)) {
            if (!fs::create_directories(databasePath, ec)) {
                fprintf(stderr, "[CrashHandler] Failed to create database dir: %s (%s)\n",
                        databasePath.c_str(), ec.message().c_str());
                return false;
            }
        }

        {
            std::lock_guard<std::mutex> lk(m_mutex);
            m_reporterPath = reporterPath;
            m_databasePath = databasePath;
            m_uploadUrl    = url;
            m_initialized  = true;
        }

        // Install global SEH handler
        SetUnhandledExceptionFilter(&CrashHandler::topLevelExceptionFilter);
        s_instance = this;

        fprintf(stderr, "[CrashHandler] Initialized: reporter=%s database=%s url=%s\n",
                reporterPath.c_str(), databasePath.c_str(), url.c_str());
        return true;
    }

    void setMetadata(const std::string& key, const std::string& value)
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        m_metadata[key] = value;
        fprintf(stderr, "[CrashHandler] Metadata: %s = %s\n",
                key.c_str(), value.c_str());
    }

    bool isInitialized() const
    {
        std::lock_guard<std::mutex> lk(m_mutex);
        return m_initialized;
    }

private:
    static LONG WINAPI topLevelExceptionFilter(EXCEPTION_POINTERS* exInfo)
    {
        if (!s_instance) return EXCEPTION_CONTINUE_SEARCH;

        std::lock_guard<std::mutex> lk(s_instance->m_mutex);
        std::string dumpPath = s_instance->m_databasePath + "\\crash.dmp";

        HANDLE hFile = CreateFileA(dumpPath.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION mei{};
            mei.ThreadId = GetCurrentThreadId();
            mei.ExceptionPointers = exInfo;
            mei.ClientPointers = FALSE;

            MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                              hFile,
                              static_cast<MINIDUMP_TYPE>(
                                  MiniDumpWithDataSegs | MiniDumpWithHandleData |
                                  MiniDumpWithThreadInfo),
                              &mei, nullptr, nullptr);
            CloseHandle(hFile);
            fprintf(stderr, "[CrashHandler] Dump written: %s\n", dumpPath.c_str());
        }

        return EXCEPTION_CONTINUE_SEARCH;
    }

    mutable std::mutex                              m_mutex;
    std::string                                     m_reporterPath;
    std::string                                     m_databasePath;
    std::string                                     m_uploadUrl;
    std::unordered_map<std::string, std::string>    m_metadata;
    bool                                            m_initialized = false;

    static inline CrashHandler* s_instance = nullptr;
};