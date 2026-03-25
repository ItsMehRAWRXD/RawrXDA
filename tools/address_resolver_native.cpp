#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    std::wstring modulePath;
    std::optional<std::wstring> mapPath;
    std::optional<std::wstring> symbolPath;
    bool skipPdb = false;
    bool includeNtSymbolPath = false;
    std::vector<uint64_t> addresses;
};

struct ImageRange {
    uint64_t imageBase = 0;
    uint64_t imageSize = 0;
    uint64_t imageEnd = 0;
};

struct MapEntry {
    uint32_t segment = 0;
    uint64_t rva = 0;
    uint64_t nextRva = UINT64_MAX;
    std::string symbol;
    std::string object;
};

struct MapResolved {
    MapEntry entry;
    uint64_t displacement = 0;
    bool contained = false;
};

std::wstring ToWide(const std::string& value) {
    if (value.empty()) {
        return L"";
    }

    int size = MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0);
    if (size <= 0) {
        return L"";
    }

    std::wstring out(static_cast<size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size);
    return out;
}

std::string ToUtf8(const std::wstring& value) {
    if (value.empty()) {
        return "";
    }

    int size = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), nullptr, 0, nullptr, nullptr);
    if (size <= 0) {
        return "";
    }

    std::string out(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()), out.data(), size, nullptr, nullptr);
    return out;
}

std::wstring Trim(const std::wstring& input) {
    size_t first = 0;
    while (first < input.size() && iswspace(input[first])) {
        ++first;
    }

    size_t last = input.size();
    while (last > first && iswspace(input[last - 1])) {
        --last;
    }

    return input.substr(first, last - first);
}

std::vector<std::wstring> Split(const std::wstring& text, wchar_t delimiter) {
    std::vector<std::wstring> parts;
    std::wstring current;
    for (wchar_t ch : text) {
        if (ch == delimiter) {
            parts.push_back(current);
            current.clear();
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);
    return parts;
}

std::optional<uint64_t> ParseHexU64(std::wstring text) {
    text = Trim(text);
    if (text.empty()) {
        return std::nullopt;
    }

    errno = 0;
    wchar_t* end = nullptr;
    unsigned long long value = wcstoull(text.c_str(), &end, 0);
    if (errno == ERANGE || end == text.c_str() || (end && *end != L'\0')) {
        return std::nullopt;
    }

    return static_cast<uint64_t>(value);
}

std::optional<std::wstring> ResolvePath(const std::wstring& path) {
    DWORD needed = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);
    if (needed == 0) {
        return std::nullopt;
    }

    std::wstring full(needed, L'\0');
    DWORD written = GetFullPathNameW(path.c_str(), needed, full.data(), nullptr);
    if (written == 0 || written >= needed) {
        return std::nullopt;
    }

    full.resize(written);
    DWORD attrs = GetFileAttributesW(full.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
        return std::nullopt;
    }

    return full;
}

void PrintUsage() {
    std::cout << "Usage:\n"
              << "  address_resolver_native.exe --module <path> --address <hex> [--address <hex> ...]\n"
              << "      [--map <path>] [--symbols <path>] [--skip-pdb] [--include-nt-symbol-path]\n";
}

bool ParseArgs(int argc, wchar_t** argv, Options& options, std::string& error) {
    for (int i = 1; i < argc; ++i) {
        std::wstring arg = argv[i];

        auto requireValue = [&](const wchar_t* name) -> std::optional<std::wstring> {
            if (i + 1 >= argc) {
                error = std::string("Missing value for ") + ToUtf8(name);
                return std::nullopt;
            }
            ++i;
            return std::wstring(argv[i]);
        };

        if (arg == L"--module" || arg == L"-m") {
            auto value = requireValue(L"--module");
            if (!value) return false;
            options.modulePath = *value;
            continue;
        }

        if (arg == L"--map") {
            auto value = requireValue(L"--map");
            if (!value) return false;
            options.mapPath = *value;
            continue;
        }

        if (arg == L"--symbols") {
            auto value = requireValue(L"--symbols");
            if (!value) return false;
            options.symbolPath = *value;
            continue;
        }

        if (arg == L"--address" || arg == L"-a") {
            auto value = requireValue(L"--address");
            if (!value) return false;
            auto parsed = ParseHexU64(*value);
            if (!parsed) {
                error = "Invalid hex address: " + ToUtf8(*value);
                return false;
            }
            options.addresses.push_back(*parsed);
            continue;
        }

        if (arg == L"--skip-pdb") {
            options.skipPdb = true;
            continue;
        }

        if (arg == L"--include-nt-symbol-path") {
            options.includeNtSymbolPath = true;
            continue;
        }

        if (arg == L"--help" || arg == L"-h" || arg == L"/?") {
            PrintUsage();
            return false;
        }

        error = "Unknown argument: " + ToUtf8(arg);
        return false;
    }

    if (options.modulePath.empty()) {
        error = "--module is required";
        return false;
    }

    if (options.addresses.empty()) {
        error = "At least one --address is required";
        return false;
    }

    return true;
}

