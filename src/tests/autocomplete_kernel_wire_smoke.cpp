#include "ipc/shm_channel.hpp"
#include "extension_kernel/autocomplete_protocol.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <immintrin.h>

int main() {
    constexpr const char* kBase = "RawrXD_Autocomplete_Smoke";

    RawrXD::IPC::ShmBiChannel server;
    RawrXD::IPC::ShmBiChannel client;

    if (!server.open_server(kBase, 64)) return 1;
    if (!client.open_client(kBase, 64)) return 2;

    RawrXD::ExtensionKernel::CompletionRequest req{};
    req.version = RawrXD::ExtensionKernel::kAutocompleteWireVersion;
    req.line = 4;
    req.col = 12;
    std::strcpy(req.filePath, "d:/rawrxd/src/demo.cpp");
    std::strcpy(req.prefix, "ret");

    if (!client.send(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&req), sizeof(req)))) {
        return 3;
    }

    std::vector<uint8_t> inbound;
    for (int i = 0; i < 5000; ++i) {
        if (server.rx().read_copy(inbound)) break;
        _mm_pause();
    }
    if (inbound.size() < sizeof(req)) return 4;

    RawrXD::ExtensionKernel::CompletionRequest seen{};
    std::memcpy(&seen, inbound.data(), sizeof(seen));
    if (seen.version != RawrXD::ExtensionKernel::kAutocompleteWireVersion) return 5;
    if (seen.line != 4 || seen.col != 12) return 6;

    RawrXD::ExtensionKernel::CompletionResult res{};
    res.version = RawrXD::ExtensionKernel::kAutocompleteWireVersion;
    res.count = 2;
    res.tokens[0] = 42;
    res.tokens[1] = 43;
    std::strcpy(res.text, "return");

    if (!server.send(std::span<const uint8_t>(
            reinterpret_cast<const uint8_t*>(&res), sizeof(res)))) {
        return 7;
    }

    std::vector<uint8_t> back;
    for (int i = 0; i < 5000; ++i) {
        if (client.rx().read_copy(back)) break;
        _mm_pause();
    }
    if (back.size() < sizeof(res)) return 8;

    RawrXD::ExtensionKernel::CompletionResult seenRes{};
    std::memcpy(&seenRes, back.data(), sizeof(seenRes));
    if (seenRes.version != RawrXD::ExtensionKernel::kAutocompleteWireVersion) return 9;
    if (std::string(seenRes.text) != "return") return 10;

    return 0;
}
