extern "C" {
    void RawrXD_Debugger_AddWatch(unsigned __int64 addr, unsigned __int64 len, const char* name);
    void RawrXD_Debugger_RemoveWatch(unsigned __int64 addr);
    void RawrXD_Debugger_RemoveWatchString(const char* name);
}
#include "webview2_bridge.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <WebView2.h>
#include "debugger_core.hpp"

namespace rawrxd::ui {

// Minimal COM helper since we can't find WIL/WRL easily
template <typename THandler, typename TArg>
class WebView2Callback final : public THandler {
public:
    explicit WebView2Callback(std::function<HRESULT(HRESULT, TArg*)> callback)
        : m_callback(callback), m_refCount(1) {}

    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject) override {
        if (riid == __uuidof(IUnknown) || riid == __uuidof(THandler)) {
            *ppvObject = static_cast<THandler*>(this);
            AddRef();
            return S_OK;
        }
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }

    virtual ULONG STDMETHODCALLTYPE AddRef() override {
        return InterlockedIncrement(&m_refCount);
    }

    virtual ULONG STDMETHODCALLTYPE Release() override {
        ULONG count = InterlockedDecrement(&m_refCount);
        if (count == 0) delete this;
        return count;
    }

    virtual HRESULT STDMETHODCALLTYPE Invoke(HRESULT result, TArg* handler) override {
        return m_callback(result, handler);
    }

private:
    std::function<HRESULT(HRESULT, TArg*)> m_callback;
    volatile LONG m_refCount;
};

namespace {
std::string bytesToHex(const uint8_t* data, size_t len) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string out;
    out.resize(len * 2);
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = data[i];
        out[(i * 2)] = kHex[(b >> 4) & 0x0F];
        out[(i * 2) + 1] = kHex[b & 0x0F];
    }
    return out;
}

bool tryParseHexAddress(const std::string& text, uint64_t& addressOut) {
    if (text.size() < 3) return false;
    if (!(text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))) return false;
    try {
        addressOut = std::stoull(text, nullptr, 16);
        return true;
    } catch (...) {
        return false;
    }
}
} // namespace

bool WebView2Bridge::initialize(HWND hwnd) {
    m_hwnd = hwnd;
    std::cout << "[WebView2Bridge] Initializing WebView2 for HWND: " << hwnd << std::endl;

    if (!m_webview2Loader) {
        m_webview2Loader = LoadLibraryW(L"WebView2Loader.dll");
    }
    if (!m_webview2Loader) {
        std::cerr << "[WebView2Bridge] WebView2Loader.dll not found" << std::endl;
        return false;
    }

    auto createEnv = reinterpret_cast<CreateCoreWebView2EnvironmentWithOptionsFunc>(
        GetProcAddress(m_webview2Loader, "CreateCoreWebView2EnvironmentWithOptions"));
    if (!createEnv) {
        std::cerr << "[WebView2Bridge] CreateCoreWebView2EnvironmentWithOptions export missing" << std::endl;
        return false;
    }

    auto envHandler = new WebView2Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler, ICoreWebView2Environment>(
        [this, hwnd](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
            if (FAILED(result)) return result;

            auto conHandler = new WebView2Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler, ICoreWebView2Controller>(
                [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                    if (FAILED(result)) return result;
                    
                    m_controller = controller;
                    m_controller->AddRef();
                    m_controller->get_CoreWebView2(&m_webview);
                    m_webview->AddRef();

                    RECT bounds;
                    GetClientRect(m_hwnd, &bounds);
                    m_controller->put_Bounds(bounds);

                    m_webview->Navigate(L"https://rawrxd.internal/dashboard");
                    return S_OK;
                });

            const HRESULT createHr = env->CreateCoreWebView2Controller(hwnd, conHandler);
            if (FAILED(createHr)) {
                conHandler->Release();
                return createHr;
            }
            return createHr;
        });

    const HRESULT hr = createEnv(nullptr, nullptr, nullptr, envHandler);
    if (FAILED(hr)) {
        envHandler->Release();
    }
    return SUCCEEDED(hr);
}

void WebView2Bridge::shutdown() {
    if (m_webview) {
        m_webview->Release();
        m_webview = nullptr;
    }
    if (m_controller) {
        m_controller->Close();
        m_controller->Release();
        m_controller = nullptr;
    }
    if (m_webview2Loader) {
        FreeLibrary(m_webview2Loader);
        m_webview2Loader = nullptr;
    }
}

void WebView2Bridge::postMessage(const std::string& message) {
    if (m_webview) {
        std::wstring wmsg(message.begin(), message.end());
        m_webview->PostWebMessageAsString(wmsg.c_str());
    }
}

