// uaf_detector.hpp
#pragma once
#include <windows.h>
#include <unordered_map>
#include <mutex>
#include <cstring>

class UAFDetector {
public:
    static constexpr uint8_t FREED_PATTERN = 0xDD;
    static constexpr uint64_t CANARY = 0xDEADBEEFCAFEBABEULL;

    struct BlockHeader {
        uint64_t canary;
        size_t size;
        const char* allocFile;
        int allocLine;
        const char* freeFile;
        int freeLine;
        bool alive;
        uint8_t padding[7];
    };

    static void* Alloc(size_t size, const char* file, int line) {
        size_t total = sizeof(BlockHeader) + size + 8; // header + data + tail canary

        void* raw = VirtualAlloc(nullptr, total, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!raw) return nullptr;

        BlockHeader* header = (BlockHeader*)raw;
        header->canary = CANARY;
        header->size = size;
        header->allocFile = file;
        header->allocLine = line;
        header->freeFile = nullptr;
        header->freeLine = 0;
        header->alive = true;

        void* user = (char*)raw + sizeof(BlockHeader);

        // Tail canary
        *(uint64_t*)((char*)user + size) = CANARY;

        {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_blocks[user] = header;
        }

        return user;
    }

    static void Free(void* ptr, const char* file, int line) {
        if (!ptr) return;

        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_blocks.find(ptr);
        if (it == s_blocks.end()) {
            HardLog("INVALID FREE: %p (never allocated)", ptr);
            __debugbreak();
            return;
        }

        BlockHeader* header = it->second;

        if (!header->alive) {
            HardLog("DOUBLE FREE: %p", ptr);
            HardLog("  First freed at: %s:%d", header->freeFile, header->freeLine);
            HardLog("  Second free at: %s:%d", file, line);
            HardLog("  Originally allocated: %s:%d", header->allocFile, header->allocLine);
            __debugbreak();
            return;
        }

        // Validate canaries
        if (header->canary != CANARY) {
            HardLog("HEADER CORRUPTION: %p", ptr);
            HardLog("  Expected: %llX, Got: %llX", CANARY, header->canary);
            __debugbreak();
        }

        uint64_t* tail = (uint64_t*)((char*)ptr + header->size);
        if (*tail != CANARY) {
            HardLog("BUFFER OVERFLOW: %p", ptr);
            HardLog("  Size: %zu", header->size);
            HardLog("  Allocated: %s:%d", header->allocFile, header->allocLine);
            __debugbreak();
        }

        // Mark freed but KEEP BLOCK for UAF detection
        header->alive = false;
        header->freeFile = file;
        header->freeLine = line;

        // Fill with pattern (like debug heap)
        memset(ptr, FREED_PATTERN, header->size);

        // DON'T actually free - keep for detection!
        // VirtualFree would make UAF undetectable
    }

    static void ValidateAccess(void* ptr, const char* operation) {
        std::lock_guard<std::mutex> lock(s_mutex);
        auto it = s_blocks.find(ptr);

        if (it == s_blocks.end()) {
            // Could be stack/static - ignore
            return;
        }

        BlockHeader* header = it->second;

        if (!header->alive) {
            HardLog("USE AFTER FREE: %p in %s", ptr, operation);
            HardLog("  Allocated: %s:%d", header->allocFile, header->allocLine);
            HardLog("  Freed: %s:%d", header->freeFile, header->freeLine);
            HardLog("  Size: %zu bytes", header->size);

            // Check if pattern still intact
            bool patternIntact = true;
            for (size_t i = 0; i < header->size && i < 64; i++) {
                if (((uint8_t*)ptr)[i] != FREED_PATTERN) {
                    patternIntact = false;
                    break;
                }
            }

            if (patternIntact) {
                HardLog("  Memory still has freed pattern (0xDD) - classic UAF");
            } else {
                HardLog("  Memory was modified after free!");
            }

            __debugbreak();
        }

        // Check for corruption of live object
        if (header->canary != CANARY) {
            HardLog("CORRUPTION: %p header smashed", ptr);
            __debugbreak();
        }
    }

    static void DumpStats() {
        std::lock_guard<std::mutex> lock(s_mutex);
        size_t alive = 0, dead = 0;
        for (auto& [ptr, header] : s_blocks) {
            if (header->alive) alive++;
            else dead++;
        }
        HardLog("HEAP STATS: %zu alive, %zu dead (zombie) blocks", alive, dead);
    }

private:
    static std::unordered_map<void*, BlockHeader*> s_blocks;
    static std::mutex s_mutex;

    static void HardLog(const char* fmt, ...) {
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        HANDLE hFile = CreateFileA("d:\\rawrxd\\uaf_log.txt",
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

// Override new/delete
#define UAF_NEW new(__FILE__, __LINE__)
void* operator new(size_t size, const char* file, int line) {
    return UAFDetector::Alloc(size, file, line);
}
void operator delete(void* p) {
    UAFDetector::Free(p, __FILE__, __LINE__);
}