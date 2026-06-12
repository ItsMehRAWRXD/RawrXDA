/**
 * Minimal stubs for api_server.cpp dependencies — enables standalone stress testing
 * without linking the full HTTP server / interactive shell / MMF infrastructure.
 */
#include <string>
#include <string>
#include <functional>

// Stub OverclockGovernor destructor
OverclockGovernor::~OverclockGovernor() {}
void OverclockGovernor::Stop() {}

// Stub HTTP server
namespace RawrXD {
    bool InitializeHttpServer(uint16_t) { return true; }
    void RegisterHttpRoute(const std::string&, const std::string&,
                           std::function<std::string(const std::string&)>) {}
}

// Stub InteractiveShell
std::string InteractiveShell::ExecuteCommand(const std::string& input) { return input; }

// Stub RawrXDStateMmf
RawrXDStateMmf& RawrXDStateMmf::instance() {
    static RawrXDStateMmf inst;
    return inst;
}
bool RawrXDStateMmf::isInitialized() const { return false; }
PatchResult RawrXDStateMmf::initialize(unsigned char, const char*) {
    PatchResult r; r.success = true; return r;
}
PatchResult RawrXDStateMmf::publishModelState(const MmfModelState&) {
    PatchResult r; r.success = true; return r;
}
MmfModelState RawrXDStateMmf::readModelState() const { return MmfModelState{}; }
MmfMemoryStats RawrXDStateMmf::readMemoryStats() const { return MmfMemoryStats{}; }
PatchResult RawrXDStateMmf::broadcastEvent(unsigned char, const char*) {
    PatchResult r; r.success = true; return r;
}
uint64_t RawrXDStateMmf::pollEvents(MmfEvent*, uint64_t, uint64_t*) const { return 0; }