void WebView2Bridge::sendBinaryMessage(ipc::MessageType type, const void* data, size_t len) {
    if (!m_webview) return;

    // Construct packet: Header + Payload
    std::vector<uint8_t> packet(sizeof(ipc::RawrIPCHeader) + len);
    ipc::RawrIPCHeader* header = reinterpret_cast<ipc::RawrIPCHeader*>(packet.data());
    
    header->magic = ipc::RAWR_IPC_MAGIC;
    header->version = 1;
    header->msg_type = type;
    header->sequence = m_sequence++; // Incremented for every outgoing packet
    header->timestamp = GetTickCount64();
    header->payload_len = static_cast<uint32_t>(len);
    header->crc32 = 0; // TODO: Implement CRC32

    if (len > 0 && data) {
        memcpy(packet.data() + sizeof(ipc::RawrIPCHeader), data, len);
    }

    // This SDK exposes string/json post APIs on ICoreWebView2.
    // Bridge binary payload as hex JSON; the page can decode back to ArrayBuffer.
    const std::string hex = bytesToHex(packet.data(), packet.size());
    std::string json;
    json.reserve(hex.size() + 40);
    json += "{\"kind\":\"rawr-ipc-b16\",\"data\":\"";
    json += hex;
    json += "\"}";

    const std::wstring wjson(json.begin(), json.end());
    if (FAILED(m_webview->PostWebMessageAsJson(wjson.c_str()))) {
        // Fallback for runtimes that reject PostWebMessageAsJson payloads.
        m_webview->PostWebMessageAsString(wjson.c_str());
    }
}

extern "C" void rawrxd_enumerate_modules_peb(
    void (*callback)(uint64_t, uint32_t, uint16_t, const wchar_t*, void*), void* context);

void WebView2Bridge::snapshotModules() {
    auto callback = [](uint64_t base, uint32_t size, uint16_t nameLen, const wchar_t* namePtr, void* /*context*/) {
        // Construct MOD_LOAD packet
        struct {
            ipc::RawrIPCHeader header;
            ipc::MsgModuleLoad payload;
            wchar_t name[256]; // Temporary fixed buffer for name serialization
        } packet;

        size_t actualNameLen = (nameLen / 2) < 255 ? (nameLen / 2) : 255;
        wcsncpy(packet.name, namePtr, actualNameLen);
        packet.name[actualNameLen] = L'\0';

        packet.payload.base_address = base;
        packet.payload.size = size;
        packet.payload.path_len = static_cast<uint16_t>(actualNameLen * 2);

        WebView2Bridge::getInstance().sendBinaryMessage(
            ipc::MessageType::MOD_LOAD, 
            &packet.payload, 
            sizeof(ipc::MsgModuleLoad) + (actualNameLen * 2)
        );
    };

    rawrxd_enumerate_modules_peb(callback, nullptr);
}