std::optional<ImageRange> ReadImageRange(const std::wstring& modulePath, std::string& error) {
    std::ifstream file(modulePath, std::ios::binary);
    if (!file) {
        error = "Unable to open module file";
        return std::nullopt;
    }

    IMAGE_DOS_HEADER dos{};
    file.read(reinterpret_cast<char*>(&dos), sizeof(dos));
    if (!file || dos.e_magic != IMAGE_DOS_SIGNATURE) {
        error = "Invalid DOS header";
        return std::nullopt;
    }

    file.seekg(dos.e_lfanew, std::ios::beg);
    DWORD peSig = 0;
    file.read(reinterpret_cast<char*>(&peSig), sizeof(peSig));
    if (!file || peSig != IMAGE_NT_SIGNATURE) {
        error = "Invalid PE signature";
        return std::nullopt;
    }

    IMAGE_FILE_HEADER fileHeader{};
    file.read(reinterpret_cast<char*>(&fileHeader), sizeof(fileHeader));
    if (!file) {
        error = "Invalid PE file header";
        return std::nullopt;
    }

    WORD magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (!file) {
        error = "Invalid optional header";
        return std::nullopt;
    }

    file.seekg(-static_cast<std::streamoff>(sizeof(magic)), std::ios::cur);

    ImageRange range{};
    if (magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
        IMAGE_OPTIONAL_HEADER64 opt{};
        file.read(reinterpret_cast<char*>(&opt), sizeof(opt));
        if (!file) {
            error = "Failed reading PE32+ optional header";
            return std::nullopt;
        }
        range.imageBase = opt.ImageBase;
        range.imageSize = opt.SizeOfImage;
    } else if (magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
        IMAGE_OPTIONAL_HEADER32 opt{};
        file.read(reinterpret_cast<char*>(&opt), sizeof(opt));
        if (!file) {
            error = "Failed reading PE32 optional header";
            return std::nullopt;
        }
        range.imageBase = opt.ImageBase;
        range.imageSize = opt.SizeOfImage;
    } else {
        error = "Unsupported PE optional header magic";
        return std::nullopt;
    }

    if (range.imageSize == 0) {
        error = "Invalid image size";
        return std::nullopt;
    }

    range.imageEnd = range.imageBase + range.imageSize - 1;
    return range;
}

std::optional<std::wstring> ResolveDefaultMapPath(const std::wstring& modulePath) {
    std::wstring candidate = modulePath;
    size_t dot = candidate.find_last_of(L'.');
    if (dot == std::wstring::npos) {
        return std::nullopt;
    }

    candidate = candidate.substr(0, dot) + L".map";
    return ResolvePath(candidate);
}

