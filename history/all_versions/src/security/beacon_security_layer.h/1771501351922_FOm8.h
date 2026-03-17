#pragma once
#include "RawrXD_SignalSlot.h"
#include <wincrypt.h>
#pragma comment(lib, "crypt32.lib")

namespace RawrXD {
// Tamper-evident beacon packet
struct SecureBeaconPacket {
    uint32_t magic;      // 'RBWR'
    uint16_t type;       // Message type
    uint16_t length;     // Payload length
    uint8_t  payload[4080];
    uint8_t  hmac[32];   // SHA-256 HMAC
    uint64_t timestamp;  // Anti-replay
};

class BeaconSecurityLayer {
    HCRYPTPROV hProv;
    HCRYPTHASH hHash;
    Signal<const SecureBeaconPacket&> onBeaconValidated;
    Signal<const std::wstring&> onBeaconRejected;
    
public:
    BeaconSecurityLayer() : hProv(0), hHash(0) {
        CryptAcquireContextW(&hProv, nullptr, nullptr, PROV_RSA_AES, CRYPT_VERIFYCONTEXT);
    }
    
    bool ValidateBeacon(const SecureBeaconPacket& pkt, const std::vector<uint8_t>& sharedKey) {
        if(pkt.magic != 0x52425752) return false; // Wrong magic
        if(GetTickCount64() - pkt.timestamp > 30000) return false; // 30s replay window
        
        // Verify HMAC
        CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
        CryptHashData(hHash, (BYTE*)&pkt, offsetof(SecureBeaconPacket, hmac), 0);
        
        BYTE calcHash[32];
        DWORD hashLen = 32;
        CryptGetHashParam(hHash, HP_HASHVAL, calcHash, &hashLen, 0);
        CryptDestroyHash(hHash);
        
        if(memcmp(calcHash, pkt.hmac, 32) != 0) {
            onBeaconRejected.emit(L"HMAC verification failed");
            return false;
        }
        
        onBeaconValidated.emit(pkt);
        return true;
    }
    
    void SignBeacon(SecureBeaconPacket& pkt, const std::vector<uint8_t>& sharedKey) {
        pkt.magic = 0x52425752;
        pkt.timestamp = GetTickCount64();
        
        CryptCreateHash(hProv, CALG_SHA_256, 0, 0, &hHash);
        CryptHashData(hHash, (BYTE*)&pkt, offsetof(SecureBeaconPacket, hmac), 0);
        DWORD hashLen = 32;
        CryptGetHashParam(hHash, HP_HASHVAL, pkt.hmac, &hashLen, 0);
        CryptDestroyHash(hHash);
    }
};

// Global beacon security instance
inline BeaconSecurityLayer& GetBeaconSecurity() {
    static BeaconSecurityLayer bsl;
    return bsl;
}
} // namespace RawrXD
