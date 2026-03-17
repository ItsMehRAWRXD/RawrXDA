// ============================================================================
// PE Writer Production API - Universal IDE Component
// Version: 2.0.0 Production
// Architecture: Modular, Extensible, Thread-Safe
// ============================================================================

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>

// Define PE structures and constants for cross-platform compatibility
// These are based on the Windows PE format specification

#define PEWRITER_API

// PE Constants (from Windows SDK)
static constexpr uint16_t IMAGE_DOS_SIGNATURE = 0x5A4D;
static constexpr uint32_t IMAGE_NT_SIGNATURE = 0x00004550;
static constexpr uint16_t IMAGE_NT_OPTIONAL_HDR64_MAGIC = 0x20B;
static constexpr uint16_t IMAGE_FILE_MACHINE_AMD64 = 0x8664;
static constexpr uint16_t IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002;
static constexpr uint16_t IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020;
static constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_CUI = 3;
static constexpr uint16_t IMAGE_SUBSYSTEM_WINDOWS_GUI = 2;
static constexpr uint16_t IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA = 0x0020;
static constexpr uint16_t IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE = 0x0040;
static constexpr uint16_t IMAGE_DLLCHARACTERISTICS_NX_COMPAT = 0x0100;
static constexpr uint16_t IMAGE_DLLCHARACTERISTICS_NO_SEH = 0x0400;
static constexpr uint16_t IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE = 0x8000;
static constexpr uint32_t IMAGE_SCN_CNT_CODE = 0x00000020;
static constexpr uint32_t IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040;
static constexpr uint32_t IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080;
static constexpr uint32_t IMAGE_SCN_MEM_EXECUTE = 0x20000000;
static constexpr uint32_t IMAGE_SCN_MEM_READ = 0x40000000;
static constexpr uint32_t IMAGE_SCN_MEM_WRITE = 0x80000000;

// PE Structure Definitions
#pragma pack(push, 1)

struct IMAGE_DOS_HEADER {
    uint16_t e_magic;
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    int32_t e_lfanew;
};

struct IMAGE_FILE_HEADER {
    uint16_t Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader;
    uint16_t Characteristics;
};

struct IMAGE_DATA_DIRECTORY {
    uint32_t VirtualAddress;
    uint32_t Size;
};

struct IMAGE_OPTIONAL_HEADER64 {
    uint16_t Magic;
    uint8_t MajorLinkerVersion;
    uint8_t MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    IMAGE_DATA_DIRECTORY DataDirectory[16];
};

struct IMAGE_NT_HEADERS64 {
    uint32_t Signature;
    IMAGE_FILE_HEADER FileHeader;
    IMAGE_OPTIONAL_HEADER64 OptionalHeader;
};

struct IMAGE_SECTION_HEADER {
    uint8_t Name[8];
    union {
        uint32_t PhysicalAddress;
        uint32_t VirtualSize;
    } Misc;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
};

struct IMAGE_IMPORT_DESCRIPTOR {
    union {
        uint32_t Characteristics;
        uint32_t OriginalFirstThunk;
    };
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t Name;
    uint32_t FirstThunk;
};

#pragma pack(pop)

namespace pewriter {

// ============================================================================
// FORWARD DECLARATIONS
// ============================================================================

class PEConfig;
class PEStructureBuilder;
class CodeEmitter;
class ImportResolver;
class RelocationManager;
class ResourceManager;
class PEValidator;
class ErrorHandler;

// ============================================================================
// ENUMS AND CONSTANTS
// ============================================================================

enum class PEArchitecture {
    x86,
    x64,
    ARM64
};

enum class PESubsystem {
    WINDOWS_GUI = 2,
    WINDOWS_CUI = 3,
    POSIX_CUI = 7,
    WINDOWS_CE_GUI = 9,
    EFI_APPLICATION = 10,
    EFI_BOOT_SERVICE_DRIVER = 11,
    EFI_RUNTIME_DRIVER = 12,
    EFI_ROM = 13,
    XBOX = 14
};

enum class PEErrorCode {
    SUCCESS = 0,
    INVALID_PARAMETER = 1,
    OUT_OF_MEMORY = 2,
    FILE_IO_ERROR = 3,
    INVALID_PE_STRUCTURE = 4,
    CODE_GENERATION_ERROR = 5,
    IMPORT_RESOLUTION_ERROR = 6,
    RELOCATION_ERROR = 7,
    VALIDATION_ERROR = 8,
    CONFIGURATION_ERROR = 9
};

// ============================================================================
// CONFIGURATION STRUCTURES
// ============================================================================

struct PEConfig {
    PEArchitecture architecture = PEArchitecture::x64;
    PESubsystem subsystem = PESubsystem::WINDOWS_CUI;
    uint64_t imageBase = 0x140000000ULL;
    uint32_t sectionAlignment = 0x1000;
    uint32_t fileAlignment = 0x200;
    uint32_t stackReserve = 0x100000;
    uint32_t stackCommit = 0x1000;
    uint32_t heapReserve = 0x100000;
    uint32_t heapCommit = 0x1000;
    std::string entryPointSymbol = "main";
    bool enableASLR = true;
    bool enableDEP = true;
    bool enableSEH = true;
    bool enableHighEntropyVA = true;
    std::vector<std::string> libraries;
    std::vector<std::string> symbols;
};

struct CodeSection {
    std::string name;
    std::vector<uint8_t> code;
    uint32_t characteristics;
    bool executable = true;
    bool readable = true;
    bool writable = false;
};

struct ImportEntry {
    std::string library;
    std::string symbol;
    uint32_t hint = 0;
};

struct RelocationEntry {
    uint32_t offset;
    uint32_t type;
    std::string symbol;
    int64_t addend = 0;
};

// ============================================================================
// MAIN PE WRITER CLASS
// ============================================================================

class PEWRITER_API PEWriter {
public:
    PEWriter();
    ~PEWriter();