std::optional<std::string> BuildSymbolPath(const Options& options, const std::wstring& modulePath) {
    std::vector<std::wstring> parts;
    size_t sep = modulePath.find_last_of(L"\\/");
    if (sep != std::wstring::npos) {
        parts.push_back(modulePath.substr(0, sep));
    }

    wchar_t cwd[MAX_PATH] = {};
    DWORD cwdLen = GetCurrentDirectoryW(MAX_PATH, cwd);
    if (cwdLen > 0 && cwdLen < MAX_PATH) {
        parts.emplace_back(cwd, cwdLen);
    }

    if (options.symbolPath) {
        parts.push_back(*options.symbolPath);
    }

    if (options.includeNtSymbolPath) {
        wchar_t* nt = nullptr;
        wchar_t* ntAlt = nullptr;
        size_t ntLen = 0;
        size_t ntAltLen = 0;
        _wdupenv_s(&nt, &ntLen, L"_NT_SYMBOL_PATH");
        _wdupenv_s(&ntAlt, &ntAltLen, L"_NT_ALT_SYMBOL_PATH");
        if (nt && *nt) parts.emplace_back(nt);
        if (ntAlt && *ntAlt) parts.emplace_back(ntAlt);
        if (nt) free(nt);
        if (ntAlt) free(ntAlt);
    }

    std::wstring joined;
    bool first = true;
    for (const auto& part : parts) {
        if (part.empty()) continue;
        if (!first) joined += L';';
        joined += part;
        first = false;
    }

    if (joined.empty()) {
        return std::nullopt;
    }

    return ToUtf8(joined);
}

std::optional<MapEntry> ParseMapLine(const std::string& line) {
    const char* p = line.c_str();
    while (*p == ' ' || *p == '\t') ++p;

    unsigned segment = 0;
    unsigned long long offset = 0;
    int consumed = 0;
    if (sscanf_s(p, "%x:%llx%n", &segment, &offset, &consumed) != 2) {
        return std::nullopt;
    }

    p += consumed;
    while (*p == ' ' || *p == '\t') ++p;

    std::string symbol;
    while (*p && *p != ' ' && *p != '\t') {
        symbol.push_back(*p++);
    }
    if (symbol.empty()) {
        return std::nullopt;
    }

    while (*p == ' ' || *p == '\t') ++p;
    while (*p && isxdigit(static_cast<unsigned char>(*p))) ++p;
    while (*p == ' ' || *p == '\t') ++p;

    std::string object = p;
    while (!object.empty() && isspace(static_cast<unsigned char>(object.back()))) {
        object.pop_back();
    }

    if (symbol == "Address" || symbol == "entry" || symbol == "Static" || symbol == "Program") {
        return std::nullopt;
    }

    MapEntry entry;
    entry.segment = static_cast<uint32_t>(segment);
    entry.rva = static_cast<uint64_t>(offset);
    entry.symbol = symbol;
    entry.object = object;
    return entry;
}

std::vector<MapEntry> ParseMapEntries(const std::wstring& mapPath) {
    std::ifstream file(mapPath);
    std::vector<MapEntry> entries;
    if (!file) {
        return entries;
    }

    std::string line;
    while (std::getline(file, line)) {
        auto parsed = ParseMapLine(line);
        if (parsed) {
            entries.push_back(*parsed);
        }
    }

    std::sort(entries.begin(), entries.end(), [](const MapEntry& a, const MapEntry& b) {
        return a.rva < b.rva;
    });

    for (size_t i = 0; i + 1 < entries.size(); ++i) {
        entries[i].nextRva = entries[i + 1].rva;
    }

    return entries;
}

std::optional<MapResolved> ResolveMap(const std::vector<MapEntry>& entries, uint64_t rva) {
    if (entries.empty()) {
        return std::nullopt;
    }

    auto it = std::upper_bound(entries.begin(), entries.end(), rva, [](uint64_t value, const MapEntry& entry) {
        return value < entry.rva;
    });

    if (it == entries.begin()) {
        return std::nullopt;
    }
    --it;

    while (true) {
        const std::string& s = it->symbol;
        if (s.rfind("$unwind$", 0) != 0 && s.rfind("$pdata$", 0) != 0) {
            break;
        }

        if (it == entries.begin()) {
            return std::nullopt;
        }
        --it;
    }

    MapResolved out;
    out.entry = *it;
    out.displacement = rva - it->rva;
    out.contained = (rva >= it->rva && rva < it->nextRva);
    return out;
}

