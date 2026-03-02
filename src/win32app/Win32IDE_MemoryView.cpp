// ============================================================================
// Win32IDE_MemoryView.cpp — Real memory/hex viewer debugger panel
// ============================================================================
// Dedicated memory view (address → hex dump, follow pointer)
// Read memory via debug engine at current process
// Provides hex/ASCII/disassembly modes with navigation
// ============================================================================

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commctrl.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <iomanip>
#include <sstream>

namespace Raw rXD {
namespace IDE {

// ============================================================================
// Memory Display Mode
// ============================================================================

enum class MemoryDisplayMode {
    Hex,                // Hex dump with ASCII
    Disassembly,        // Disassembled instructions
    Unicode,            // Wide character view
    Float,              // Float/double values
    Pointer             // Pointer chain view
};

// ============================================================================
// Memory View Configuration
// ============================================================================

struct MemoryViewConfig {
    uint64_t baseAddress = 0;
    size_t bytesPerRow = 16;
    size_t rowCount = 32;
    MemoryDisplayMode mode = MemoryDisplayMode::Hex;
    bool showAscii = true;
    bool showAddress = true;
};

// ============================================================================
// Memory Viewer
// ============================================================================

class MemoryViewer {
private:
    std::mutex m_mutex;
    MemoryViewConfig m_config;
    std::vector<uint8_t> m_buffer;
    HANDLE m_debugProcess = NULL;
    bool m_initialized = false;
    
public:
    MemoryViewer() = default;
    ~MemoryViewer() = default;
    
    // Initialize viewer
    bool initialize(HANDLE debugProcess) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        m_debugProcess = debugProcess;
        m_config = MemoryViewConfig{};
        
        m_initialized = true;
        
        fprintf(stderr, "[MemoryViewer] Initialized\n");
        return true;
    }
    
    // Set display address
    void setAddress(uint64_t address) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.baseAddress = address;
        
        fprintf(stderr, "[MemoryViewer] Set base address: 0x%llX\n", address);
    }
    
    // Set display mode
    void setMode(MemoryDisplayMode mode) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config.mode = mode;
        
        fprintf(stderr, "[MemoryViewer] Set mode: %d\n", (int)mode);
    }
    
    // Read memory from process
    bool readMemory() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_initialized || !m_debugProcess) {
            fprintf(stderr, "[MemoryViewer] Not initialized or no process\n");
            return false;
        }
        
        size_t totalBytes = m_config.bytesPerRow * m_config.rowCount;
        m_buffer.resize(totalBytes);
        
#ifdef _WIN32
        SIZE_T bytesRead = 0;
        if (ReadProcessMemory(m_debugProcess, 
                             (LPCVOID)m_config.baseAddress,
                             m_buffer.data(),
                             totalBytes,
                             &bytesRead)) {
            
            fprintf(stderr, "[MemoryViewer] Read %zu bytes from 0x%llX\n",
                    (size_t)bytesRead, m_config.baseAddress);
            
            // Adjust buffer size if we read less
            if (bytesRead < totalBytes) {
                m_buffer.resize(bytesRead);
            }
            
            return true;
        } else {
            DWORD error = GetLastError();
            fprintf(stderr, "[MemoryViewer] ReadProcessMemory failed: %lu\n", error);
            
            // Try to read smaller chunks
            return readMemoryPartial();
        }
#else
        return false;
#endif
    }
    
    // Get formatted memory view
    std::string getFormattedView() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (m_buffer.empty()) {
            return "No memory data available. Read memory first.\n";
        }
        
        std::ostringstream ss;
        
        switch (m_config.mode) {
            case MemoryDisplayMode::Hex:
                return formatHexDump();
            case MemoryDisplayMode::Disassembly:
                return formatDisassembly();
            case MemoryDisplayMode::Unicode:
                return formatUnicode();
            case MemoryDisplayMode::Float:
                return formatFloat();
            case MemoryDisplayMode::Pointer:
                return formatPointers();
            default:
                return formatHexDump();
        }
    }
    
    // Navigate forward/backward
    void navigate(int64_t offset) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        uint64_t newAddress = m_config.baseAddress + offset;
        m_config.baseAddress = newAddress;
        
        fprintf(stderr, "[MemoryViewer] Navigated to: 0x%llX\n", newAddress);
    }
    
    // Follow pointer at address
    bool followPointer(uint64_t address) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        if (!m_debugProcess) {
            return false;
        }
        
#ifdef _WIN32
        uint64_t pointerValue = 0;
        SIZE_T bytesRead = 0;
        
        if (ReadProcessMemory(m_debugProcess,
                             (LPCVOID)address,
                             &pointerValue,
                             sizeof(pointerValue),
                             &bytesRead)) {
            
            m_config.baseAddress = pointerValue;
            
            fprintf(stderr, "[MemoryViewer] Followed pointer: 0x%llX -> 0x%llX\n",
                    address, pointerValue);
            
            return true;
        }
#endif
        
        return false;
    }
    
