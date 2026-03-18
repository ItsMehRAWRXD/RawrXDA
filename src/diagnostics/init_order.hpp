// init_order.hpp
#pragma once
#include <atomic>
#include <vector>
#include <mutex>

class InitOrderDetector {
public:
    struct InitRecord {
        const char* name;
        const char* file;
        int line;
        int sequence;
        bool isConstructed;
    };

    static void RegisterConstruction(const char* name, const char* file, int line) {
        std::lock_guard<std::mutex> lock(s_mutex);

        int seq = s_sequence++;
        s_records.push_back({name, file, line, seq, true});

        HardLog("STATIC CONSTRUCT[%d]: %s at %s:%d", seq, name, file, line);

        if (s_mainEntered) {
            HardLog("WARNING: %s constructed AFTER main() - init order fiasco risk", name);
        }
    }

    static void RegisterDestruction(const char* name) {
        std::lock_guard<std::mutex> lock(s_mutex);
        HardLog("STATIC DESTROY: %s", name);

        // Mark as destroyed
        for (auto& r : s_records) {
            if (strcmp(r.name, name) == 0) {
                r.isConstructed = false;
                break;
            }
        }
    }

    static void MarkMainEntered() {
        s_mainEntered = true;
        HardLog("MAIN ENTERED - static init phase complete (%d objects)",
            (int)s_records.size());
    }

    static void CheckDependency(const char* dependent, const char* dependency) {
        std::lock_guard<std::mutex> lock(s_mutex);

        // Find dependency
        bool dependencyReady = false;
        for (auto& r : s_records) {
            if (strcmp(r.name, dependency) == 0 && r.isConstructed) {
                dependencyReady = true;
                break;
            }
        }

        if (!dependencyReady) {
            HardLog("INIT ORDER BUG: %s using %s before construction!",
                dependent, dependency);
            HardLog("  Available objects:");
            for (auto& r : s_records) {
                if (r.isConstructed) {
                    HardLog("    - %s (seq %d)", r.name, r.sequence);
                }
            }
            __debugbreak();
        }
    }

    static void DumpOrder() {
        std::lock_guard<std::mutex> lock(s_mutex);
        HardLog("=== Static Init Order ===");
        for (auto& r : s_records) {
            HardLog("[%d] %s at %s:%d %s",
                r.sequence, r.name, r.file, r.line,
                r.isConstructed ? "(alive)" : "(destroyed)");
        }
    }

private:
    static std::atomic<int> s_sequence;
    static std::vector<InitRecord> s_records;
    static std::atomic<bool> s_mainEntered;
    static std::mutex s_mutex;

    static void HardLog(const char* fmt, ...) {
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        HANDLE hFile = CreateFileA("d:\\rawrxd\\init_order_log.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

        if (hFile != INVALID_HANDLE_VALUE) {
            DWORD written;
            WriteFile(hFile, buf, strlen(buf), &written, nullptr);
            WriteFile(hFile, "\r\n", 2, &written, nullptr);
            CloseHandle(hFile);
        }

        OutputDebugStringA(buf);
        OutputDebugStringA("\n");
    }
};

// RAII guard for static objects
template<typename T>
class StaticGuard {
public:
    StaticGuard(const char* name, const char* file, int line) {
        InitOrderDetector::RegisterConstruction(name, file, line);
        m_name = name;
    }

    ~StaticGuard() {
        InitOrderDetector::RegisterDestruction(m_name);
    }

private:
    const char* m_name;
};

#define STATIC_OBJECT(T, name) \
    static StaticGuard<T> _guard_##name(#name, __FILE__, __LINE__); \
    static T name;

#define CHECK_DEP(dep) InitOrderDetector::CheckDependency(__FUNCTION__, #dep)