std::wstring FindDbgHelpPath() {
    wchar_t winDir[MAX_PATH] = {};
    GetWindowsDirectoryW(winDir, MAX_PATH);

    std::vector<std::wstring> candidates = {
        L"dbghelp.dll"
    };

    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = exePath;
    size_t slash = exeDir.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        exeDir = exeDir.substr(0, slash);
        candidates.push_back(exeDir + L"\\dbghelp.dll");
    }

    candidates.push_back(std::wstring(winDir) + L"\\System32\\dbghelp.dll");
    candidates.push_back(L"C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\dbghelp.dll");
    candidates.push_back(L"C:\\Program Files (x86)\\Windows Kits\\10\\Debuggers\\x64\\srcsrv\\dbghelp.dll");

    for (const auto& path : candidates) {
        if (path == L"dbghelp.dll") {
            HMODULE probe = LoadLibraryW(path.c_str());
            if (probe) {
                FreeLibrary(probe);
                return path;
            }
            continue;
        }

        DWORD attrs = GetFileAttributesW(path.c_str());
        if (attrs == INVALID_FILE_ATTRIBUTES || (attrs & FILE_ATTRIBUTE_DIRECTORY)) {
            continue;
        }

        HMODULE probe = LoadLibraryW(path.c_str());
        if (probe) {
            FreeLibrary(probe);
            return path;
        }
    }

    return L"";
}

class DbgHelpApi {
public:
    bool available = false;
    std::string error;

    using SymInitializeW_t = BOOL (WINAPI*)(HANDLE, PCWSTR, BOOL);
    using SymCleanup_t = BOOL (WINAPI*)(HANDLE);
    using SymSetOptions_t = DWORD (WINAPI*)(DWORD);
    using SymLoadModuleExW_t = DWORD64 (WINAPI*)(HANDLE, HANDLE, PCWSTR, PCWSTR, DWORD64, DWORD, PMODLOAD_DATA, DWORD);
    using SymFromAddr_t = BOOL (WINAPI*)(HANDLE, DWORD64, PDWORD64, PSYMBOL_INFO);
    using SymGetLineFromAddr64_t = BOOL (WINAPI*)(HANDLE, DWORD64, PDWORD, PIMAGEHLP_LINE64);

    SymInitializeW_t SymInitializeW_ = nullptr;
    SymCleanup_t SymCleanup_ = nullptr;
    SymSetOptions_t SymSetOptions_ = nullptr;
    SymLoadModuleExW_t SymLoadModuleExW_ = nullptr;
    SymFromAddr_t SymFromAddr_ = nullptr;
    SymGetLineFromAddr64_t SymGetLineFromAddr64_ = nullptr;

    HMODULE module = nullptr;

    bool LoadFromPath(const std::wstring& path) {
        module = LoadLibraryW(path.c_str());
        if (!module) {
            error = "LoadLibrary failed for dbghelp";
            return false;
        }

        SymInitializeW_ = reinterpret_cast<SymInitializeW_t>(GetProcAddress(module, "SymInitializeW"));
        SymCleanup_ = reinterpret_cast<SymCleanup_t>(GetProcAddress(module, "SymCleanup"));
        SymSetOptions_ = reinterpret_cast<SymSetOptions_t>(GetProcAddress(module, "SymSetOptions"));
        SymLoadModuleExW_ = reinterpret_cast<SymLoadModuleExW_t>(GetProcAddress(module, "SymLoadModuleExW"));
        SymFromAddr_ = reinterpret_cast<SymFromAddr_t>(GetProcAddress(module, "SymFromAddr"));
        SymGetLineFromAddr64_ = reinterpret_cast<SymGetLineFromAddr64_t>(GetProcAddress(module, "SymGetLineFromAddr64"));

        if (!SymInitializeW_ || !SymCleanup_ || !SymSetOptions_ || !SymLoadModuleExW_ || !SymFromAddr_ || !SymGetLineFromAddr64_) {
            error = "Missing one or more required dbghelp exports";
            FreeLibrary(module);
            module = nullptr;
            return false;
        }

        available = true;
        return true;
    }

    ~DbgHelpApi() {
        if (module) {
            FreeLibrary(module);
            module = nullptr;
        }
    }
};

