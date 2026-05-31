// ============================================================================
// native_ide_tools_extended.cpp — Extended Native IDE Tools Implementation
// Chunked I/O, Performance Tools, Deployment Tools, Configuration Tools
// ============================================================================

#include "native_ide_tools.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <functional>
#include <cmath>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#define PATH_SEP '\\'
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#define PATH_SEP '/'
#endif

namespace RawrXD {
namespace NativeIDE {

// ============================================================================
// CHUNKED FILE I/O — Fixes 2GB+ File Limit (P0 Blocker)
// ============================================================================

namespace ChunkedIO {

// 64MB chunk size for large file operations
constexpr size_t CHUNK_SIZE = 64 * 1024 * 1024;

struct ChunkedFile {
    FILE* handle;
    std::string path;
    size_t fileSize;
    size_t currentPos;
    bool isOpen;
    bool useMemoryMap;
    
#ifdef _WIN32
    HANDLE hFile;
    HANDLE hMap;
    void* mappedView;
#else
    int fd;
    void* mappedView;
#endif
};

// Open file with chunked reading support
ChunkedFile* ChunkedOpen(const char* path, bool readOnly = true) {
    ChunkedFile* cf = new ChunkedFile();
    cf->path = path;
    cf->currentPos = 0;
    cf->isOpen = false;
    cf->useMemoryMap = false;
    cf->mappedView = nullptr;
    
#ifdef _WIN32
    cf->hFile = INVALID_HANDLE_VALUE;
    cf->hMap = nullptr;
    
    // Try memory mapping first for large files
    if (readOnly) {
        cf->hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 
                                nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (cf->hFile != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER size;
            if (GetFileSizeEx(cf->hFile, &size)) {
                cf->fileSize = (size_t)size.QuadPart;
                
                // Use memory mapping for files > 100MB
                if (cf->fileSize > 100 * 1024 * 1024) {
                    cf->hMap = CreateFileMappingA(cf->hFile, nullptr, PAGE_READONLY, 
                                                  0, 0, nullptr);
                    if (cf->hMap) {
                        cf->mappedView = MapViewOfFile(cf->hMap, FILE_MAP_READ, 
                                                        0, 0, 0);
                        if (cf->mappedView) {
                            cf->useMemoryMap = true;
                            cf->isOpen = true;
                            return cf;
                        }
                        CloseHandle(cf->hMap);
                        cf->hMap = nullptr;
                    }
                }
            }
        }
    }
#endif
    
    // Fallback to standard FILE I/O
    cf->handle = std::fopen(path, readOnly ? "rb" : "r+b");
    if (!cf->handle) {
        delete cf;
        return nullptr;
    }
    
    std::fseek(cf->handle, 0, SEEK_END);
    cf->fileSize = std::ftell(cf->handle);
    std::fseek(cf->handle, 0, SEEK_SET);
    cf->isOpen = true;
    
    return cf;
}

// Close chunked file
void ChunkedClose(ChunkedFile* cf) {
    if (!cf) return;
    
#ifdef _WIN32
    if (cf->mappedView) {
        UnmapViewOfFile(cf->mappedView);
    }
    if (cf->hMap) {
        CloseHandle(cf->hMap);
    }
    if (cf->hFile != INVALID_HANDLE_VALUE) {
        CloseHandle(cf->hFile);
    }
#endif
    
    if (cf->handle) {
        std::fclose(cf->handle);
    }
    
    cf->isOpen = false;
    delete cf;
}

// Read chunk from file
size_t ChunkedRead(ChunkedFile* cf, void* buffer, size_t offset, size_t size) {
    if (!cf || !cf->isOpen) return 0;
    
    // Clamp to file size
    if (offset >= cf->fileSize) return 0;
    size_t readSize = std::min(size, cf->fileSize - offset);
    
#ifdef _WIN32
    if (cf->useMemoryMap && cf->mappedView) {
        // Memory-mapped read (fastest for large files)
        std::memcpy(buffer, (char*)cf->mappedView + offset, readSize);
        return readSize;
    }
#endif
    
    // Standard FILE I/O with seek
    std::fseek(cf->handle, (long)offset, SEEK_SET);
    return std::fread(buffer, 1, readSize, cf->handle);
}

// Read entire file in chunks (handles >2GB files)
char* ChunkedReadAll(const char* path, size_t* outSize) {
    ChunkedFile* cf = ChunkedOpen(path, true);
    if (!cf) {
        if (outSize) *outSize = 0;
        return nullptr;
    }
    
    // Allocate buffer
    char* buffer = (char*)std::malloc(cf->fileSize + 1);
    if (!buffer) {
        ChunkedClose(cf);
        if (outSize) *outSize = 0;
        return nullptr;
    }
    
    // Read in chunks
    size_t totalRead = 0;
    size_t remaining = cf->fileSize;
    char* ptr = buffer;
    
    while (remaining > 0) {
        size_t toRead = std::min(remaining, CHUNK_SIZE);
        size_t read = ChunkedRead(cf, ptr, totalRead, toRead);
        if (read == 0) break;
        
        totalRead += read;
        ptr += read;
        remaining -= read;
    }
    
    buffer[totalRead] = '\0';
    if (outSize) *outSize = totalRead;
    
    ChunkedClose(cf);
    return buffer;
}

// Write file in chunks (handles >2GB files)
bool ChunkedWrite(const char* path, const char* data, size_t size) {
    FILE* f = std::fopen(path, "wb");
    if (!f) return false;
    
    const char* ptr = data;
    size_t remaining = size;
    
    while (remaining > 0) {
        size_t toWrite = std::min(remaining, CHUNK_SIZE);
        size_t written = std::fwrite(ptr, 1, toWrite, f);
        if (written != toWrite) {
            std::fclose(f);
            return false;
        }
        ptr += written;
        remaining -= written;
    }
    
    std::fclose(f);
    return true;
}

// Copy file in chunks (handles >2GB files)
bool ChunkedCopy(const char* srcPath, const char* dstPath) {
    ChunkedFile* src = ChunkedOpen(srcPath, true);
    if (!src) return false;
    
    FILE* dst = std::fopen(dstPath, "wb");
    if (!dst) {
        ChunkedClose(src);
        return false;
    }
    
    char* buffer = (char*)std::malloc(CHUNK_SIZE);
    if (!buffer) {
        std::fclose(dst);
        ChunkedClose(src);
        return false;
    }
    
    size_t totalRead = 0;
    bool success = true;
    
    while (totalRead < src->fileSize) {
        size_t toRead = std::min(src->fileSize - totalRead, CHUNK_SIZE);
        size_t read = ChunkedRead(src, buffer, totalRead, toRead);
        if (read == 0) {
            success = false;
            break;
        }
        
        size_t written = std::fwrite(buffer, 1, read, dst);
        if (written != read) {
            success = false;
            break;
        }
        
        totalRead += read;
    }
    
    std::free(buffer);
    std::fclose(dst);
    ChunkedClose(src);
    
    return success;
}

// Get file size without reading
size_t ChunkedGetSize(const char* path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExA(path, GetFileExInfoStandard, &info)) {
        LARGE_INTEGER size;
        size.HighPart = info.nFileSizeHigh;
        size.LowPart = info.nFileSizeLow;
        return (size_t)size.QuadPart;
    }
#else
    struct stat st;
    if (stat(path, &st) == 0) {
        return (size_t)st.st_size;
    }
#endif
    return 0;
}

// Check if file is larger than threshold
bool ChunkedIsLargeFile(const char* path, size_t thresholdMB = 100) {
    size_t size = ChunkedGetSize(path);
    return size > (thresholdMB * 1024 * 1024);
}

} // namespace ChunkedIO

// ============================================================================
// PERFORMANCE TOOLS (15+ tools)
// ============================================================================

static ToolResult Tool_PerfProfile(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto exeIt = params.find("executable");
    auto argsIt = params.find("args");
    auto durationIt = params.find("duration");
    
    if (exeIt == params.end()) {
        result.error = "Missing parameter: executable";
        result.statusCode = 400;
        return result;
    }
    
    std::string duration = durationIt != params.end() ? durationIt->second : "10";
    std::string cmd;
    
#ifdef __linux__
    cmd = "perf record -g -F 99 -- sleep " + duration + " && perf report";
#else
    cmd = exeIt->second;
    if (argsIt != params.end()) cmd += " " + argsIt->second;
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfBenchmark(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto exeIt = params.find("executable");
    auto iterationsIt = params.find("iterations");
    
    if (exeIt == params.end()) {
        result.error = "Missing parameter: executable";
        result.statusCode = 400;
        return result;
    }
    
    int iterations = iterationsIt != params.end() ? std::stoi(iterationsIt->second) : 10;
    
    std::string cmd = exeIt->second;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        char* output = nullptr;
        Platform::Execute(cmd.c_str(), &output);
        if (output) Platform::Free(output);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    double avgMs = (double)duration / iterations / 1000.0;
    
    result.success = true;
    result.output = "Average execution time: " + std::to_string(avgMs) + " ms over " + 
                    std::to_string(iterations) + " iterations";
    result.metadata["avg_ms"] = std::to_string(avgMs);
    result.metadata["iterations"] = std::to_string(iterations);
    return result;
}

static ToolResult Tool_PerfMemory(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pidIt = params.find("pid");
    
#ifdef _WIN32
    std::string cmd = "tasklist /FI \"PID eq " + 
                      (pidIt != params.end() ? pidIt->second : "0") + "\" /FO CSV";
#else
    std::string cmd = "ps -p " + (pidIt != params.end() ? pidIt->second : "1") + 
                      " -o pid,rss,vsz,pmem,comm";
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfCPU(const std::map<std::string, std::string>& params) {
    (void)params;
    
    ToolResult result;
    
#ifdef _WIN32
    char* output = nullptr;
    int rc = Platform::Execute("wmic cpu get loadpercentage", &output);
#else
    char* output = nullptr;
    int rc = Platform::Execute("top -bn1 | head -5", &output);
#endif
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfDiskIO(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    
#ifdef _WIN32
    std::string cmd = "wmic logicaldisk where \"DeviceID='" + path + "'\" get Size,FreeSpace";
#else
    std::string cmd = "df -h " + path;
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfNetwork(const std::map<std::string, std::string>& params) {
    (void)params;
    
    ToolResult result;
    
#ifdef _WIN32
    char* output = nullptr;
    int rc = Platform::Execute("netstat -e", &output);
#else
    char* output = nullptr;
    int rc = Platform::Execute("netstat -i", &output);
#endif
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfGPUTemp(const std::map<std::string, std::string>& params) {
    (void)params;
    
    ToolResult result;
    
#ifdef _WIN32
    char* output = nullptr;
    int rc = Platform::Execute("nvidia-smi --query-gpu=temperature.gpu --format=csv", &output);
#else
    char* output = nullptr;
    int rc = Platform::Execute("nvidia-smi --query-gpu=temperature.gpu --format=csv", &output);
#endif
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfGPUUtil(const std::map<std::string, std::string>& params) {
    (void)params;
    
    ToolResult result;
    
    char* output = nullptr;
    int rc = Platform::Execute("nvidia-smi --query-gpu=utilization.gpu,memory.used,memory.total --format=csv", &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfCache(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto actionIt = params.find("action");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string action = actionIt != params.end() ? actionIt->second : "clear";
    
#ifdef _WIN32
    std::string cmd;
    if (action == "clear") {
        cmd = "powershell -Command \"Clear-DnsClientCache\"";
    } else {
        cmd = "powershell -Command \"Get-DnsClientCache\"";
    }
#else
    std::string cmd;
    if (action == "clear") {
        cmd = "sync && echo 3 | sudo tee /proc/sys/vm/drop_caches";
    } else {
        cmd = "free -h";
    }
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfThreads(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pidIt = params.find("pid");
    
#ifdef _WIN32
    std::string cmd = "powershell -Command \"Get-Process " + 
                      (pidIt != params.end() ? "-Id " + pidIt->second : "") + 
                      " | Select-Object Id,ProcessName,Threads\"";
#else
    std::string cmd = "ps -H -p " + (pidIt != params.end() ? pidIt->second : "1");
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfLatency(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto hostIt = params.find("host");
    auto countIt = params.find("count");
    
    if (hostIt == params.end()) {
        result.error = "Missing parameter: host";
        result.statusCode = 400;
        return result;
    }
    
    int count = countIt != params.end() ? std::stoi(countIt->second) : 4;
    
    std::string cmd = "ping -c " + std::to_string(count) + " " + hostIt->second;
    
    auto start = std::chrono::high_resolution_clock::now();
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    result.metadata["total_ms"] = std::to_string(duration);
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfThroughput(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    auto sizeIt = params.find("size");
    
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing parameters: src and dst required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = sizeIt != params.end() ? std::stoull(sizeIt->second) : 1024 * 1024;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // Use chunked copy for throughput test
    bool success = ChunkedIO::ChunkedCopy(srcIt->second.c_str(), dstIt->second.c_str());
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (double)size / ((double)duration / 1000.0) / (1024.0 * 1024.0);
    
    result.success = success;
    result.output = "Throughput: " + std::to_string(throughput) + " MB/s";
    result.metadata["throughput_mbps"] = std::to_string(throughput);
    result.metadata["size_bytes"] = std::to_string(size);
    result.metadata["duration_ms"] = std::to_string(duration);
    return result;
}

static ToolResult Tool_PerfTrace(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto exeIt = params.find("executable");
    auto durationIt = params.find("duration");
    
    if (exeIt == params.end()) {
        result.error = "Missing parameter: executable";
        result.statusCode = 400;
        return result;
    }
    
    std::string duration = durationIt != params.end() ? durationIt->second : "5";
    
#ifdef __linux__
    std::string cmd = "strace -c " + exeIt->second + " 2>&1";
#else
    std::string cmd = exeIt->second;
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_PerfFlame(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto exeIt = params.find("executable");
    auto durationIt = params.find("duration");
    
    if (exeIt == params.end()) {
        result.error = "Missing parameter: executable";
        result.statusCode = 400;
        return result;
    }
    
    std::string duration = durationIt != params.end() ? durationIt->second : "30";
    
#ifdef __linux__
    std::string cmd = "perf record -g " + exeIt->second + " && perf script | stackcollapse-perf.pl | flamegraph.pl > flame.svg";
#else
    std::string cmd = exeIt->second;
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// DEPLOYMENT TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_DeployDocker(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto imageIt = params.find("image");
    auto nameIt = params.find("name");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "docker " + actionIt->second;
    
    if (actionIt->second == "run" && imageIt != params.end()) {
        cmd += " " + imageIt->second;
        if (nameIt != params.end()) cmd += " --name " + nameIt->second;
    } else if (actionIt->second == "build" && imageIt != params.end()) {
        cmd += " -t " + imageIt->second + " .";
    } else if (actionIt->second == "push" && imageIt != params.end()) {
        cmd += " " + imageIt->second;
    } else if (actionIt->second == "pull" && imageIt != params.end()) {
        cmd += " " + imageIt->second;
    } else if (actionIt->second == "stop" && nameIt != params.end()) {
        cmd += " " + nameIt->second;
    } else if (actionIt->second == "rm" && nameIt != params.end()) {
        cmd += " " + nameIt->second;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployK8s(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto fileIt = params.find("file");
    auto nsIt = params.find("namespace");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "kubectl " + actionIt->second;
    
    if (nsIt != params.end()) cmd += " -n " + nsIt->second;
    if (fileIt != params.end()) cmd += " -f " + fileIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployHelm(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto chartIt = params.find("chart");
    auto nameIt = params.find("name");
    auto nsIt = params.find("namespace");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "helm " + actionIt->second;
    
    if (actionIt->second == "install" || actionIt->second == "upgrade") {
        if (nameIt != params.end()) cmd += " " + nameIt->second;
        if (chartIt != params.end()) cmd += " " + chartIt->second;
        if (nsIt != params.end()) cmd += " -n " + nsIt->second;
    } else if (actionIt->second == "uninstall") {
        if (nameIt != params.end()) cmd += " " + nameIt->second;
        if (nsIt != params.end()) cmd += " -n " + nsIt->second;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployTerraform(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto pathIt = params.find("path");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "cd \"" + path + "\" && terraform " + actionIt->second;
    
    if (actionIt->second == "apply" || actionIt->second == "destroy") {
        cmd += " -auto-approve";
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployAnsible(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto playbookIt = params.find("playbook");
    auto inventoryIt = params.find("inventory");
    
    if (playbookIt == params.end()) {
        result.error = "Missing parameter: playbook";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "ansible-playbook " + playbookIt->second;
    if (inventoryIt != params.end()) cmd += " -i " + inventoryIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeploySSH(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto hostIt = params.find("host");
    auto cmdIt = params.find("command");
    auto userIt = params.find("user");
    
    if (hostIt == params.end() || cmdIt == params.end()) {
        result.error = "Missing parameters: host and command required";
        result.statusCode = 400;
        return result;
    }
    
    std::string user = userIt != params.end() ? userIt->second : "root";
    std::string cmd = "ssh " + user + "@" + hostIt->second + " \"" + cmdIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeploySCP(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    auto hostIt = params.find("host");
    auto userIt = params.find("user");
    
    if (srcIt == params.end() || dstIt == params.end() || hostIt == params.end()) {
        result.error = "Missing parameters: src, dst, and host required";
        result.statusCode = 400;
        return result;
    }
    
    std::string user = userIt != params.end() ? userIt->second : "root";
    std::string cmd = "scp " + srcIt->second + " " + user + "@" + hostIt->second + ":" + dstIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployNginx(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto configIt = params.find("config");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd;
    if (actionIt->second == "reload") {
        cmd = "nginx -s reload";
    } else if (actionIt->second == "test") {
        cmd = "nginx -t";
    } else if (actionIt->second == "start") {
        cmd = "nginx";
    } else if (actionIt->second == "stop") {
        cmd = "nginx -s stop";
    } else if (actionIt->second == "config" && configIt != params.end()) {
        cmd = "nginx -t -c " + configIt->second;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeploySystemd(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto serviceIt = params.find("service");
    
    if (actionIt == params.end() || serviceIt == params.end()) {
        result.error = "Missing parameters: action and service required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "systemctl " + actionIt->second + " " + serviceIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_DeployCompose(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto actionIt = params.find("action");
    auto fileIt = params.find("file");
    
    if (actionIt == params.end()) {
        result.error = "Missing parameter: action";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "docker-compose";
    if (fileIt != params.end()) cmd += " -f " + fileIt->second;
    cmd += " " + actionIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// CONFIGURATION TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_ConfigGet(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto keyIt = params.find("key");
    
    if (fileIt == params.end() || keyIt == params.end()) {
        result.error = "Missing parameters: file and key required";
        result.statusCode = 400;
        return result;
    }
    
    // Use chunked I/O for large config files
    size_t size = 0;
    char* data = ChunkedIO::ChunkedReadAll(fileIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read config file: " + fileIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    // Simple key=value parsing
    std::string key = keyIt->second + "=";
    size_t pos = content.find(key);
    if (pos == std::string::npos) {
        // Try key: value format
        key = keyIt->second + ":";
        pos = content.find(key);
    }
    
    if (pos != std::string::npos) {
        size_t start = pos + key.length();
        size_t end = content.find('\n', start);
        if (end == std::string::npos) end = content.length();
        
        std::string value = content.substr(start, end - start);
        // Trim whitespace
        size_t first = value.find_first_not_of(" \t\r");
        size_t last = value.find_last_not_of(" \t\r");
        if (first != std::string::npos && last != std::string::npos) {
            value = value.substr(first, last - first + 1);
        }
        
        result.success = true;
        result.output = value;
        return result;
    }
    
    result.error = "Key not found: " + keyIt->second;
    result.statusCode = 404;
    return result;
}

static ToolResult Tool_ConfigSet(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto keyIt = params.find("key");
    auto valueIt = params.find("value");
    
    if (fileIt == params.end() || keyIt == params.end() || valueIt == params.end()) {
        result.error = "Missing parameters: file, key, and value required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = ChunkedIO::ChunkedReadAll(fileIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read config file: " + fileIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    // Find and replace key
    std::string key = keyIt->second + "=";
    size_t pos = content.find(key);
    if (pos == std::string::npos) {
        key = keyIt->second + ":";
        pos = content.find(key);
    }
    
    if (pos != std::string::npos) {
        size_t start = pos + key.length();
        size_t end = content.find('\n', start);
        if (end == std::string::npos) end = content.length();
        
        content.replace(start, end - start, valueIt->second);
    } else {
        // Append new key
        content += "\n" + keyIt->second + "=" + valueIt->second;
    }
    
    if (!ChunkedIO::ChunkedWrite(fileIt->second.c_str(), content.c_str(), content.size())) {
        result.error = "Failed to write config file: " + fileIt->second;
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Configuration updated";
    return result;
}

static ToolResult Tool_ConfigList(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = ChunkedIO::ChunkedReadAll(fileIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read config file: " + fileIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    // Parse all key=value pairs
    std::string output;
    std::istringstream stream(content);
    std::string line;
    
    while (std::getline(stream, line)) {
        // Skip comments and empty lines
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        
        size_t eqPos = line.find('=');
        if (eqPos == std::string::npos) {
            eqPos = line.find(':');
        }
        
        if (eqPos != std::string::npos) {
            output += line + "\n";
        }
    }
    
    result.success = true;
    result.output = output;
    return result;
}

static ToolResult Tool_ConfigValidate(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto typeIt = params.find("type");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string type = typeIt != params.end() ? typeIt->second : "json";
    std::string cmd;
    
    if (type == "json") {
        cmd = "python -m json.tool \"" + fileIt->second + "\" > /dev/null";
    } else if (type == "yaml") {
        cmd = "python -c \"import yaml; yaml.safe_load(open('" + fileIt->second + "'))\"";
    } else if (type == "toml") {
        cmd = "python -c \"import toml; toml.load('" + fileIt->second + "')\"";
    } else if (type == "xml") {
        cmd = "xmllint --noout \"" + fileIt->second + "\"";
    } else {
        result.error = "Unknown config type: " + type;
        result.statusCode = 400;
        return result;
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = (rc == 0) ? "Configuration is valid" : "Configuration is invalid";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_ConfigMerge(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto file1It = params.find("file1");
    auto file2It = params.find("file2");
    auto outputIt = params.find("output");
    
    if (file1It == params.end() || file2It == params.end() || outputIt == params.end()) {
        result.error = "Missing parameters: file1, file2, and output required";
        result.statusCode = 400;
        return result;
    }
    
    // Read both files
    size_t size1 = 0, size2 = 0;
    char* data1 = ChunkedIO::ChunkedReadAll(file1It->second.c_str(), &size1);
    char* data2 = ChunkedIO::ChunkedReadAll(file2It->second.c_str(), &size2);
    
    if (!data1 || !data2) {
        if (data1) Platform::Free(data1);
        if (data2) Platform::Free(data2);
        result.error = "Failed to read config files";
        result.statusCode = 404;
        return result;
    }
    
    std::string content1(data1, size1);
    std::string content2(data2, size2);
    Platform::Free(data1);
    Platform::Free(data2);
    
    // Simple merge (append file2 to file1)
    std::string merged = content1 + "\n" + content2;
    
    if (!ChunkedIO::ChunkedWrite(outputIt->second.c_str(), merged.c_str(), merged.size())) {
        result.error = "Failed to write merged config";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Configurations merged successfully";
    return result;
}

static ToolResult Tool_ConfigDiff(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto file1It = params.find("file1");
    auto file2It = params.find("file2");
    
    if (file1It == params.end() || file2It == params.end()) {
        result.error = "Missing parameters: file1 and file2 required";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "diff \"" + file1It->second + "\" \"" + file2It->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "Files are identical";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_ConfigTemplate(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto templateIt = params.find("template");
    auto outputIt = params.find("output");
    auto varsIt = params.find("vars");
    
    if (templateIt == params.end() || outputIt == params.end()) {
        result.error = "Missing parameters: template and output required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = ChunkedIO::ChunkedReadAll(templateIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read template file";
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    // Simple variable substitution
    if (varsIt != params.end()) {
        // Parse vars as key=value,key=value
        std::istringstream stream(varsIt->second);
        std::string pair;
        while (std::getline(stream, pair, ',')) {
            size_t eqPos = pair.find('=');
            if (eqPos != std::string::npos) {
                std::string key = "${" + pair.substr(0, eqPos) + "}";
                std::string value = pair.substr(eqPos + 1);
                
                size_t pos = 0;
                while ((pos = content.find(key, pos)) != std::string::npos) {
                    content.replace(pos, key.length(), value);
                    pos += value.length();
                }
            }
        }
    }
    
    if (!ChunkedIO::ChunkedWrite(outputIt->second.c_str(), content.c_str(), content.size())) {
        result.error = "Failed to write output file";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Template processed successfully";
    return result;
}

static ToolResult Tool_ConfigBackup(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    // Generate backup filename with timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << fileIt->second << ".backup." << std::put_time(std::localtime(&timestamp), "%Y%m%d_%H%M%S");
    std::string backupPath = ss.str();
    
    if (!ChunkedIO::ChunkedCopy(fileIt->second.c_str(), backupPath.c_str())) {
        result.error = "Failed to create backup";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Backup created: " + backupPath;
    result.metadata["backup_path"] = backupPath;
    return result;
}

static ToolResult Tool_ConfigRestore(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto backupIt = params.find("backup");
    auto targetIt = params.find("target");
    
    if (backupIt == params.end() || targetIt == params.end()) {
        result.error = "Missing parameters: backup and target required";
        result.statusCode = 400;
        return result;
    }
    
    if (!ChunkedIO::ChunkedCopy(backupIt->second.c_str(), targetIt->second.c_str())) {
        result.error = "Failed to restore backup";
        result.statusCode = 500;
        return result;
    }
    
    result.success = true;
    result.output = "Configuration restored successfully";
    return result;
}

// ============================================================================
// SEARCH TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_SearchGrep(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    auto recursiveIt = params.find("recursive");
    
    if (patternIt == params.end()) {
        result.error = "Missing parameter: pattern";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    bool recursive = recursiveIt != params.end() && recursiveIt->second == "true";
    
    std::string cmd = "grep -n";
    if (recursive) cmd += " -r";
    cmd += " \"" + patternIt->second + "\" \"" + path + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "No matches found";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchFind(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto nameIt = params.find("name");
    auto pathIt = params.find("path");
    auto typeIt = params.find("type");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    
    std::string cmd = "find \"" + path + "\"";
    if (nameIt != params.end()) cmd += " -name \"" + nameIt->second + "\"";
    if (typeIt != params.end()) {
        if (typeIt->second == "f") cmd += " -type f";
        else if (typeIt->second == "d") cmd += " -type d";
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchRegex(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    
    if (patternIt == params.end()) {
        result.error = "Missing parameter: pattern";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "grep -E -r \"" + patternIt->second + "\" \"" + path + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "No matches found";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchReplace(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto replacementIt = params.find("replacement");
    auto pathIt = params.find("path");
    
    if (patternIt == params.end() || replacementIt == params.end()) {
        result.error = "Missing parameters: pattern and replacement required";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "find \"" + path + "\" -type f -exec sed -i 's/" + 
                      patternIt->second + "/" + replacementIt->second + "/g' {} +";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "Search and replace completed";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchSymbols(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto symbolIt = params.find("symbol");
    auto pathIt = params.find("path");
    
    if (symbolIt == params.end()) {
        result.error = "Missing parameter: symbol";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "ctags -R --c-types=f -x \"" + path + "\" | grep \"" + symbolIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "No symbols found";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchFiles(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string pattern = patternIt != params.end() ? patternIt->second : "*";
    
#ifdef _WIN32
    std::string cmd = "dir /s /b \"" + path + "\\" + pattern + "\"";
#else
    std::string cmd = "find \"" + path + "\" -name \"" + pattern + "\"";
#endif
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchHistory(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto patternIt = params.find("pattern");
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "cd \"" + path + "\" && git log --all --oneline --grep=\"" + 
                      (patternIt != params.end() ? patternIt->second : "") + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchBlame(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto lineIt = params.find("line");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "git blame \"" + fileIt->second + "\"";
    if (lineIt != params.end()) cmd += " -L " + lineIt->second + "," + lineIt->second;
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchTodos(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "grep -rn \"TODO\\|FIXME\\|HACK\\|XXX\" \"" + path + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "No TODOs found";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_SearchDeps(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto typeIt = params.find("type");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string type = typeIt != params.end() ? typeIt->second : "all";
    
    std::string cmd;
    if (type == "npm" || type == "all") {
        cmd = "cd \"" + path + "\" && npm list --depth=0 2>/dev/null";
    } else if (type == "pip" || type == "all") {
        cmd = "cd \"" + path + "\" && pip list 2>/dev/null";
    } else if (type == "cargo" || type == "all") {
        cmd = "cd \"" + path + "\" && cargo tree --depth=1 2>/dev/null";
    } else {
        cmd = "cd \"" + path + "\" && ls -la";
    }
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// REFACTORING TOOLS (10+ tools)
// ============================================================================

static ToolResult Tool_RefactorRename(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto oldIt = params.find("old");
    auto newIt = params.find("new");
    auto pathIt = params.find("path");
    
    if (oldIt == params.end() || newIt == params.end()) {
        result.error = "Missing parameters: old and new required";
        result.statusCode = 400;
        return result;
    }
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "find \"" + path + "\" -type f -exec sed -i 's/\\b" + 
                      oldIt->second + "\\b/" + newIt->second + "/g' {} +";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "Rename completed";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorExtract(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto startIt = params.find("start");
    auto endIt = params.find("end");
    auto nameIt = params.find("name");
    
    if (fileIt == params.end() || startIt == params.end() || endIt == params.end() || nameIt == params.end()) {
        result.error = "Missing parameters: file, start, end, and name required";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = 0;
    char* data = ChunkedIO::ChunkedReadAll(fileIt->second.c_str(), &size);
    if (!data) {
        result.error = "Failed to read file: " + fileIt->second;
        result.statusCode = 404;
        return result;
    }
    
    std::string content(data, size);
    Platform::Free(data);
    
    int start = std::stoi(startIt->second);
    int end = std::stoi(endIt->second);
    
    if (start < 0 || end > (int)content.size() || start >= end) {
        result.error = "Invalid start/end positions";
        result.statusCode = 400;
        return result;
    }
    
    std::string extracted = content.substr(start, end - start);
    
    result.success = true;
    result.output = "Extracted code:\n" + extracted;
    result.metadata["extracted"] = extracted;
    result.metadata["name"] = nameIt->second;
    return result;
}

static ToolResult Tool_RefactorInline(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto symbolIt = params.find("symbol");
    
    if (fileIt == params.end() || symbolIt == params.end()) {
        result.error = "Missing parameters: file and symbol required";
        result.statusCode = 400;
        return result;
    }
    
    // This would require semantic analysis - placeholder
    result.success = false;
    result.error = "Inline refactoring requires semantic analysis - not yet implemented";
    result.statusCode = 501;
    return result;
}

static ToolResult Tool_RefactorMove(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing parameters: src and dst required";
        result.statusCode = 400;
        return result;
    }
    
    if (!ChunkedIO::ChunkedCopy(srcIt->second.c_str(), dstIt->second.c_str())) {
        result.error = "Failed to move file";
        result.statusCode = 500;
        return result;
    }
    
    Platform::DeleteFile(srcIt->second.c_str());
    
    result.success = true;
    result.output = "File moved successfully";
    return result;
}

static ToolResult Tool_RefactorFormat(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    auto styleIt = params.find("style");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string style = styleIt != params.end() ? styleIt->second : "llvm";
    std::string cmd = "clang-format -style=" + style + " -i \"" + fileIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "File formatted";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorSort(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    std::string cmd = "sort -o \"" + fileIt->second + "\" \"" + fileIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "File sorted";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorImports(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto fileIt = params.find("file");
    
    if (fileIt == params.end()) {
        result.error = "Missing parameter: file";
        result.statusCode = 400;
        return result;
    }
    
    // Sort and deduplicate imports
    std::string cmd = "sed -i '/^import\\|^#include\\|^use\\|^require/s/^/SORTME/' \"" + 
                      fileIt->second + "\" && sort -u -o \"" + fileIt->second + "\" \"" + 
                      fileIt->second + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = (rc == 0);
    result.statusCode = rc;
    result.output = output ? output : "Imports sorted";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorDeadCode(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    std::string cmd = "cppdeadcode \"" + path + "\" 2>/dev/null || echo 'Dead code analysis requires cppdeadcode'";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "Dead code analysis complete";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorDuplication(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto minLinesIt = params.find("min_lines");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    int minLines = minLinesIt != params.end() ? std::stoi(minLinesIt->second) : 5;
    
    std::string cmd = "cpd --minimum-tokens " + std::to_string(minLines * 10) + 
                      " --files \"" + path + "\"/**/*.cpp 2>/dev/null || echo 'Duplication analysis requires PMD'";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "Duplication analysis complete";
    if (output) Platform::Free(output);
    return result;
}

static ToolResult Tool_RefactorComplexity(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto thresholdIt = params.find("threshold");
    
    std::string path = pathIt != params.end() ? pathIt->second : ".";
    int threshold = thresholdIt != params.end() ? std::stoi(thresholdIt->second) : 10;
    
    std::string cmd = "lizard --threshold " + std::to_string(threshold) + " \"" + path + "\"";
    
    char* output = nullptr;
    int rc = Platform::Execute(cmd.c_str(), &output);
    
    result.success = true;
    result.statusCode = rc;
    result.output = output ? output : "Complexity analysis complete";
    if (output) Platform::Free(output);
    return result;
}

// ============================================================================
// LARGE FILE TOOLS (P0 Blocker Fix)
// ============================================================================

static ToolResult Tool_FileReadLarge(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto offsetIt = params.find("offset");
    auto sizeIt = params.find("size");
    
    if (pathIt == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    size_t offset = offsetIt != params.end() ? std::stoull(offsetIt->second) : 0;
    size_t size = sizeIt != params.end() ? std::stoull(sizeIt->second) : 0;
    
    // Get file size first
    size_t fileSize = ChunkedIO::ChunkedGetSize(pathIt->second.c_str());
    if (fileSize == 0) {
        result.error = "File not found or empty: " + pathIt->second;
        result.statusCode = 404;
        return result;
    }
    
    // Default to reading entire file if size not specified
    if (size == 0) size = fileSize - offset;
    
    // Clamp to file size
    if (offset >= fileSize) {
        result.error = "Offset beyond file size";
        result.statusCode = 400;
        return result;
    }
    if (offset + size > fileSize) size = fileSize - offset;
    
    // Allocate buffer
    char* buffer = (char*)std::malloc(size + 1);
    if (!buffer) {
        result.error = "Failed to allocate memory";
        result.statusCode = 500;
        return result;
    }
    
    // Open and read chunk
    ChunkedIO::ChunkedFile* cf = ChunkedIO::ChunkedOpen(pathIt->second.c_str(), true);
    if (!cf) {
        std::free(buffer);
        result.error = "Failed to open file: " + pathIt->second;
        result.statusCode = 404;
        return result;
    }
    
    size_t read = ChunkedIO::ChunkedRead(cf, buffer, offset, size);
    buffer[read] = '\0';
    
    ChunkedIO::ChunkedClose(cf);
    
    result.success = true;
    result.output = std::string(buffer, read);
    result.metadata["bytes_read"] = std::to_string(read);
    result.metadata["file_size"] = std::to_string(fileSize);
    result.metadata["offset"] = std::to_string(offset);
    
    std::free(buffer);
    return result;
}

static ToolResult Tool_FileWriteLarge(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    auto contentIt = params.find("content");
    auto appendIt = params.find("append");
    
    if (pathIt == params.end() || contentIt == params.end()) {
        result.error = "Missing parameters: path and content required";
        result.statusCode = 400;
        return result;
    }
    
    bool append = appendIt != params.end() && appendIt->second == "true";
    
    // For large content, use chunked write
    const std::string& content = contentIt->second;
    
    if (append) {
        // Read existing content first
        size_t existingSize = 0;
        char* existing = ChunkedIO::ChunkedReadAll(pathIt->second.c_str(), &existingSize);
        
        if (existing) {
            std::string combined = std::string(existing, existingSize) + content;
            Platform::Free(existing);
            
            if (!ChunkedIO::ChunkedWrite(pathIt->second.c_str(), combined.c_str(), combined.size())) {
                result.error = "Failed to write file";
                result.statusCode = 500;
                return result;
            }
        } else {
            // File doesn't exist, just write
            if (!ChunkedIO::ChunkedWrite(pathIt->second.c_str(), content.c_str(), content.size())) {
                result.error = "Failed to write file";
                result.statusCode = 500;
                return result;
            }
        }
    } else {
        if (!ChunkedIO::ChunkedWrite(pathIt->second.c_str(), content.c_str(), content.size())) {
            result.error = "Failed to write file";
            result.statusCode = 500;
            return result;
        }
    }
    
    result.success = true;
    result.output = "File written successfully";
    result.metadata["bytes_written"] = std::to_string(content.size());
    return result;
}

static ToolResult Tool_FileCopyLarge(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto srcIt = params.find("src");
    auto dstIt = params.find("dst");
    
    if (srcIt == params.end() || dstIt == params.end()) {
        result.error = "Missing parameters: src and dst required";
        result.statusCode = 400;
        return result;
    }
    
    size_t fileSize = ChunkedIO::ChunkedGetSize(srcIt->second.c_str());
    
    auto start = std::chrono::high_resolution_clock::now();
    
    if (!ChunkedIO::ChunkedCopy(srcIt->second.c_str(), dstIt->second.c_str())) {
        result.error = "Failed to copy file";
        result.statusCode = 500;
        return result;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    double throughput = (double)fileSize / ((double)duration / 1000.0) / (1024.0 * 1024.0);
    
    result.success = true;
    result.output = "File copied successfully";
    result.metadata["bytes_copied"] = std::to_string(fileSize);
    result.metadata["duration_ms"] = std::to_string(duration);
    result.metadata["throughput_mbps"] = std::to_string(throughput);
    return result;
}

static ToolResult Tool_FileInfo(const std::map<std::string, std::string>& params) {
    ToolResult result;
    auto pathIt = params.find("path");
    
    if (pathIt == params.end()) {
        result.error = "Missing parameter: path";
        result.statusCode = 400;
        return result;
    }
    
    size_t size = ChunkedIO::ChunkedGetSize(pathIt->second.c_str());
    bool isLarge = ChunkedIO::ChunkedIsLargeFile(pathIt->second.c_str(), 100);
    
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA info;
    if (GetFileAttributesExA(pathIt->second.c_str(), GetFileExInfoStandard, &info)) {
        result.success = true;
        result.output = "File: " + pathIt->second + "\n";
        result.output += "Size: " + std::to_string(size) + " bytes\n";
        result.output += "Size (human): " + std::to_string(size / (1024.0 * 1024.0)) + " MB\n";
        result.output += "Large file: " + std::string(isLarge ? "yes" : "no") + "\n";
        result.output += "Readonly: " + std::string((info.dwFileAttributes & FILE_ATTRIBUTE_READONLY) ? "yes" : "no") + "\n";
        result.output += "Hidden: " + std::string((info.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? "yes" : "no") + "\n";
        result.output += "System: " + std::string((info.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? "yes" : "no") + "\n";
        result.metadata["size"] = std::to_string(size);
        result.metadata["is_large"] = isLarge ? "true" : "false";
    } else {
        result.error = "File not found: " + pathIt->second;
        result.statusCode = 404;
    }
#else
    struct stat st;
    if (stat(pathIt->second.c_str(), &st) == 0) {
        result.success = true;
        result.output = "File: " + pathIt->second + "\n";
        result.output += "Size: " + std::to_string(size) + " bytes\n";
        result.output += "Size (human): " + std::to_string(size / (1024.0 * 1024.0)) + " MB\n";
        result.output += "Large file: " + std::string(isLarge ? "yes" : "no") + "\n";
        result.output += "Permissions: " + std::to_string(st.st_mode & 0777) + "\n";
        result.metadata["size"] = std::to_string(size);
        result.metadata["is_large"] = isLarge ? "true" : "false";
    } else {
        result.error = "File not found: " + pathIt->second;
        result.statusCode = 404;
    }
#endif
    
    return result;
}

// ============================================================================
// Register Extended Tools
// ============================================================================

void RegisterExtendedTools() {
    auto& registry = ToolRegistry::Instance();
    
    // LARGE FILE TOOLS (P0 Blocker Fix)
    registry.RegisterTool({"file_read_large", "Read large file with chunked I/O (handles >2GB)", 
                          ToolCategory::File, {"path", "offset", "size"}, Tool_FileReadLarge});
    registry.RegisterTool({"file_write_large", "Write large file with chunked I/O (handles >2GB)", 
                          ToolCategory::File, {"path", "content", "append"}, Tool_FileWriteLarge});
    registry.RegisterTool({"file_copy_large", "Copy large file with chunked I/O (handles >2GB)", 
                          ToolCategory::File, {"src", "dst"}, Tool_FileCopyLarge});
    registry.RegisterTool({"file_info", "Get file information including size", 
                          ToolCategory::File, {"path"}, Tool_FileInfo});
    
    // PERFORMANCE TOOLS (15 tools)
    registry.RegisterTool({"perf_profile", "Profile executable performance", 
                          ToolCategory::Performance, {"executable", "args", "duration"}, Tool_PerfProfile});
    registry.RegisterTool({"perf_benchmark", "Run performance benchmark", 
                          ToolCategory::Performance, {"executable", "iterations"}, Tool_PerfBenchmark});
    registry.RegisterTool({"perf_memory", "Get memory usage", 
                          ToolCategory::Performance, {"pid"}, Tool_PerfMemory});
    registry.RegisterTool({"perf_cpu", "Get CPU usage", 
                          ToolCategory::Performance, {}, Tool_PerfCPU});
    registry.RegisterTool({"perf_disk_io", "Get disk I/O stats", 
                          ToolCategory::Performance, {"path"}, Tool_PerfDiskIO});
    registry.RegisterTool({"perf_network", "Get network stats", 
                          ToolCategory::Performance, {}, Tool_PerfNetwork});
    registry.RegisterTool({"perf_gpu_temp", "Get GPU temperature", 
                          ToolCategory::Performance, {}, Tool_PerfGPUTemp});
    registry.RegisterTool({"perf_gpu_util", "Get GPU utilization", 
                          ToolCategory::Performance, {}, Tool_PerfGPUUtil});
    registry.RegisterTool({"perf_cache", "Manage system cache", 
                          ToolCategory::Performance, {"path", "action"}, Tool_PerfCache});
    registry.RegisterTool({"perf_threads", "Get thread information", 
                          ToolCategory::Performance, {"pid"}, Tool_PerfThreads});
    registry.RegisterTool({"perf_latency", "Measure network latency", 
                          ToolCategory::Performance, {"host", "count"}, Tool_PerfLatency});
    registry.RegisterTool({"perf_throughput", "Measure file throughput", 
                          ToolCategory::Performance, {"src", "dst", "size"}, Tool_PerfThroughput});
    registry.RegisterTool({"perf_trace", "Trace system calls", 
                          ToolCategory::Performance, {"executable", "duration"}, Tool_PerfTrace});
    registry.RegisterTool({"perf_flame", "Generate flame graph", 
                          ToolCategory::Performance, {"executable", "duration"}, Tool_PerfFlame});
    
    // DEPLOYMENT TOOLS (10 tools)
    registry.RegisterTool({"deploy_docker", "Docker operations", 
                          ToolCategory::Deployment, {"action", "image", "name"}, Tool_DeployDocker});
    registry.RegisterTool({"deploy_k8s", "Kubernetes operations", 
                          ToolCategory::Deployment, {"action", "file", "namespace"}, Tool_DeployK8s});
    registry.RegisterTool({"deploy_helm", "Helm operations", 
                          ToolCategory::Deployment, {"action", "chart", "name", "namespace"}, Tool_DeployHelm});
    registry.RegisterTool({"deploy_terraform", "Terraform operations", 
                          ToolCategory::Deployment, {"action", "path"}, Tool_DeployTerraform});
    registry.RegisterTool({"deploy_ansible", "Run Ansible playbook", 
                          ToolCategory::Deployment, {"playbook", "inventory"}, Tool_DeployAnsible});
    registry.RegisterTool({"deploy_ssh", "Execute SSH command", 
                          ToolCategory::Deployment, {"host", "command", "user"}, Tool_DeploySSH});
    registry.RegisterTool({"deploy_scp", "Copy file via SCP", 
                          ToolCategory::Deployment, {"src", "dst", "host", "user"}, Tool_DeploySCP});
    registry.RegisterTool({"deploy_nginx", "Nginx operations", 
                          ToolCategory::Deployment, {"action", "config"}, Tool_DeployNginx});
    registry.RegisterTool({"deploy_systemd", "Systemd service operations", 
                          ToolCategory::Deployment, {"action", "service"}, Tool_DeploySystemd});
    registry.RegisterTool({"deploy_compose", "Docker Compose operations", 
                          ToolCategory::Deployment, {"action", "file"}, Tool_DeployCompose});
    
    // CONFIGURATION TOOLS (10 tools)
    registry.RegisterTool({"config_get", "Get configuration value", 
                          ToolCategory::Config, {"file", "key"}, Tool_ConfigGet});
    registry.RegisterTool({"config_set", "Set configuration value", 
                          ToolCategory::Config, {"file", "key", "value"}, Tool_ConfigSet});
    registry.RegisterTool({"config_list", "List all configuration values", 
                          ToolCategory::Config, {"file"}, Tool_ConfigList});
    registry.RegisterTool({"config_validate", "Validate configuration file", 
                          ToolCategory::Config, {"file", "type"}, Tool_ConfigValidate});
    registry.RegisterTool({"config_merge", "Merge configuration files", 
                          ToolCategory::Config, {"file1", "file2", "output"}, Tool_ConfigMerge});
    registry.RegisterTool({"config_diff", "Compare configuration files", 
                          ToolCategory::Config, {"file1", "file2"}, Tool_ConfigDiff});
    registry.RegisterTool({"config_template", "Process configuration template", 
                          ToolCategory::Config, {"template", "output", "vars"}, Tool_ConfigTemplate});
    registry.RegisterTool({"config_backup", "Backup configuration file", 
                          ToolCategory::Config, {"file"}, Tool_ConfigBackup});
    registry.RegisterTool({"config_restore", "Restore configuration from backup", 
                          ToolCategory::Config, {"backup", "target"}, Tool_ConfigRestore});
    
    // SEARCH TOOLS (10 tools)
    registry.RegisterTool({"search_grep", "Search with grep", 
                          ToolCategory::Search, {"pattern", "path", "recursive"}, Tool_SearchGrep});
    registry.RegisterTool({"search_find", "Find files", 
                          ToolCategory::Search, {"name", "path", "type"}, Tool_SearchFind});
    registry.RegisterTool({"search_regex", "Search with regex", 
                          ToolCategory::Search, {"pattern", "path"}, Tool_SearchRegex});
    registry.RegisterTool({"search_replace", "Search and replace in files", 
                          ToolCategory::Search, {"pattern", "replacement", "path"}, Tool_SearchReplace});
    registry.RegisterTool({"search_symbols", "Search for code symbols", 
                          ToolCategory::Search, {"symbol", "path"}, Tool_SearchSymbols});
    registry.RegisterTool({"search_files", "Search for files by pattern", 
                          ToolCategory::Search, {"pattern", "path"}, Tool_SearchFiles});
    registry.RegisterTool({"search_history", "Search git history", 
                          ToolCategory::Search, {"pattern", "path"}, Tool_SearchHistory});
    registry.RegisterTool({"search_blame", "Git blame for file", 
                          ToolCategory::Search, {"file", "line"}, Tool_SearchBlame});
    registry.RegisterTool({"search_todos", "Find TODOs and FIXMEs", 
                          ToolCategory::Search, {"path"}, Tool_SearchTodos});
    registry.RegisterTool({"search_deps", "List dependencies", 
                          ToolCategory::Search, {"path", "type"}, Tool_SearchDeps});
    
    // REFACTORING TOOLS (10 tools)
    registry.RegisterTool({"refactor_rename", "Rename symbol across project", 
                          ToolCategory::Refactor, {"old", "new", "path"}, Tool_RefactorRename});
    registry.RegisterTool({"refactor_extract", "Extract code to function", 
                          ToolCategory::Refactor, {"file", "start", "end", "name"}, Tool_RefactorExtract});
    registry.RegisterTool({"refactor_inline", "Inline function", 
                          ToolCategory::Refactor, {"file", "symbol"}, Tool_RefactorInline});
    registry.RegisterTool({"refactor_move", "Move file", 
                          ToolCategory::Refactor, {"src", "dst"}, Tool_RefactorMove});
    registry.RegisterTool({"refactor_format", "Format code", 
                          ToolCategory::Refactor, {"file", "style"}, Tool_RefactorFormat});
    registry.RegisterTool({"refactor_sort", "Sort file contents", 
                          ToolCategory::Refactor, {"file"}, Tool_RefactorSort});
    registry.RegisterTool({"refactor_imports", "Sort and deduplicate imports", 
                          ToolCategory::Refactor, {"file"}, Tool_RefactorImports});
    registry.RegisterTool({"refactor_dead_code", "Find dead code", 
                          ToolCategory::Refactor, {"path"}, Tool_RefactorDeadCode});
    registry.RegisterTool({"refactor_duplication", "Find code duplication", 
                          ToolCategory::Refactor, {"path", "min_lines"}, Tool_RefactorDuplication});
    registry.RegisterTool({"refactor_complexity", "Analyze code complexity", 
                          ToolCategory::Refactor, {"path", "threshold"}, Tool_RefactorComplexity});
}

} // namespace NativeIDE
} // namespace RawrXD