private:
    bool readMemoryPartial() {
#ifdef _WIN32
        // Try reading in 4KB chunks
        const size_t chunkSize = 4096;
        size_t totalBytes = m_config.bytesPerRow * m_config.rowCount;
        
        m_buffer.clear();
        m_buffer.reserve(totalBytes);
        
        uint64_t currentAddr = m_config.baseAddress;
        size_t remaining = totalBytes;
        
        while (remaining > 0) {
            size_t toRead = std::min(remaining, chunkSize);
            uint8_t chunk[4096];
            SIZE_T bytesRead = 0;
            
            if (ReadProcessMemory(m_debugProcess,
                                 (LPCVOID)currentAddr,
                                 chunk,
                                 toRead,
                                 &bytesRead)) {
                
                m_buffer.insert(m_buffer.end(), chunk, chunk + bytesRead);
                
                if (bytesRead < toRead) {
                    break; // Reached unreadable region
                }
                
                currentAddr += bytesRead;
                remaining -= bytesRead;
            } else {
                break;
            }
        }
        
        return !m_buffer.empty();
#else
        return false;
#endif
    }
    
    std::string formatHexDump() {
        std::ostringstream ss;
        
        size_t offset = 0;
        while (offset < m_buffer.size()) {
            // Address
            if (m_config.showAddress) {
                ss << std::hex << std::setw(16) << std::setfill('0')
                   << (m_config.baseAddress + offset) << "  ";
            }
            
            // Hex bytes
            size_t lineBytes = std::min(m_config.bytesPerRow, m_buffer.size() - offset);
            for (size_t i = 0; i < lineBytes; ++i) {
                ss << std::hex << std::setw(2) << std::setfill('0')
                   << (int)m_buffer[offset + i] << " ";
                
                if ((i + 1) % 8 == 0) {
                    ss << " ";
                }
            }
            
            // Padding
            for (size_t i = lineBytes; i < m_config.bytesPerRow; ++i) {
                ss << "   ";
                if ((i + 1) % 8 == 0) {
                    ss << " ";
                }
            }
            
            // ASCII
            if (m_config.showAscii) {
                ss << " |";
                for (size_t i = 0; i < lineBytes; ++i) {
                    uint8_t byte = m_buffer[offset + i];
                    char c = (byte >= 32 && byte < 127) ? (char)byte : '.';
                    ss << c;
                }
                ss << "|";
            }
            
            ss << "\n";
            offset += lineBytes;
        }
        
        return ss.str();
    }
    
    std::string formatDisassembly() {
        // Simplified disassembly (real impl would use Zydis/Capstone)
        std::ostringstream ss;
        
        ss << "; Disassembly at 0x" << std::hex << m_config.baseAddress << "\n";
        ss << "; (Full disassembly requires Zydis integration)\n\n";
        
        // Show as hex for now
        return formatHexDump();
    }
    
    std::string formatUnicode() {
        std::ostringstream ss;
        
        size_t offset = 0;
        while (offset + 1 < m_buffer.size()) {
            uint16_t wchar = *reinterpret_cast<uint16_t*>(&m_buffer[offset]);
            
            if (wchar >= 32 && wchar < 127) {
                ss << (char)wchar;
            } else if (wchar == 0) {
                ss << "\\0";
            } else {
                ss << ".";
            }
            
            offset += 2;
            
            if (offset % 32 == 0) {
                ss << "\n";
            }
        }
        
        return ss.str();
    }
    
    std::string formatFloat() {
        std::ostringstream ss;
        
        size_t offset = 0;
        int index = 0;
        
        while (offset + 3 < m_buffer.size()) {
            float f = *reinterpret_cast<float*>(&m_buffer[offset]);
            
            ss << std::dec << std::setw(4) << index << ": "
               << std::fixed << std::setprecision(6) << f << "\n";
            
            offset += 4;
            index++;
        }
        
        return ss.str();
    }
    
    std::string formatPointers() {
        std::ostringstream ss;
        
        size_t offset = 0;
        int index = 0;
        
        while (offset + 7 < m_buffer.size()) {
            uint64_t ptr = *reinterpret_cast<uint64_t*>(&m_buffer[offset]);
            
            ss << std::dec << std::setw(4) << index << ": 0x"
               << std::hex << std::setw(16) << std::setfill('0') << ptr;
            
            // Try to validate pointer (basic heuristic)
            if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                ss << " (valid?)";
            }
            
            ss << "\n";
            
            offset += 8;
            index++;
        }
        
        return ss.str();
    }
};

// ========================================================================== ==
// Global Instance
// ============================================================================

static std::unique_ptr<MemoryViewer> g_memoryViewer;
static std::mutex g_viewerMutex;

} // namespace IDE
} // namespace RawrXD

// ============================================================================
// C API
// ============================================================================

extern "C" {

bool RawrXD_IDE_InitMemoryViewer(void* debugProcess) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_viewerMutex);
    
    RawrXD::IDE::g_memoryViewer = std::make_unique<RawrXD::IDE::MemoryViewer>();
    return RawrXD::IDE::g_memoryViewer->initialize((HANDLE)debugProcess);
}

void RawrXD_IDE_SetMemoryAddress(uint64_t address) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_viewerMutex);
    
    if (!RawrXD::IDE::g_memoryViewer) {
        return;
    }
    
    RawrXD::IDE::g_memoryViewer->setAddress(address);
}

bool RawrXD_IDE_ReadMemory() {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_viewerMutex);
    
    if (!RawrXD::IDE::g_memoryViewer) {
        return false;
    }
    
    return RawrXD::IDE::g_memoryViewer->readMemory();
}

const char* RawrXD_IDE_GetMemoryView() {
    static thread_local char buf[65536];
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_viewerMutex);
    
    if (!RawrXD::IDE::g_memoryViewer) {
        return "Memory viewer not initialized";
    }
    
    std::string view = RawrXD::IDE::g_memoryViewer->getFormattedView();
    snprintf(buf, sizeof(buf), "%s", view.c_str());
    return buf;
}

bool RawrXD_IDE_FollowPointer(uint64_t address) {
    std::lock_guard<std::mutex> lock(RawrXD::IDE::g_viewerMutex);
    
    if (!RawrXD::IDE::g_memoryViewer) {
        return false;
    }
    
    return RawrXD::IDE::g_memoryViewer->followPointer(address);
}

} // extern "C"
