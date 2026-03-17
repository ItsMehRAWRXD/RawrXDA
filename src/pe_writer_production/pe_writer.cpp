// ============================================================================
// PE Writer Production Implementation
// Main implementation file with modular architecture
// ============================================================================

#include "pe_writer.h"
#include "core/pe_structure_builder.h"
#include "emitter/code_emitter.h"
#include "structures/import_resolver.h"
#include "structures/relocation_manager.h"
#include "structures/resource_manager.h"
#include "config/config_parser.h"
#include "core/pe_validator.h"
#include "core/error_handler.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>
#include <mutex>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace pewriter {

// ============================================================================
// PEWriter Implementation
// ============================================================================

PEWriter::PEWriter()
    : structureBuilder_(std::make_unique<PEStructureBuilder>()),
      codeEmitter_(std::make_unique<CodeEmitter>()),
      importResolver_(std::make_unique<ImportResolver>()),
      relocationManager_(std::make_unique<RelocationManager>()),
      resourceManager_(std::make_unique<ResourceManager>()),
      validator_(std::make_unique<PEValidator>()),
      errorHandler_(std::make_unique<ErrorHandler>()) {
}

PEWriter::~PEWriter() = default;

bool PEWriter::configure(const PEConfig& config) {
    try {
        config_ = config;

        // Validate configuration
        if (!validator_->validateConfig(config_)) {
            errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR,
                                  "Invalid configuration parameters");
            return false;
        }

        // Initialize components with config
        structureBuilder_->setConfig(config_);
        codeEmitter_->setArchitecture(config_.architecture);
        importResolver_->setLibraries(config_.libraries);

        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::loadConfigFromJSON(const std::string& jsonPath) {
    try {
        ConfigParser parser;
        PEConfig config;
        if (!parser.parseJSON(jsonPath, config)) {
            errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR,
                                  "Failed to parse JSON configuration");
            return false;
        }
        return configure(config);
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::loadConfigFromXML(const std::string& xmlPath) {
    try {
        ConfigParser parser;
        PEConfig config;
        if (!parser.parseXML(xmlPath, config)) {
            errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR,
                                  "Failed to parse XML configuration");
            return false;
        }
        return configure(config);
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::CONFIGURATION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::addCodeSection(const CodeSection& section) {
    try {
        if (section.code.empty()) {
            errorHandler_->setError(PEErrorCode::INVALID_PARAMETER,
                                  "Code section cannot be empty");
            return false;
        }

        // Validate section name
        if (section.name.empty() || section.name.length() > 8) {
            errorHandler_->setError(PEErrorCode::INVALID_PARAMETER,
                                  "Invalid section name");
            return false;
        }

        // Add to structure builder
        structureBuilder_->addSection(section.name, section.code,
                                    section.characteristics);

        // Emit code if needed
        codeEmitter_->emitSection(section);

        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::CODE_GENERATION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::addDataSection(const std::string& name, const std::vector<uint8_t>& data) {
    try {
        if (name.empty() || name.length() > 8) {
            errorHandler_->setError(PEErrorCode::INVALID_PARAMETER,
                                  "Invalid section name");
            return false;
        }

        uint32_t characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
                                 IMAGE_SCN_MEM_READ;
        if (!data.empty()) {
            // Make writable if data is present
            characteristics |= IMAGE_SCN_MEM_WRITE;
        }

        structureBuilder_->addSection(name, data, characteristics);
        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::INVALID_PARAMETER, e.what());
        return false;
    }
}

bool PEWriter::addImport(const ImportEntry& import) {
    try {
        if (import.library.empty() || import.symbol.empty()) {
            errorHandler_->setError(PEErrorCode::INVALID_PARAMETER,
                                  "Invalid import entry");
            return false;
        }

        importResolver_->addImport(import.library, import.symbol, import.hint);
        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::IMPORT_RESOLUTION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::addRelocation(const RelocationEntry& relocation) {
    try {
        relocationManager_->addRelocation(relocation);
        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::RELOCATION_ERROR, e.what());
        return false;
    }
}

bool PEWriter::addResource(int type, int id, const std::vector<uint8_t>& data) {
    try {
        resourceManager_->addResource(type, id, data);
        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::INVALID_PARAMETER, e.what());
        return false;
    }
}

bool PEWriter::addVersionInfo(const std::unordered_map<std::string, std::string>& info) {
    try {
        resourceManager_->addVersionInfo(info);
        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::INVALID_PARAMETER, e.what());
        return false;
    }
}

bool PEWriter::build() {
    try {
        // Reset state
        isBuilt_ = false;
        peImage_.clear();

        // Build PE structure
        if (!structureBuilder_->build()) {
            errorHandler_->setError(PEErrorCode::INVALID_PE_STRUCTURE,
                                  "Failed to build PE structure");
            return false;
        }

        // Resolve imports
        if (!importResolver_->resolve()) {
            errorHandler_->setError(PEErrorCode::IMPORT_RESOLUTION_ERROR,
                                  "Failed to resolve imports");
            return false;
        }

        // Apply relocations
        if (!relocationManager_->apply()) {
            errorHandler_->setError(PEErrorCode::RELOCATION_ERROR,
                                  "Failed to apply relocations");
            return false;
        }

        // Build resources
        if (!resourceManager_->build()) {
            errorHandler_->setError(PEErrorCode::INVALID_PE_STRUCTURE,
                                  "Failed to build resources");
            return false;
        }

        // Finalize PE image
        peImage_ = structureBuilder_->getImage();
        isBuilt_ = true;

        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::INVALID_PE_STRUCTURE, e.what());
        return false;
    }
}

bool PEWriter::validate() const {
    if (!isBuilt_) {
        return false;
    }

    return validator_->validateImage(peImage_);
}

bool PEWriter::writeToFile(const std::string& filename) {
    try {
        if (!isBuilt_) {
            errorHandler_->setError(PEErrorCode::INVALID_PE_STRUCTURE,
                                  "PE image not built");
            return false;
        }

        std::ofstream file(filename, std::ios::binary);
        if (!file) {
            errorHandler_->setError(PEErrorCode::FILE_IO_ERROR,
                                  "Failed to open output file");
            return false;
        }

        file.write(reinterpret_cast<const char*>(peImage_.data()), peImage_.size());
        if (!file) {
            errorHandler_->setError(PEErrorCode::FILE_IO_ERROR,
                                  "Failed to write to file");
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        errorHandler_->setError(PEErrorCode::FILE_IO_ERROR, e.what());
        return false;
    }
}

bool PEWriter::enableDebugging() {
    // Implementation for debug information
    return structureBuilder_->enableDebugInfo();
}

bool PEWriter::addTLS() {
    // Implementation for TLS support
    return structureBuilder_->addTLSSupport();
}

bool PEWriter::addExceptionHandling() {
    // Implementation for SEH
    return structureBuilder_->addExceptionHandling();
}

PEErrorCode PEWriter::getLastError() const {
    return errorHandler_->getLastError();
}

std::string PEWriter::getErrorMessage() const {
    return errorHandler_->getErrorMessage();
}

void PEWriter::clearError() {
    errorHandler_->clear();
}

void PEWriter::setProgressCallback(ProgressCallback callback) {
    // Set progress callback on all components
    structureBuilder_->setProgressCallback(callback);
    importResolver_->setProgressCallback(callback);
    relocationManager_->setProgressCallback(callback);
}

void PEWriter::setErrorCallback(ErrorCallback callback) {
    errorHandler_->setErrorCallback(callback);
}

// ============================================================================
// IDEBridge Implementation
// ============================================================================

IDEBridge::IDEBridge()
    : writer_(std::make_unique<PEWriter>()),
      lspInitialized_(false),
      restRunning_(false),
      restPort_(0)
{
    #ifdef _WIN32
    hCommandPipe_ = INVALID_HANDLE_VALUE;
    hLSPPipe_     = INVALID_HANDLE_VALUE;
    restSocket_   = INVALID_SOCKET;
    #endif
}

IDEBridge::~IDEBridge() {
    stopRESTServer();
    #ifdef _WIN32
    if (hCommandPipe_ != INVALID_HANDLE_VALUE) { CloseHandle(hCommandPipe_); hCommandPipe_ = INVALID_HANDLE_VALUE; }
    if (hLSPPipe_     != INVALID_HANDLE_VALUE) { CloseHandle(hLSPPipe_);     hLSPPipe_     = INVALID_HANDLE_VALUE; }
    #endif
}

bool IDEBridge::registerCommands() {
    // Create a named pipe for receiving IDE commands
    #ifdef _WIN32
    hCommandPipe_ = CreateNamedPipeA(
        "\\\\.\\pipe\\RawrXD_PEWriter_Commands",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,        // max instances
        4096,     // out buffer
        4096,     // in buffer
        0,        // default timeout
        nullptr); // default security
    if (hCommandPipe_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    #endif

    // Register known command dispatch table
    commandDispatch_["build"]         = [this](const std::vector<std::string>& a) { return writer_->build(); };
    commandDispatch_["validate"]      = [this](const std::vector<std::string>& a) { return writer_->validate(); };
    commandDispatch_["write"]         = [this](const std::vector<std::string>& a) {
        return !a.empty() && writer_->writeToFile(a[0]);
    };
    commandDispatch_["addImport"]     = [this](const std::vector<std::string>& a) {
        if (a.size() < 2) return false;
        ImportEntry entry; entry.library = a[0]; entry.symbol = a[1];
        return writer_->addImport(entry);
    };
    commandDispatch_["enableDebug"]   = [this](const std::vector<std::string>&) { return writer_->enableDebugging(); };
    commandDispatch_["clearError"]    = [this](const std::vector<std::string>&) { writer_->clearError(); return true; };

    return true;
}

bool IDEBridge::handleCommand(const std::string& command, const std::vector<std::string>& args) {
    std::lock_guard<std::mutex> guard(mutex_);

    auto it = commandDispatch_.find(command);
    if (it == commandDispatch_.end()) {
        return false; // Unknown command
    }
    return it->second(args);
}

bool IDEBridge::initializeLSP() {
    #ifdef _WIN32
    hLSPPipe_ = CreateNamedPipeA(
        "\\\\.\\pipe\\RawrXD_PEWriter_LSP",
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
        1, 65536, 65536, 0, nullptr);
    if (hLSPPipe_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    #endif
    lspInitialized_ = true;
    return true;
}

bool IDEBridge::processLSPMessage(const std::string& message) {
    if (!lspInitialized_) return false;
    std::lock_guard<std::mutex> guard(mutex_);

    // Parse JSON-RPC 2.0 envelope: {"jsonrpc":"2.0","id":N,"method":"...","params":{...}}
    auto extractField = [&](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\"";
        size_t pos = message.find(search);
        if (pos == std::string::npos) return "";
        pos = message.find(':', pos + search.size());
        if (pos == std::string::npos) return "";
        pos++;
        while (pos < message.size() && (message[pos] == ' ' || message[pos] == '\t')) pos++;
        if (pos >= message.size()) return "";
        if (message[pos] == '"') {
            size_t end = message.find('"', pos + 1);
            return (end != std::string::npos) ? message.substr(pos + 1, end - pos - 1) : "";
        }
        size_t end = message.find_first_of(",}]", pos);
        return message.substr(pos, end - pos);
    };

    std::string jsonrpc = extractField("jsonrpc");
    if (jsonrpc != "2.0") return false;

    std::string method = extractField("method");
    std::string id     = extractField("id");

    // Dispatch LSP methods
    if (method == "initialize") {
        // Respond with server capabilities
        std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + id +
            ",\"result\":{\"capabilities\":{\"peWriter\":true,\"version\":\"" +
            getVersion() + "\"}}}";
        lspResponseQueue_.push_back(response);
        return true;
    } else if (method == "pewriter/build") {
        bool ok = writer_->build();
        std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + id +
            ",\"result\":{\"success\":" + (ok ? "true" : "false") + "}}";
        lspResponseQueue_.push_back(response);
        return true;
    } else if (method == "pewriter/validate") {
        bool ok = writer_->validate();
        std::string response = "{\"jsonrpc\":\"2.0\",\"id\":" + id +
            ",\"result\":{\"valid\":" + (ok ? "true" : "false") + "}}";
        lspResponseQueue_.push_back(response);
        return true;
    } else if (method == "shutdown") {
        lspResponseQueue_.push_back("{\"jsonrpc\":\"2.0\",\"id\":" + id + ",\"result\":null}");
        return true;
    }

    // Unknown method
    lspResponseQueue_.push_back("{\"jsonrpc\":\"2.0\",\"id\":" + id +
        ",\"error\":{\"code\":-32601,\"message\":\"Method not found\"}}");
    return false;
}

bool IDEBridge::startRESTServer(uint16_t port) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (restRunning_) return false;

    #ifdef _WIN32
    // Bind a TCP socket on localhost:port using Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return false;

    restSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (restSocket_ == INVALID_SOCKET) { WSACleanup(); return false; }

    sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = htons(port);

    if (bind(restSocket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        closesocket(restSocket_); WSACleanup(); return false;
    }
    if (listen(restSocket_, SOMAXCONN) == SOCKET_ERROR) {
        closesocket(restSocket_); WSACleanup(); return false;
    }

    restRunning_ = true;
    restPort_    = port;

    // Launch accept loop on background thread
    restThread_ = std::thread([this]() {
        while (restRunning_) {
            SOCKET client = accept(restSocket_, nullptr, nullptr);
            if (client == INVALID_SOCKET) break;
            // Read HTTP request line
            char buf[4096]{};
            int n = recv(client, buf, sizeof(buf) - 1, 0);
            if (n > 0) {
                std::string req(buf, n);
                std::string body;
                // Dispatch simple REST endpoints
                if (req.find("POST /build") != std::string::npos) {
                    bool ok = writer_->build();
                    body = "{\"success\":" + std::string(ok ? "true" : "false") + "}";
                } else if (req.find("POST /validate") != std::string::npos) {
                    bool ok = writer_->validate();
                    body = "{\"valid\":" + std::string(ok ? "true" : "false") + "}";
                } else if (req.find("GET /version") != std::string::npos) {
                    body = "{\"version\":\"" + getVersion() + "\"}";
                } else {
                    body = "{\"error\":\"unknown endpoint\"}";
                }
                std::string resp = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n"
                    "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
                send(client, resp.c_str(), static_cast<int>(resp.size()), 0);
            }
            closesocket(client);
        }
    });
    #endif
    return true;
}

void IDEBridge::stopRESTServer() {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!restRunning_) return;
    restRunning_ = false;

    #ifdef _WIN32
    closesocket(restSocket_);
    restSocket_ = INVALID_SOCKET;
    if (restThread_.joinable()) restThread_.join();
    WSACleanup();
    #endif
}

void IDEBridge::lock() {
    mutex_.lock();
}

void IDEBridge::unlock() {
    mutex_.unlock();
}

// ============================================================================
// Utility Functions
// ============================================================================

std::string getVersion() {
    return "2.0.0";
}

bool isSupportedArchitecture(PEArchitecture arch) {
    return arch == PEArchitecture::x64 || arch == PEArchitecture::x86;
}

std::vector<std::string> getSupportedFeatures() {
    return {
        "x64 Code Generation",
        "Import Resolution",
        "Relocation Handling",
        "Resource Management",
        "TLS Support",
        "Exception Handling",
        "Debug Information",
        "IDE Integration"
    };
}

// ============================================================================
// Exception Implementation
// ============================================================================

PEException::PEException(PEErrorCode code, const std::string& message)
    : code_(code), message_(message) {}

PEErrorCode PEException::code() const {
    return code_;
}

const char* PEException::what() const noexcept {
    return message_.c_str();
}

} // namespace pewriter