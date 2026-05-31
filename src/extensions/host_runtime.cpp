/**
 * @file host_runtime.cpp
 * @brief Native extension host core
 * 
 * Provides:
 * - Extension process lifecycle management
 * - Message passing between IDE and extensions
 * - Extension capability registration
 * - Error handling and recovery
 * 
 * @author RawrXD Extension Team
 * @version 1.0.0
 */

#include "host_runtime.h"
#include "process_broker.h"
#include "os_sandbox.h"
#include <windows.h>
#include <psapi.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace RawrXD::Extensions {

static uint64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}

ExtensionHost::ExtensionHost(const HostConfig& config)
    : m_config(config)
    , m_nextExtensionId(1)
    , m_running(false)
    , m_ipcPipe(INVALID_HANDLE_VALUE)
    , m_broker(nullptr)
    , m_sandbox(nullptr)
{
}

ExtensionHost::~ExtensionHost() {
    shutdown();
}

bool ExtensionHost::initialize(ProcessBroker* broker, OSSandbox* sandbox) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_running) return true;

    m_broker = broker;
    m_sandbox = sandbox;

    std::filesystem::path hostDir = m_config.extensionDir / "host";
    std::filesystem::create_directories(hostDir);

    if (!initializeIPC()) return false;

    m_running = true;
    m_messageThread = std::thread(&ExtensionHost::processMessages, this);
    m_healthThread = std::thread(&ExtensionHost::healthMonitorLoop, this);
    return true;
}

void ExtensionHost::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running) return;
        m_running = false;
    }
    m_messageCv.notify_all();
    m_healthCv.notify_all();

    unloadAllExtensions();

    if (m_messageThread.joinable()) m_messageThread.join();
    if (m_healthThread.joinable()) m_healthThread.join();

    cleanupIPC();
}

int64_t ExtensionHost::loadExtension(const std::string& path,
                                     const ExtensionManifest& manifest) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_running) return -1;
    if (m_extensions.size() >= m_config.maxExtensions) return -1;
    if (!validateExtension(path, manifest)) return -1;

    int64_t extId = m_nextExtensionId++;

    ExtensionInstance instance;
    instance.id = extId;
    instance.manifest = manifest;
    instance.path = path;
    instance.state = ExtensionState::Loading;
    instance.lastHeartbeat = nowMs();

    HMODULE hModule = LoadLibraryA(path.c_str());
    if (!hModule) return -1;

    instance.hModule = hModule;

    auto initFunc = reinterpret_cast<ExtensionInitFunc>(
        GetProcAddress(hModule, "extension_init"));
    auto activateFunc = reinterpret_cast<ExtensionActivateFunc>(
        GetProcAddress(hModule, "extension_activate"));
    auto deactivateFunc = reinterpret_cast<ExtensionDeactivateFunc>(
        GetProcAddress(hModule, "extension_deactivate"));
    auto messageFunc = reinterpret_cast<ExtensionMessageFunc>(
        GetProcAddress(hModule, "extension_handle_message"));

    if (!initFunc) {
        FreeLibrary(hModule);
        return -1;
    }

    instance.initFunc = initFunc;
    instance.activateFunc = activateFunc;
    instance.deactivateFunc = deactivateFunc;
    instance.messageFunc = messageFunc;

    ExtensionContext context;
    context.hostVersion = RAWRXD_VERSION;
    context.extensionId = extId;
    context.apiVersion = manifest.apiVersion;

    if (!initFunc(&context)) {
        FreeLibrary(hModule);
        return -1;
    }

    instance.state = ExtensionState::Loaded;

    if (m_sandbox) {
        m_sandbox->applyToProcess(extId, GetCurrentProcess());
    }

    if (manifest.autoActivate && activateFunc) {
        if (activateFunc()) {
            instance.state = ExtensionState::Active;
        }
    }

    m_extensions[extId] = instance;
    return extId;
}

bool ExtensionHost::unloadExtension(int64_t extId) {
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return false;

    auto& instance = it->second;

    if (instance.state == ExtensionState::Active) {
        lock.unlock();
        sendShutdownHandshake(extId);
        lock.lock();
    }

    if (instance.state == ExtensionState::Active && instance.deactivateFunc) {
        instance.deactivateFunc();
    }

    if (m_sandbox) {
        m_sandbox->removeFromProcess(extId);
    }

    if (instance.hModule) {
        FreeLibrary(static_cast<HMODULE>(instance.hModule));
    }

    m_extensions.erase(it);
    return true;
}

