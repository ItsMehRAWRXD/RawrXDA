#include "titan/titan_abi.h"
#include "titan/rx_kernel.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

int main() {
    constexpr const char* kMapName = "Local\\RawrXD.RxHotpath.Smoke";

    const uint64_t handle = Titan_CreateRxKernel(kMapName);
    if (handle == 0) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: Titan_CreateRxKernel returned 0\n");
        return 1;
    }

    void* channel_raw = nullptr;
    uint32_t status = Titan_GetRxSharedChannel(handle, &channel_raw);
    if (status != TITAN_STATUS_OK || channel_raw == nullptr) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: Titan_GetRxSharedChannel status=%u ptr=%p\n", status, channel_raw);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    auto* channel = static_cast<titan::RxChannel*>(channel_raw);

    // Verify direct alias calls are wired.
    status = Titan_RxSubmit(handle, "int answer = 42;");
    if (status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: Titan_RxSubmit status=%u\n", status);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    const uint32_t committed_alias = Titan_RxStep(handle);
    if (committed_alias == 0) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: Titan_RxStep committed 0 tokens\n");
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    // Validate command dispatch path using requested opcode aliases.
    const char* context = "for (int i = 0; i < n; ++i) { total += i; }";

    TitanAbiCommand submit_cmd{};
    submit_cmd.opcode = OP_RX_SUBMIT;
    submit_cmd.handle = handle;
    submit_cmd.ptr0 = reinterpret_cast<uint64_t>(context);

    TitanAbiResponse resp{};
    status = Titan_ExecuteCommand(&submit_cmd, &resp);
    if (status != TITAN_STATUS_OK || resp.status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: submit dispatch status=%u resp.status=%u\n", status, resp.status);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    TitanAbiCommand step_cmd{};
    step_cmd.opcode = OP_RX_STEP;
    step_cmd.handle = handle;

    status = Titan_ExecuteCommand(&step_cmd, &resp);
    if (status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: step dispatch status=%u\n", status);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    if (channel->gate.load(std::memory_order_acquire) != 1u) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: gate not committed\n");
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    // Speculative commit path: read all 8 tokens in a single block move.
    std::array<uint32_t, 8> committed{};
    std::array<float, 8> committed_conf{};
    status = Titan_RxReadDraftBlock(handle, committed.data(), committed_conf.data());
    if (status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: Titan_RxReadDraftBlock status=%u\n", status);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    TitanAbiCommand read_cmd{};
    read_cmd.opcode = TITAN_OP_RX_READ_DRAFT_BLOCK;
    read_cmd.handle = handle;
    read_cmd.ptr0 = reinterpret_cast<uint64_t>(committed.data());
    read_cmd.ptr1 = reinterpret_cast<uint64_t>(committed_conf.data());
    status = Titan_ExecuteCommand(&read_cmd, &resp);
    if (status != TITAN_STATUS_OK || resp.status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: read dispatch status=%u resp.status=%u\n", status, resp.status);
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    bool has_any = false;
    for (uint32_t token : committed) {
        if (token != 0u) {
            has_any = true;
            break;
        }
    }

    if (!has_any) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: all committed tokens are zero\n");
        (void)Titan_DestroyRxKernel(handle);
        return 1;
    }

    status = Titan_DestroyRxKernel(handle);
    if (status != TITAN_STATUS_OK) {
        std::fprintf(stderr, "[RX-HOTPATH] FAIL: destroy status=%u\n", status);
        return 1;
    }

    std::fprintf(stderr, "[RX-HOTPATH] PASS committed=%u first=%u\n", resp.value, committed[0]);
    return 0;
}
