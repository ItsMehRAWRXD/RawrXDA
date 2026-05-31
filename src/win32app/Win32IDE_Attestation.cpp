#include "Win32IDE.h"
#include "../security/SovereignAttestation.h"
#include <nlohmann/json.hpp>

using namespace RawrXD::Security;

void Win32IDE::initializeSovereignAttestation() {
    auto& attest = SovereignAttestation::Instance();
    if (!attest.Initialize()) {
        LOG_WARNING("SovereignAttestation failed to initialize");
        return;
    }
    
    // Log IDE startup
    attest.RecordEvent("ide_startup", nlohmann::json{
        {"version", "1.0.0"},
        {"build", "turnkey_RC1"},
        {"deviceId", attest.GenerateProof().deviceId}
    }.dump());
    
    LOG_INFO("SovereignAttestation active (NIST 800-218)");
}

void Win32IDE::recordModelLoadAttestation(const std::string& modelPath, const void* mappedBase, size_t size) {
    auto& attest = SovereignAttestation::Instance();
    attest.UpdateVramIntegrity(modelPath, mappedBase, size);
    
    attest.RecordEvent("model_load", nlohmann::json{
        {"path", modelPath},
        {"size", size},
        {"timestamp", std::time(nullptr)}
    }.dump());
    
    LOG_INFO("Attestation: Recorded model load for " + modelPath);
}

void Win32IDE::exportAttestationReport() {
    auto& attest = SovereignAttestation::Instance();
    auto proof = attest.GenerateProof(AttestationLevel::Full);
    std::string jsonStr = attest.ExportToJson(proof);
    
    // Save to workspace root
    std::string path = m_projectRoot + "\\.rawrxd\\attestation_proof.json";
    std::filesystem::create_directories(m_projectRoot + "\\.rawrxd");
    
    std::ofstream f(path);
    if (f.is_open()) {
        f << jsonStr;
        f.close();
        
        // Copy to clipboard
        if (OpenClipboard(m_hwndMain)) {
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, jsonStr.size() + 1);
            if (hMem) {
                memcpy(GlobalLock(hMem), jsonStr.c_str(), jsonStr.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
        }
        
        MessageBoxA(m_hwndMain, ("Attestation proof exported to " + path + " and copied to clipboard").c_str(), 
                    "Sovereign Attestation", MB_OK | MB_ICONINFORMATION);
    }
}