void ExtensionHost::unloadAllExtensions() {
    std::vector<int64_t> extIds;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (const auto& [id, _] : m_extensions) {
            extIds.push_back(id);
        }
    }
    for (int64_t id : extIds) {
        unloadExtension(id);
    }
}

bool ExtensionHost::activateExtension(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return false;

    auto& instance = it->second;
    if (instance.state == ExtensionState::Active) return true;

    if (instance.activateFunc && instance.activateFunc()) {
        instance.state = ExtensionState::Active;
        instance.lastHeartbeat = nowMs();
        return true;
    }
    return false;
}

bool ExtensionHost::deactivateExtension(int64_t extId) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return false;

    auto& instance = it->second;
    if (instance.state != ExtensionState::Active) return true;

    if (instance.deactivateFunc && instance.deactivateFunc()) {
        instance.state = ExtensionState::Loaded;
        return true;
    }
    return false;
}

ExtensionState ExtensionHost::getExtensionState(int64_t extId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return ExtensionState::Unknown;
    return it->second.state;
}

bool ExtensionHost::sendMessage(int64_t extId, const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_extensions.find(extId);
    if (it == m_extensions.end()) return false;

    auto& instance = it->second;
    if (instance.state != ExtensionState::Active || !instance.messageFunc) {
        return false;
    }
    return instance.messageFunc(message.c_str(), message.length());
}

bool ExtensionHost::sendMessageWithResponse(int64_t extId,
                                               const std::string& message,
                                               std::string& response,
                                               uint32_t timeoutMs) {
    if (m_broker && m_broker->isExtensionAlive(extId)) {
        BrokerMessage req;
        req.type = static_cast<uint32_t>(BrokerMsgType::Request);
        req.payload.assign(message.begin(), message.end());
        req.payloadLen = static_cast<uint32_t>(req.payload.size());
        req.crc32 = 0;
        if (!m_broker->sendMessage(extId, req)) return false;
        BrokerMessage resp;
        if (!m_broker->recvMessage(extId, resp, timeoutMs)) return false;
        response.assign(resp.payload.begin(), resp.payload.end());
        return true;
    }
    return false;
}

void ExtensionHost::broadcastMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& [id, instance] : m_extensions) {
        if (instance.state == ExtensionState::Active && instance.messageFunc) {
            instance.messageFunc(message.c_str(), message.length());
        }
    }
}

void ExtensionHost::registerCapability(const std::string& name,
                                      const CapabilityHandler& handler) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capabilities[name] = handler;
}

void ExtensionHost::unregisterCapability(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_capabilities.erase(name);
}

bool ExtensionHost::invokeCapability(const std::string& name,
                                    const std::string& params,
                                    std::string& result) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_capabilities.find(name);
    if (it == m_capabilities.end()) return false;
    return it->second(params, result);
}

bool ExtensionHost::validateExtension(const std::string& path,
                                     const ExtensionManifest& manifest) {
    if (!std::filesystem::exists(path)) return false;
    if (manifest.apiVersion > m_config.maxApiVersion) return false;
    for (const auto& cap : manifest.requiredCapabilities) {
        if (m_capabilities.find(cap) == m_capabilities.end()) return false;
    }
    for (const auto& perm : manifest.requestedPermissions) {
        if (!isPermissionAllowed(perm)) return false;
    }
    return true;
}

bool ExtensionHost::isPermissionAllowed(const std::string& permission) const {
    for (const auto& allowed : m_config.allowedPermissions) {
        if (allowed == permission || allowed == "*") return true;
    }
    return false;
}

bool ExtensionHost::initializeIPC() {
    std::string pipeName = "\\\\.\\pipe\\RawrXD_ExtensionHost_" +
                          std::to_string(GetCurrentProcessId());
    m_ipcPipe = CreateNamedPipeA(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        PIPE_UNLIMITED_INSTANCES,
        65536, 65536, 0, nullptr);
    return m_ipcPipe != INVALID_HANDLE_VALUE;
}

