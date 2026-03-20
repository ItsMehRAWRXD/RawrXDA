#include "../../include/gguf_d3d12_bridge.h"

namespace RawrXD {

GGUFD3D12Bridge::GGUFD3D12Bridge() {
    fenceEvent_ = nullptr;
    fenceValue_ = 0;
    fusedRecording_ = false;
    fusedOpsRecorded_ = 0;
}
GGUFD3D12Bridge::~GGUFD3D12Bridge() = default;

} // namespace RawrXD
