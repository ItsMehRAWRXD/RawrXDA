// Companion extension host smoke client — production MessageSegmenter + ExtensionHostIpcBridge.
#include "../win32app/ExtensionHostIpcBridge.hpp"

#include <cstring>
#include <string>

#include <windows.h>

namespace
{

constexpr wchar_t kSmokePingFlag[] = L"--smoke-ping-test";
constexpr wchar_t kConnectSmokeFlag[] = L"--connect-smoke";
constexpr wchar_t kClientPingFlag[] = L"--smoke-ipc-client-ping";
constexpr wchar_t kFullSmokeFlag[] = L"--full-smoke";
constexpr wchar_t kPhaseAbFlag[] = L"--phase-ab";
constexpr wchar_t kConnectIdeSmokeFlag[] = L"--connect-ide-smoke";

bool hasArg(int argc, wchar_t** argv, const wchar_t* flag)
{
    for (int i = 1; i < argc; ++i)
    {
        if (argv[i] && wcscmp(argv[i], flag) == 0)
        {
            return true;
        }
    }
    return false;
}

bool smokeModeRequested(int argc, wchar_t** argv)
{
    if (hasArg(argc, argv, kSmokePingFlag) || hasArg(argc, argv, kConnectSmokeFlag) ||
        hasArg(argc, argv, kClientPingFlag) || hasArg(argc, argv, kConnectIdeSmokeFlag))
    {
        return true;
    }

    char mode[64] = {};
    if (GetEnvironmentVariableA("RAWRXD_EXTENSION_HOST_MODE", mode, sizeof(mode)) > 0)
    {
        return strcmp(mode, "CLIENT_SMOKE") == 0;
    }
    return false;
}

bool fullSmokePhasesRequested(int argc, wchar_t** argv)
{
    return hasArg(argc, argv, kFullSmokeFlag) || hasArg(argc, argv, kPhaseAbFlag) ||
           (GetEnvironmentVariableA("RAWRXD_SMOKE_IPC_FULL", nullptr, 0) > 0);
}

bool runFullSmokePhases(RawrXD::ExtensionHost::ExtensionHostIpcBridge& bridge)
{
    const uint8_t legacyPayload[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04};
    if (!bridge.WriteMessage(0x0101, legacyPayload, sizeof(legacyPayload)))
    {
        return false;
    }

    std::string oversized(131072, static_cast<char>(0xA5));
    oversized.front() = static_cast<char>(0xAA);
    oversized.back() = static_cast<char>(0x55);
    if (!bridge.WriteMessage(0x0102, reinterpret_cast<const uint8_t*>(oversized.data()),
                             static_cast<uint32_t>(oversized.size())))
    {
        return false;
    }

    Sleep(250);
    return true;
}

}  // namespace

int wmain(int argc, wchar_t** argv)
{
    if (!smokeModeRequested(argc, argv))
    {
        return 2;
    }

    RawrXD::ExtensionHost::ExtensionHostIpcBridge bridge;
    if (!bridge.ConnectAsClient(nullptr, 15000))
    {
        return 101;
    }

    if (fullSmokePhasesRequested(argc, argv))
    {
        if (!runFullSmokePhases(bridge))
        {
            return 103;
        }
        return 0;
    }

    const std::string payload = "PING_LIVE_TRAFFIC_TEST";
    if (!bridge.WriteLogicalMessage(RawrXD::IPC::Chat::ChatMessageType::ChatMessage, payload))
    {
        return 102;
    }

    return 0;
}
