#include "Win32IDE.h"
#include "../../include/plugin_signature.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::wstring> collectPluginFiles(const std::filesystem::path& root) {
    std::vector<std::wstring> out;
    std::error_code ec;
    if (!std::filesystem::exists(root, ec) || !std::filesystem::is_directory(root, ec)) {
        return out;
    }

    for (const auto& entry : std::filesystem::directory_iterator(root, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        std::wstring ext = entry.path().extension().wstring();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
        if (ext == L".dll" || ext == L".vsix" || ext == L".rawrpkg") {
            out.push_back(entry.path().wstring());
        }
    }
    return out;
}

}  // namespace

void HandlePluginSignature(void* idePtr) {
    Win32IDE* ide = static_cast<Win32IDE*>(idePtr);
    if (!ide) {
        return;
    }

    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (!verifier.isInitialized()) {
        if (!verifier.initialize(RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy())) {
            MessageBoxA(ide->getMainWindow(), "Failed to initialize plugin signature verifier.", "Plugin Signature", MB_ICONERROR | MB_OK);
            return;
        }
    }

    const std::filesystem::path pluginDir = std::filesystem::path("plugins");
    const auto files = collectPluginFiles(pluginDir);

    size_t valid = 0;
    size_t invalid = 0;
    size_t blocked = 0;
    size_t noSignature = 0;

    std::ostringstream report;
    report << "[PluginSignature] Scan root: " << pluginDir.string() << "\n";

    for (const auto& file : files) {
        const auto result = verifier.verify(file.c_str());
        const bool allowed = verifier.shouldAllowInstall(result);

        if (result.valid) {
            ++valid;
            report << "  VALID   " << std::filesystem::path(file).string() << "\n";
        } else if (result.status == RawrXD::Plugin::SignatureStatus::NoSignature) {
            ++noSignature;
            if (!allowed) {
                ++blocked;
            }
            report << "  UNSIGNED " << (allowed ? "ALLOW " : "BLOCK ") << std::filesystem::path(file).string() << "\n";
        } else {
            ++invalid;
            if (!allowed) {
                ++blocked;
            }
            report << "  INVALID " << (allowed ? "ALLOW " : "BLOCK ") << std::filesystem::path(file).string();
            if (result.detail) {
                report << " (" << result.detail << ")";
            }
            report << "\n";
        }
    }

    report << "[PluginSignature] Summary: files=" << files.size()
           << " valid=" << valid
           << " unsigned=" << noSignature
           << " invalid=" << invalid
           << " blocked=" << blocked << "\n";

    ide->appendToOutput(report.str(), "Security", Win32IDE::OutputSeverity::Info);

    MessageBoxA(ide->getMainWindow(),
                (std::string("Plugin signature scan complete.\nFiles: ") + std::to_string(files.size()) +
                 "\nValid: " + std::to_string(valid) +
                 "\nInvalid: " + std::to_string(invalid) +
                 "\nBlocked: " + std::to_string(blocked)).c_str(),
                "Plugin Signature",
                MB_OK | ((blocked > 0 || invalid > 0) ? MB_ICONWARNING : MB_ICONINFORMATION));
}

void Win32IDE::initPluginSignatureVerifier() {
    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (verifier.isInitialized()) {
        return;
    }

    auto policy = RawrXD::Plugin::PluginSignatureVerifier::createStandardPolicy();
    const char* strict = std::getenv("RAWRXD_AIRGAPPED");
    if (strict && std::string(strict) == "1") {
        policy = RawrXD::Plugin::PluginSignatureVerifier::createStrictPolicy();
    }

    if (!verifier.initialize(policy)) {
        LOG_ERROR("[PluginSignature] failed to initialize verifier");
    }
}

bool Win32IDE::verifyPluginBeforeLoad(const wchar_t* pluginPath) {
    if (!pluginPath) {
        return false;
    }

    auto& verifier = RawrXD::Plugin::PluginSignatureVerifier::instance();
    if (!verifier.isInitialized()) {
        initPluginSignatureVerifier();
    }

    const auto result = verifier.verify(pluginPath);
    return verifier.shouldAllowInstall(result);
}