void ExtensionHost::cleanupIPC() {
    if (m_ipcPipe != INVALID_HANDLE_VALUE) {
        DisconnectNamedPipe(m_ipcPipe);
        CloseHandle(m_ipcPipe);
        m_ipcPipe = INVALID_HANDLE_VALUE;
    }
}

void ExtensionHost::processMessages() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_messageCv.wait_for(lock, std::chrono::milliseconds(100),
            [this] { return !m_running || !m_messageQueue.empty(); });
        while (!m_messageQueue.empty()) {
            auto msg = std::move(m_messageQueue.front());
            m_messageQueue.pop();
            lock.unlock();
            processSingleMessage(msg);
            lock.lock();
        }
    }
}

void ExtensionHost::processSingleMessage(const QueuedMessage& msg) {
    try {
        auto j = nlohmann::json::parse(msg.payload);
        std::string method = j.value("method", "");
        if (method == "capability/invoke") {
            std::string capName = j.value("capability", "");
            std::string params = j.value("params", "");
            std::string result;
            bool ok = invokeCapability(capName, params, result);
            nlohmann::json resp;
            resp["success"] = ok;
            resp["result"] = result;
            sendMessage(msg.extId, resp.dump());
        } else if (method == "heartbeat") {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_extensions.find(msg.extId);
            if (it != m_extensions.end()) {
                it->second.lastHeartbeat = nowMs();
            }
        }
    } catch (...) {
        // Invalid JSON - ignore
    }
}

void ExtensionHost::healthMonitorLoop() {
    while (m_running) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_healthCv.wait_for(lock, std::chrono::milliseconds(3000),
            [this] { return !m_running; });
        if (!m_running) break;

        uint64_t now = nowMs();
        std::vector<int64_t> deadExtensions;

        for (auto& [id, instance] : m_extensions) {
            if (instance.state != ExtensionState::Active) continue;
            if ((now - instance.lastHeartbeat) > 10000) {
                deadExtensions.push_back(id);
                continue;
            }
            if (instance.hModule) {
                PROCESS_MEMORY_COUNTERS pmc = {};
                HANDLE hProcess = GetCurrentProcess();
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    if (pmc.WorkingSetSize > m_config.maxMemoryPerExtension) {
                        deadExtensions.push_back(id);
                    }
                }
            }
        }
        lock.unlock();

        for (int64_t id : deadExtensions) {
            handleExtensionCrash(id, "Heartbeat timeout or memory limit exceeded");
        }
    }
}

void ExtensionHost::handleExtensionCrash(int64_t extId, const std::string& reason) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_extensions.find(extId);
        if (it != m_extensions.end()) {
            it->second.state = ExtensionState::Error;
        }
    }
    OutputDebugStringA("[ExtensionHost] Extension crashed: ");
    OutputDebugStringA(std::to_string(extId).c_str());
    OutputDebugStringA(" - ");
    OutputDebugStringA(reason.c_str());
    OutputDebugStringA("\n");
    deactivateExtension(extId);
    nlohmann::json event;
    event["type"] = "extension/crash";
    event["extensionId"] = extId;
    event["reason"] = reason;
    broadcastMessage(event.dump());
}

void ExtensionHost::sendShutdownHandshake(int64_t extId) {
    nlohmann::json msg;
    msg["type"] = "shutdown";
    msg["timeoutMs"] = 5000;
    sendMessage(extId, msg.dump());
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(5000)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        auto state = getExtensionState(extId);
        if (state == ExtensionState::Loaded || state == ExtensionState::Unknown) {
            break;
        }
    }
}

HostStats ExtensionHost::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    HostStats stats;
    stats.totalExtensions = static_cast<int>(m_extensions.size());
    stats.activeExtensions = 0;
    stats.totalMemoryUsage = 0;
    for (const auto& [id, instance] : m_extensions) {
        if (instance.state == ExtensionState::Active) {
            stats.activeExtensions++;
        }
        if (instance.hModule) {
            MODULEINFO modInfo;
            if (GetModuleInformation(GetCurrentProcess(),
                                    static_cast<HMODULE>(instance.hModule),
                                    &modInfo, sizeof(modInfo))) {
                stats.totalMemoryUsage += modInfo.SizeOfImage;
            }
        }
    }
    return stats;
}

} // namespace RawrXD::Extensions
