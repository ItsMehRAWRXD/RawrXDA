// =============================================================================
// SovereignReplication.cpp — Phase 55: TCP binary replication with hash verify
// =============================================================================
// Wire protocol (little-endian):
//   [MAGIC:8 = 0x52585250_4C435400 "RXRPLCT\0"]
//   [chunk_count:4]
//   for each chunk:
//     [chunk_size:4][chunk_data:N]
//   [fnv64_checksum:8]   (covers all preceding bytes)
//
// Peer negotiation:
//   Sender: connect → send frame → await "OK\n"
//   Receiver: accept → validate magic + checksum → send "OK\n" → return payload
// =============================================================================
#include "SovereignReplication.h"
#include "SovereignSnapshot.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#pragma comment(lib, "ws2_32.lib")

namespace RawrXD::Runtime {

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint64_t kMagic        = 0x52585250'4C435400ULL;
static constexpr uint16_t kDefaultPort  = 9006;
static constexpr DWORD    kTimeoutMs    = 10000;
static constexpr size_t   kChunkSize    = 16 * 1024;
static constexpr size_t   kMaxPayload   = 64 * 1024 * 1024;

// ---------------------------------------------------------------------------
// FNV-1a 64-bit
// ---------------------------------------------------------------------------
static uint64_t fnv1a64(const uint8_t* d, size_t n) {
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < n; ++i) h = (h ^ d[i]) * 0x100000001b3ULL;
    return h;
}

// ---------------------------------------------------------------------------
// Winsock RAII guard
// ---------------------------------------------------------------------------
struct WsaGuard {
    WSADATA data{};
    bool ok = false;
    WsaGuard() { ok = (WSAStartup(MAKEWORD(2, 2), &data) == 0); }
    ~WsaGuard()  { if (ok) WSACleanup(); }
};

// ---------------------------------------------------------------------------
// Wire helpers
// ---------------------------------------------------------------------------
static void appendU64(std::vector<uint8_t>& b, uint64_t v) {
    for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}
static void appendU32(std::vector<uint8_t>& b, uint32_t v) {
    for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>((v >> (i*8)) & 0xFF));
}

static std::vector<uint8_t> buildFrame(const std::vector<uint8_t>& payload) {
    const size_t sz = payload.size();
    uint32_t chunks = static_cast<uint32_t>((sz + kChunkSize - 1) / kChunkSize);
    std::vector<uint8_t> frame;
    frame.reserve(12 + sz + chunks*4 + 8);
    appendU64(frame, kMagic);
    appendU32(frame, chunks);
    size_t off = 0;
    for (uint32_t i = 0; i < chunks; ++i) {
        size_t len = std::min(kChunkSize, sz - off);
        appendU32(frame, static_cast<uint32_t>(len));
        frame.insert(frame.end(), payload.data() + off, payload.data() + off + len);
        off += len;
    }
    appendU64(frame, fnv1a64(frame.data(), frame.size()));
    return frame;
}

// ---------------------------------------------------------------------------
// Socket I/O helpers
// ---------------------------------------------------------------------------
static bool sendAll(SOCKET s, const uint8_t* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
        int n = send(s,
            reinterpret_cast<const char*>(data + sent),
            static_cast<int>(std::min(len - sent, size_t{65536})),
            0);
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

static bool recvAll(SOCKET s, uint8_t* buf, size_t len) {
    size_t done = 0;
    while (done < len) {
        int n = recv(s,
            reinterpret_cast<char*>(buf + done),
            static_cast<int>(std::min(len - done, size_t{65536})),
            0);
        if (n <= 0) return false;
        done += static_cast<size_t>(n);
    }
    return true;
}

// ---------------------------------------------------------------------------
// Singleton
// ---------------------------------------------------------------------------
SovereignReplication& SovereignReplication::instance() {
    static SovereignReplication inst;
    return inst;
}

// ---------------------------------------------------------------------------
// propagateTo — sends mesh snapshot to a remote listener
// ---------------------------------------------------------------------------
bool SovereignReplication::propagateTo(const std::string& host, uint16_t port) {
    if (host.empty() || host.size() > 253) return false;
    if (port == 0) port = kDefaultPort;

    std::vector<uint8_t> payload;
    SovereignSnapshot::instance().getLatestMeshSnapshot(payload);
    if (payload.size() > kMaxPayload) return false;

    WsaGuard wsa;
    if (!wsa.ok) return false;

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return false;

    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO,
        reinterpret_cast<const char*>(&kTimeoutMs), sizeof(kTimeoutMs));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&kTimeoutMs), sizeof(kTimeoutMs));

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (InetPtonA(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
        struct hostent* he = gethostbyname(host.c_str());
        if (!he) { closesocket(sock); return false; }
        std::memcpy(&addr.sin_addr, he->h_addr, sizeof(addr.sin_addr));
    }

    if (connect(sock, reinterpret_cast<const struct sockaddr*>(&addr),
                sizeof(addr)) != 0) {
        closesocket(sock); return false;
    }

    std::vector<uint8_t> frame = buildFrame(payload);
    bool sent = sendAll(sock, frame.data(), frame.size());

    bool ackOk = false;
    if (sent) {
        char ack[4]{};
        if (recv(sock, ack, 3, 0) == 3)
            ackOk = (ack[0]=='O' && ack[1]=='K' && ack[2]=='\n');
    }

    closesocket(sock);
    return ackOk;
}

