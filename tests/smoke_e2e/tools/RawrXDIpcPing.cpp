// Minimal extension-host client: sends one production-framed legacy chat frame.
#include "Win32IDE_ChatIpcProtocol.h"

#include <cstdint>
#include <string>
#include <vector>

#include <windows.h>

namespace
{

constexpr wchar_t kPipeName[] = L"\\\\.\\pipe\\RawrXDExtensionPipe";

}  // namespace

int main()
{
    if (!WaitNamedPipeW(kPipeName, 5000))
    {
        return 101;
    }

    HANDLE pipe = CreateFileW(kPipeName, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return 102;
    }

    DWORD mode = PIPE_READMODE_BYTE;
    SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr);

    const std::string payload = "PING_LIVE_TRAFFIC_TEST";
    RawrXD::IPC::Chat::MessageSegmenter segmenter;
    if (!segmenter.Begin(RawrXD::IPC::Chat::ChatMessageType::ChatMessage, payload))
    {
        CloseHandle(pipe);
        return 103;
    }

    std::vector<uint8_t> prefixed;
    if (!segmenter.NextPrefixedWireBlob(prefixed) || prefixed.empty())
    {
        CloseHandle(pipe);
        return 104;
    }

    DWORD written = 0;
    const BOOL ok = WriteFile(pipe, prefixed.data(), static_cast<DWORD>(prefixed.size()), &written, nullptr);
    FlushFileBuffers(pipe);
    CloseHandle(pipe);

    if (!ok || written != prefixed.size())
    {
        return 105;
    }

    return 0;
}
