#pragma once

#include <cstdint>

namespace RawrXD::ExtensionKernel {

static constexpr uint32_t kAutocompleteWireVersion = 1;
static constexpr uint32_t kAutocompleteFlagSyntaxNoise = 0x1;
static constexpr uint32_t kAutocompleteFlagContextChurn = 0x2;
static constexpr uint32_t kAutocompleteFlagColdStart = 0x4;
static constexpr uint32_t kAutocompleteFlagFileSwitch = 0x8;

struct CompletionRequest {
    uint32_t version = kAutocompleteWireVersion;
    uint32_t line = 0;
    uint32_t col = 0;
    uint32_t flags = 0;
    char filePath[320] = {0};
    char content[2048] = {0};
    char prefix[768] = {0};
    char suffix[768] = {0};
};

struct CompletionResult {
    uint32_t version = kAutocompleteWireVersion;
    uint32_t count = 0;
    float acceptanceRate = 0.0f;
    float speedupEstimate = 1.0f;
    uint32_t cacheHit = 0;
    uint32_t kvStitchCount = 0;
    uint32_t tokensGenerated = 0;
    uint32_t tokensAccepted = 0;
    uint32_t verifyRejects = 0;
    uint32_t specDepth = 0;
    uint32_t specHeads = 0;
    uint32_t specHeadsPruned = 0;
    int32_t tokens[32] = {0};
    char text[1536] = {0};
};

} // namespace RawrXD::ExtensionKernel