// ---------------------------------------------------------------------------
// receiveFrom — accept one incoming replication frame on loopback
// ---------------------------------------------------------------------------
ReplicationResult SovereignReplication::receiveFrom(uint16_t listenPort,
                                                    uint32_t timeoutMs) {
    ReplicationResult result{};
    if (listenPort == 0) listenPort = kDefaultPort;

    WsaGuard wsa;
    if (!wsa.ok) return result;

    SOCKET srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (srv == INVALID_SOCKET) return result;

    struct sockaddr_in bind_addr{};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(listenPort);
    bind_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    BOOL reuse = TRUE;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    if (bind(srv, reinterpret_cast<const struct sockaddr*>(&bind_addr),
             sizeof(bind_addr)) != 0) {
        closesocket(srv); return result;
    }
    listen(srv, 1);

    DWORD tv = timeoutMs ? timeoutMs : kTimeoutMs;
    setsockopt(srv, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&tv), sizeof(tv));

    SOCKET client = accept(srv, nullptr, nullptr);
    closesocket(srv);
    if (client == INVALID_SOCKET) return result;

    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO,
        reinterpret_cast<const char*>(&tv), sizeof(tv));

    // Read header (magic[8] + chunk_count[4] = 12 bytes)
    uint8_t hdr[12]{};
    if (!recvAll(client, hdr, 12)) { closesocket(client); return result; }

    uint64_t magic = 0;
    for (int i = 0; i < 8; ++i) magic |= (uint64_t(hdr[i]) << (i*8));
    if (magic != kMagic) { closesocket(client); return result; }

    uint32_t chunkCount = 0;
    for (int i = 0; i < 4; ++i) chunkCount |= (uint32_t(hdr[8+i]) << (i*8));
    if (chunkCount > kMaxPayload / kChunkSize + 1) { closesocket(client); return result; }

    // Re-accumulate frame for checksum (include the header bytes)
    std::vector<uint8_t> frame(hdr, hdr + 12);
    std::vector<uint8_t> payload;
    for (uint32_t i = 0; i < chunkCount; ++i) {
        uint8_t szBuf[4]{};
        if (!recvAll(client, szBuf, 4)) { closesocket(client); return result; }
        uint32_t chunkLen = 0;
        for (int j = 0; j < 4; ++j) chunkLen |= (uint32_t(szBuf[j]) << (j*8));
        if (chunkLen > kMaxPayload) { closesocket(client); return result; }
        frame.insert(frame.end(), szBuf, szBuf + 4);
        size_t base = frame.size();
        frame.resize(base + chunkLen);
        if (!recvAll(client, frame.data() + base, chunkLen)) {
            closesocket(client); return result;
        }
        payload.insert(payload.end(),
            frame.data() + base, frame.data() + base + chunkLen);
    }

    // Read and validate checksum
    uint8_t csBuf[8]{};
    if (!recvAll(client, csBuf, 8)) { closesocket(client); return result; }
    uint64_t storedCs = 0;
    for (int i = 0; i < 8; ++i) storedCs |= (uint64_t(csBuf[i]) << (i*8));
    uint64_t computed = fnv1a64(frame.data(), frame.size());

    if (storedCs != computed) {
        send(client, "ER\n", 3, 0);
        closesocket(client);
        return result;
    }

    send(client, "OK\n", 3, 0);
    closesocket(client);

    result.success  = true;
    result.payload  = std::move(payload);
    result.checksum = storedCs;
    return result;
}

} // namespace RawrXD::Runtime
