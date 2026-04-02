// Out-of-line stubs so tests can link swarm_scheduler.cpp + RawrXDModelLoaderMemoryBackend
// without the full model loader TU (test uses SwarmScheduler with null backend only).
#include "rawrxd_model_loader.h"

void* RawrXDModelLoader::MapWindow(uint64_t, size_t)
{
    return reinterpret_cast<void*>(1);
}
void RawrXDModelLoader::UnmapWindow() {}
void RawrXDModelLoader::markComputeRangeInUse(std::uint64_t, std::uint64_t) {}
void RawrXDModelLoader::unmarkComputeRangeInUse(std::uint64_t, std::uint64_t) {}
void* RawrXDModelLoader::MapPrefetchWindow(uint64_t, size_t)
{
    return reinterpret_cast<void*>(2);
}
void RawrXDModelLoader::UnmapPrefetchWindow() {}
bool RawrXDModelLoader::HasActivePrefetchMapping() const
{
    return false;
}
bool RawrXDModelLoader::ComputeMappingCovers(uint64_t, uint64_t) const
{
    return false;
}
void RawrXDModelLoader::recordSwarmPinBackoffCycle() const {}