    // Configuration
    bool configure(const PEConfig& config);
    bool loadConfigFromJSON(const std::string& jsonPath);
    bool loadConfigFromXML(const std::string& xmlPath);

    // Code and Data Management
    bool addCodeSection(const CodeSection& section);
    bool addDataSection(const std::string& name, const std::vector<uint8_t>& data);
    bool addImport(const ImportEntry& import);
    bool addRelocation(const RelocationEntry& relocation);

    // Resource Management
    bool addResource(int type, int id, const std::vector<uint8_t>& data);
    bool addVersionInfo(const std::unordered_map<std::string, std::string>& info);

    // Build and Validation
    bool build();
    bool validate() const;
    bool writeToFile(const std::string& filename);

    // Advanced Features
    bool enableDebugging();
    bool addTLS();
    bool addExceptionHandling();

    // Error Handling
    PEErrorCode getLastError() const;
    std::string getErrorMessage() const;
    void clearError();

    // Callbacks for IDE Integration
    using ProgressCallback = std::function<void(int percentage, const std::string& message)>;
    using ErrorCallback = std::function<void(PEErrorCode code, const std::string& message)>;

    void setProgressCallback(ProgressCallback callback);
    void setErrorCallback(ErrorCallback callback);

private:
    std::unique_ptr<PEStructureBuilder> structureBuilder_;
    std::unique_ptr<CodeEmitter> codeEmitter_;
    std::unique_ptr<ImportResolver> importResolver_;
    std::unique_ptr<RelocationManager> relocationManager_;
    std::unique_ptr<ResourceManager> resourceManager_;
    std::unique_ptr<PEValidator> validator_;
    std::unique_ptr<ErrorHandler> errorHandler_;

    PEConfig config_;
    std::vector<uint8_t> peImage_;
    bool isBuilt_ = false;
};

// ============================================================================
// IDE INTEGRATION CLASSES
// ============================================================================

class PEWRITER_API IDEBridge {
public:
    IDEBridge();
    ~IDEBridge();

    // VS Code Extension Interface
    bool registerCommands();
    bool handleCommand(const std::string& command, const std::vector<std::string>& args);

    // Language Server Protocol
    bool initializeLSP();
    bool processLSPMessage(const std::string& message);

    // REST API
    bool startRESTServer(uint16_t port);
    void stopRESTServer();

    // Thread Safety
    void lock();
    void unlock();

private:
    std::unique_ptr<PEWriter> writer_;
    std::mutex mutex_;

    // Command dispatch table
    std::unordered_map<std::string, std::function<bool(const std::vector<std::string>&)>> commandDispatch_;

    // LSP state
    bool lspInitialized_;
    std::vector<std::string> lspResponseQueue_;

    // REST server state
    bool restRunning_;
    uint16_t restPort_;
    std::thread restThread_;

    #ifdef _WIN32
    void* hCommandPipe_;  // HANDLE
    void* hLSPPipe_;      // HANDLE
    uintptr_t restSocket_;  // SOCKET
    #endif
};

// ============================================================================
// UTILITY FUNCTIONS
// ============================================================================

PEWRITER_API std::string getVersion();
PEWRITER_API bool isSupportedArchitecture(PEArchitecture arch);
PEWRITER_API std::vector<std::string> getSupportedFeatures();

// ============================================================================
// EXCEPTION CLASSES
// ============================================================================

class PEException : public std::exception {
public:
    PEException(PEErrorCode code, const std::string& message);
    PEErrorCode code() const;
    const char* what() const noexcept override;

private:
    PEErrorCode code_;
    std::string message_;
};

} // namespace pewriter