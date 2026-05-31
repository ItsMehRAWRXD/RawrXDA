#include "ExtensionInstance.h"
#include "ExtensionAPI_VSCode_Internal.h"
#include <iostream>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace RawrXD::Extensions {

// Forward declaration of dispatcher
void DispatchAPI(ExtensionInstance* instance, const IPCHeader& header, const std::vector<uint8_t>& payload);

ExtensionInstance::ExtensionInstance(const std::string& id, const std::string& path)
    : m_id(id), m_path(path) {
    std::wstring wid(id.begin(), id.end());
    m_ipc = std::make_unique<ExtensionIPCChannel>(wid);
}

ExtensionInstance::~ExtensionInstance() {
    Shutdown();
}

bool ExtensionInstance::Launch() {
    if (!m_ipc->Create()) return false;

    char cmdLine[MAX_PATH];
    sprintf_s(cmdLine, "\"%s\" --extensionId=%s", m_path.c_str(), m_id.c_str());

    m_hProcess = MASM_CreateIsolatedProcess(NULL, cmdLine, NULL, 256);
    if (!m_hProcess) return false;

    m_running = true;
    m_messageThread = std::thread([this]() {
        if (m_ipc->Connect()) {
            MessageLoop();
        }
        m_running = false;
    });

    return true;
}

void ExtensionInstance::Shutdown() {
    if (m_running) {
        m_running = false;
        m_ipc->Send(IPCMessageType::Shutdown, 0, {});
        if (m_messageThread.joinable()) m_messageThread.join();
    }
    if (m_hProcess) {
        TerminateProcess(m_hProcess, 0);
        CloseHandle(m_hProcess);
        m_hProcess = NULL;
    }
}

void ExtensionInstance::MessageLoop() {
    while (m_running) {
        IPCHeader header;
        std::vector<uint8_t> payload;
        if (m_ipc->Receive(header, payload)) {
            DispatchAPI(this, header, payload);
        } else {
            break;
        }
    }
}

}

// Local helper to link dispatcher
#include <nlohmann/json.hpp>
extern void RawrXD::Extensions::APIDispatcher::Dispatch(RawrXD::Extensions::ExtensionInstance*, const RawrXD::Extensions::IPCHeader&, const std::vector<uint8_t>&);

void RawrXD::Extensions::DispatchAPI(ExtensionInstance* instance, const IPCHeader& header, const std::vector<uint8_t>& payload) {
    if (header.type == IPCMessageType::Request) {
        APIDispatcher::Dispatch(instance, header, payload);
    }
}
