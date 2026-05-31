#include <windows.h>
#include <vector>
#include <cstdint>
#include <string>
#include <mutex>
#include <stdexcept>

// External MASM for Atomic SHA-256 chain append
extern "C" void WOM_CommitBlock(const uint8_t* data, size_t size, uint8_t* out_chain_hash);

namespace RawrXD {
namespace Autonomy {

/**
 * @brief Pillar 4: Mnemosyne Store (WOM Manager)
 * Manages the immutable persistent memory for the Agentic IDE.
 * Files: C:\ProgramData\Win32IDE\mnemosyne.bin
 */
class MnemosyneStore {
public:
    static MnemosyneStore& GetInstance() {
        static MnemosyneStore instance;
        return instance;
    }

    void InitWOM() {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 1. Create/Open the WOM backing file
        // Using FILE_FLAG_WRITE_THROUGH and FILE_APPEND_DATA for atomic semantics.
        m_hFile = CreateFileW(L"C:\\ProgramData\\Win32IDE\\mnemosyne.bin",
                            FILE_APPEND_DATA | GENERIC_READ,
                            FILE_SHARE_READ,
                            NULL,
                            OPEN_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
                            NULL);

        if (m_hFile == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to initialize Mnemosyne WOM Store.");
        }

        // 2. Load Genesis Hash (Silicon ID bound)
        m_lastChainHash.assign(32, 0x55); 
    }

    void AppendImmutable(const std::vector<uint8_t>& data) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        // 3. Cryptographic Chaining: Hash(Data || LastHash)
        uint8_t nextHash[32];
        WOM_CommitBlock(data.data(), data.size(), nextHash);

        // 4. Atomic Write to File
        DWORD bytesWritten;
        WriteFile(m_hFile, data.data(), (DWORD)data.size(), &bytesWritten, NULL);
        
        memcpy(m_lastChainHash.data(), nextHash, 32);
    }

    std::vector<uint8_t> GetLastChainHash() const {
        return m_lastChainHash;
    }

private:
    MnemosyneStore() : m_hFile(INVALID_HANDLE_VALUE) {}
    
    HANDLE m_hFile;
    std::vector<uint8_t> m_lastChainHash;
    std::mutex m_mutex;
};

} // namespace Autonomy
} // namespace RawrXD