class SymbolSession {
public:
    SymbolSession(DbgHelpApi& api, HANDLE process, const std::wstring& symbolPath)
        : api_(api), process_(process) {
        if (!api_.available) {
            return;
        }

        const DWORD opts = SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_FAIL_CRITICAL_ERRORS;
        api_.SymSetOptions_(opts);
        if (api_.SymInitializeW_(process_, symbolPath.c_str(), FALSE) == TRUE) {
            initialized_ = true;
        } else {
            DWORD symInitErr = GetLastError();
            if (symInitErr == ERROR_INVALID_PARAMETER) {
                // DbgHelp can report already-initialized state on reused process contexts.
                initialized_ = true;
            } else {
                error_ = "SymInitializeW failed with Win32 error " + std::to_string(symInitErr);
            }
        }
    }

    ~SymbolSession() {
        if (initialized_) {
            api_.SymCleanup_(process_);
        }
    }

    bool IsInitialized() const { return initialized_; }
    const std::string& Error() const { return error_; }

    bool LoadModule(const std::wstring& modulePath, uint64_t imageBase, uint32_t imageSize) {
        if (!initialized_) {
            return false;
        }

        DWORD64 loaded = api_.SymLoadModuleExW_(process_, nullptr, modulePath.c_str(), nullptr, imageBase, imageSize, nullptr, 0);
        if (loaded == 0) {
            error_ = "SymLoadModuleExW failed with Win32 error " + std::to_string(GetLastError());
            return false;
        }

        return true;
    }

    struct ResolveResult {
        bool symbolResolved = false;
        bool lineResolved = false;
        std::string symbol;
        uint64_t symbolDisplacement = 0;
        std::string file;
        uint32_t line = 0;
        uint32_t lineDisplacement = 0;
        std::string error;
    };

    ResolveResult Resolve(uint64_t address) {
        ResolveResult r;
        if (!initialized_) {
            r.error = error_;
            return r;
        }

        constexpr size_t kMaxName = 1024;
        constexpr size_t kBufferSize = sizeof(SYMBOL_INFO) + kMaxName;
        std::vector<uint8_t> symbolBuffer(kBufferSize, 0);
        auto* sym = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer.data());
        sym->SizeOfStruct = sizeof(SYMBOL_INFO);
        sym->MaxNameLen = static_cast<ULONG>(kMaxName);

        DWORD64 symDisp = 0;
        if (api_.SymFromAddr_(process_, address, &symDisp, sym) == TRUE) {
            r.symbolResolved = true;
            r.symbol = std::string(sym->Name, sym->NameLen);
            r.symbolDisplacement = symDisp;
        }

        IMAGEHLP_LINE64 line{};
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD lineDisp = 0;
        if (api_.SymGetLineFromAddr64_(process_, address, &lineDisp, &line) == TRUE) {
            r.lineResolved = true;
            r.file = line.FileName ? line.FileName : "";
            r.line = line.LineNumber;
            r.lineDisplacement = lineDisp;
        }

        if (!r.symbolResolved && !r.lineResolved) {
            r.error = "No symbol or source line resolved; Win32 error " + std::to_string(GetLastError());
        }

        return r;
    }

private:
    DbgHelpApi& api_;
    HANDLE process_ = nullptr;
    bool initialized_ = false;
    std::string error_;
};

void PrintHexLine(const char* label, uint64_t value) {
    std::cout << std::left << std::setw(12) << label
              << ": 0x"
              << std::right << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << value
              << std::dec << std::setfill(' ') << "\n";
}

} // namespace