void WebView2Bridge::onBinaryMessageFromUI(const uint8_t* buffer, size_t length) {
    if (!buffer || length < sizeof(ipc::RawrIPCHeader)) return;
    const auto* header = reinterpret_cast<const ipc::RawrIPCHeader*>(buffer);
    if (header->magic != ipc::RAWR_IPC_MAGIC) return;

    size_t payloadOffset = sizeof(ipc::RawrIPCHeader);
    // Some UI scripts still build a legacy 32-byte header (4-byte pad after crc32).
    if (length >= payloadOffset + 4) {
        const bool hasLegacyPad = (buffer[payloadOffset] == 0 && buffer[payloadOffset + 1] == 0 &&
                                   buffer[payloadOffset + 2] == 0 && buffer[payloadOffset + 3] == 0);
        if (hasLegacyPad && header->payload_len <= (length - (payloadOffset + 4))) {
            payloadOffset += 4;
        }
    }

    if (length < payloadOffset) return;
    const size_t availablePayload = length - payloadOffset;
    const size_t payloadLen = std::min<size_t>(header->payload_len, availablePayload);
    const uint8_t* payload = buffer + payloadOffset;
    auto& dbg = debug::DebuggerCore::getInstance();

    switch (header->msg_type) {
        case ipc::MessageType::REQ_READ_MEM: {
            if (payloadLen < sizeof(ipc::MsgReadMem)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgReadMem*>(payload);
            auto data = dbg.readMemory(msg->address, msg->size);

            struct DataMemPayload {
                uint64_t address;
                uint32_t size;
            } outMeta{msg->address, static_cast<uint32_t>(data.size())};

            std::vector<uint8_t> out(sizeof(outMeta) + data.size());
            memcpy(out.data(), &outMeta, sizeof(outMeta));
            if (!data.empty()) {
                memcpy(out.data() + sizeof(outMeta), data.data(), data.size());
            }
            sendBinaryMessage(ipc::MessageType::DATA_MEM, out.data(), out.size());
            break;
        }

        case ipc::MessageType::REQ_WRITE_MEM: {
            if (payloadLen < sizeof(ipc::MsgWriteMem)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgWriteMem*>(payload);
            if (payloadLen < sizeof(ipc::MsgWriteMem) + msg->size) break;
            const uint8_t* patchData = payload + sizeof(ipc::MsgWriteMem);
            std::vector<uint8_t> patch(patchData, patchData + msg->size);
            dbg.patchMemory(msg->address, patch);
            break;
        }

        case ipc::MessageType::REQ_WATCH: {
            if (payloadLen < sizeof(ipc::MsgWatchSymbol)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgWatchSymbol*>(payload);
            if (payloadLen < sizeof(ipc::MsgWatchSymbol) + msg->name_len) break;
            const char* nameRaw = reinterpret_cast<const char*>(payload + sizeof(ipc::MsgWatchSymbol));
            std::string name(nameRaw, nameRaw + msg->name_len);
            RawrXD_Debugger_AddWatch(msg->module_base, 0, name.c_str());
            break;
        }

        case ipc::MessageType::REQ_WATCH_ADD: {
            if (payloadLen < sizeof(ipc::MsgWatchAdd)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgWatchAdd*>(payload);
            if (payloadLen < sizeof(ipc::MsgWatchAdd) + msg->label_len) break;
            const char* labelRaw = reinterpret_cast<const char*>(payload + sizeof(ipc::MsgWatchAdd));
            std::string label(labelRaw, labelRaw + msg->label_len);

            uint64_t address = msg->address;
            if (address == 0 && !label.empty()) {
                address = dbg.resolveNameToAddress(label);
            }
            if (address != 0) {
                RawrXD_Debugger_AddWatch(address, msg->size ? msg->size : 8, label.c_str());
            }
            break;
        }

        case ipc::MessageType::REQ_WATCH_REMOVE: {
            if (payloadLen == sizeof(uint64_t)) {
                const uint64_t address = *reinterpret_cast<const uint64_t*>(payload);
                RawrXD_Debugger_RemoveWatch(address);
                break;
            }

            std::string token(reinterpret_cast<const char*>(payload), payloadLen);
            uint64_t address = 0;
            if (tryParseHexAddress(token, address) && address != 0) {
                RawrXD_Debugger_RemoveWatch(address);
            } else if (!token.empty()) {
                RawrXD_Debugger_RemoveWatchString(token.c_str());
            }
            break;
        }

        case ipc::MessageType::REQ_SYMBOL:
        case ipc::MessageType::REQ_RESOLVE_NAME: {
            if (payloadLen < sizeof(ipc::MsgResolveName)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgResolveName*>(payload);
            if (payloadLen < sizeof(ipc::MsgResolveName) + msg->name_len) break;
            const char* nameRaw = reinterpret_cast<const char*>(payload + sizeof(ipc::MsgResolveName));
            std::string name(nameRaw, nameRaw + msg->name_len);

            uint64_t resolved = 0;
            if (msg->module_base != 0) {
                resolved = dbg.resolveExport(msg->module_base, name);
            } else {
                resolved = dbg.resolveNameToAddress(name);
            }

            std::vector<uint8_t> out(sizeof(ipc::MsgResolveResult) + name.size());
            auto* res = reinterpret_cast<ipc::MsgResolveResult*>(out.data());
            res->address = resolved;
            res->name_len = static_cast<uint16_t>(name.size());
            memcpy(out.data() + sizeof(ipc::MsgResolveResult), name.data(), name.size());
            sendBinaryMessage(ipc::MessageType::DATA_RESOLVE_RESULT, out.data(), out.size());
            break;
        }

        case ipc::MessageType::REQ_EMIT_CODE: {
            if (payloadLen < sizeof(ipc::MsgEmitCode)) break;
            const auto* msg = reinterpret_cast<const ipc::MsgEmitCode*>(payload);
            if (payloadLen < sizeof(ipc::MsgEmitCode) + msg->source_text_len) break;
            const char* asmRaw = reinterpret_cast<const char*>(payload + sizeof(ipc::MsgEmitCode));
            std::string asmSource(asmRaw, asmRaw + msg->source_text_len);
            dbg.assembleAndInject(msg->target_address, asmSource);
            break;
        }

        default:
            break;
    }
}

void WebView2Bridge::onMessageFromUI(std::function<void(const std::string&)> callback) {
    m_messageHandler = callback;
}

} // namespace rawrxd::ui
