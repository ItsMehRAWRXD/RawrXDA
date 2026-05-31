#pragma once
// =============================================================================
// SovereignReplication.h — Phase 55: TCP binary replication client/server
// =============================================================================
#include <cstdint>
#include <string>
#include <vector>

namespace RawrXD::Runtime {

struct ReplicationResult {
    bool                  success  = false;
    std::vector<uint8_t>  payload; ///< Received mesh snapshot bytes
    uint64_t              checksum = 0;
};

class SovereignReplication {
public:
    static SovereignReplication& instance();

    /// Send current mesh snapshot to a remote host:port over TCP.
    /// Returns true when the remote sends back "OK\n" ACK.
    bool propagateTo(const std::string& host, uint16_t port = 9006);

    /// Listen on loopback:listenPort for one incoming replication frame.
    /// Validates magic header + FNV-64 checksum, then sends ACK.
    ReplicationResult receiveFrom(uint16_t listenPort = 9006,
                                  uint32_t timeoutMs  = 10000);

private:
    SovereignReplication() = default;
};

} // namespace RawrXD::Runtime