int wmain(int argc, wchar_t** argv) {
    Options opts;
    std::string err;
    if (!ParseArgs(argc, argv, opts, err)) {
        if (!err.empty()) {
            std::cerr << "Error: " << err << "\n";
        }
        return 2;
    }

    auto modulePath = ResolvePath(opts.modulePath);
    if (!modulePath) {
        std::cerr << "Fatal: module path does not exist\n";
        return 1;
    }

    std::optional<std::wstring> mapPath;
    if (opts.mapPath) {
        mapPath = ResolvePath(*opts.mapPath);
    } else {
        mapPath = ResolveDefaultMapPath(*modulePath);
    }

    auto range = ReadImageRange(*modulePath, err);
    if (!range) {
        std::cerr << "Fatal: " << err << "\n";
        return 1;
    }

    std::vector<MapEntry> mapEntries;
    if (mapPath) {
        mapEntries = ParseMapEntries(*mapPath);
    }

    std::wstring dbghelpPath = FindDbgHelpPath();
    DbgHelpApi dbghelp;
    if (!opts.skipPdb && !dbghelpPath.empty()) {
        if (!dbghelp.LoadFromPath(dbghelpPath)) {
            std::cerr << "PDB Status  : unavailable (" << dbghelp.error << ")\n";
        }
    }

    std::wstring symbolPathW;
    auto symbolPath = BuildSymbolPath(opts, *modulePath);
    if (symbolPath) {
        symbolPathW = ToWide(*symbolPath);
    }

    HANDLE process = GetCurrentProcess();
    std::optional<SymbolSession> session;
    bool moduleLoaded = false;
    if (!opts.skipPdb && dbghelp.available) {
        session.emplace(dbghelp, process, symbolPathW);
        if (session->IsInitialized()) {
            moduleLoaded = session->LoadModule(*modulePath, range->imageBase, static_cast<uint32_t>(range->imageSize));
        }
    }

    std::cout << "Module      : " << ToUtf8(*modulePath) << "\n";
    PrintHexLine("Image Base", range->imageBase);
    PrintHexLine("Image End", range->imageEnd);
    std::cout << "Image Size   : 0x" << std::hex << std::uppercase << range->imageSize << std::dec << "\n";
    std::cout << "Map File     : " << (mapPath ? ToUtf8(*mapPath) : std::string("<not found>")) << "\n";
    std::cout << "DbgHelp      : " << (!dbghelpPath.empty() ? ToUtf8(dbghelpPath) : std::string("<not found>")) << "\n";
    std::cout << "PDB Mode     : " << (opts.skipPdb ? "disabled (--skip-pdb)" : (opts.includeNtSymbolPath ? "enabled (+NT symbol paths)" : "enabled (local symbol paths only)")) << "\n";

    if (opts.skipPdb) {
        std::cout << "PDB Status   : skipped by request\n";
    } else if (!dbghelp.available) {
        std::cout << "PDB Status   : unavailable (dbghelp not loaded)\n";
    } else if (!session || !session->IsInitialized()) {
        std::cout << "PDB Status   : unavailable (" << (session ? session->Error() : std::string("session not created")) << ")\n";
    } else if (!moduleLoaded) {
        std::cout << "PDB Status   : symbol engine initialized, module load failed (" << session->Error() << ")\n";
    } else {
        std::cout << "PDB Status   : symbol engine initialized\n";
    }

    for (uint64_t address : opts.addresses) {
        std::cout << "\n";
        PrintHexLine("Address", address);

        bool inside = (address >= range->imageBase && address <= range->imageEnd);
        if (!inside) {
            std::cout << "Result       : That address is also outside the module image\n";
            continue;
        }

        uint64_t rva = address - range->imageBase;
        std::cout << "Result       : inside the module image (RVA 0x" << std::hex << std::uppercase << rva << std::dec << ")\n";

        if (!opts.skipPdb && session && session->IsInitialized() && moduleLoaded) {
            auto resolved = session->Resolve(address);
            if (resolved.symbolResolved) {
                std::cout << "PDB Symbol   : " << resolved.symbol << " +0x" << std::hex << std::uppercase << resolved.symbolDisplacement << std::dec << "\n";
            }
            if (resolved.lineResolved) {
                std::cout << "Source       : " << resolved.file << ":" << resolved.line << "\n";
                if (resolved.lineDisplacement > 0) {
                    std::cout << "Line Disp    : 0x" << std::hex << std::uppercase << resolved.lineDisplacement << std::dec << "\n";
                }
            }
            if (!resolved.symbolResolved && !resolved.lineResolved && !resolved.error.empty()) {
                std::cout << "PDB Note     : " << resolved.error << "\n";
            }
        }

        auto mapResolved = ResolveMap(mapEntries, rva);
        if (mapResolved) {
            std::cout << "Map Symbol   : " << mapResolved->entry.symbol << " +0x" << std::hex << std::uppercase << mapResolved->displacement << std::dec << "\n";
            if (!mapResolved->entry.object.empty()) {
                std::cout << "Map Object   : " << mapResolved->entry.object << "\n";
            }
            std::cout << "Map Match    : " << (mapResolved->contained ? "contained" : "nearest-lower") << "\n";
        }
    }

    return 0;
}
