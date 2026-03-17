// ============================================================================
// main.cpp — RawrXD Enterprise License Creator CLI
// ============================================================================
// Command-line tool for generating .rawrlic license files.
//
// Usage:
//   RawrXD_LicenseCreator.exe --generate-keypair
//   RawrXD_LicenseCreator.exe --create-license --tier enterprise --output license.rawrlic
//   RawrXD_LicenseCreator.exe --verify license.rawrlic
//   RawrXD_LicenseCreator.exe --show-hwid
//
// Build: cl.exe main.cpp license_generator.cpp /EHsc /std:c++20 /Fe:RawrXD_LicenseCreator.exe advapi32.lib
// ============================================================================

#include "license_generator.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <cstdlib>

using namespace RawrXD::LicenseCreator;

// ============================================================================
// CLI Argument Parser
// ============================================================================
class ArgParser {
public:
    ArgParser(int argc, char** argv) {
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.starts_with("--")) {
                std::string key = arg.substr(2);
                if (i + 1 < argc && !std::string(argv[i + 1]).starts_with("--")) {
                    m_args[key] = argv[i + 1];
                    ++i;
                } else {
                    m_flags.insert(key);
                }
            }
        }
    }

    bool hasFlag(const std::string& flag) const {
        return m_flags.count(flag) > 0;
    }

    std::string get(const std::string& key, const std::string& defaultVal = "") const {
        auto it = m_args.find(key);
        return (it != m_args.end()) ? it->second : defaultVal;
    }

    int getInt(const std::string& key, int defaultVal = 0) const {
        auto val = get(key);
        return val.empty() ? defaultVal : std::atoi(val.c_str());
    }

    uint64_t getUInt64(const std::string& key, uint64_t defaultVal = 0) const {
        auto val = get(key);
        return val.empty() ? defaultVal : std::stoull(val, nullptr, 0);
    }

private:
    std::map<std::string, std::string> m_args;
    std::set<std::string> m_flags;
};

// ============================================================================
// Command Handlers
// ============================================================================

void showUsage() {
    std::cout << R"(
RawrXD Enterprise License Creator v1.0
======================================

COMMANDS:
  --generate-keypair              Generate new RSA-4096 keypair
      --output <prefix>           Output file prefix (default: rawrxd_rsa)
                                  Creates: <prefix>_public.blob, <prefix>_private.blob

  --create-license                Create a new license file
      --tier <tier>               License tier: community, trial, pro, enterprise, oem
      --output <file>             Output .rawrlic file path
      --expiry-days <N>           License expiry in days (0 = perpetual, default)
      --hwid <hash>               Bind to specific HWID (0 = floating, default)
      --seats <N>                 Concurrent seat count (default: 1)
      --private-key <file>        Private key blob file (default: rawrxd_rsa_private.blob)
      --public-key <file>         Public key blob file (default: rawrxd_rsa_public.blob)
      --customer-name <name>      Customer name (for reference)
      --customer-email <email>    Customer email (for reference)

  --verify <file>                 Verify a license file
      --hwid <hash>               Expected HWID (optional)
      --public-key <file>         Public key blob file (default: rawrxd_rsa_public.blob)

  --show-hwid                     Display current machine HWID

  --embed-public-key              Generate ASM code for embedding public key
      --public-key <file>         Public key blob file
      --output <file>             Output .asm include file

OPTIONS:
  --help                          Show this help message

EXAMPLES:
  # Generate production keypair
  RawrXD_LicenseCreator.exe --generate-keypair --output production

  # Create perpetual enterprise license (floating)
  RawrXD_LicenseCreator.exe --create-license --tier enterprise --output ent.rawrlic

  # Create 30-day trial license bound to this machine
  RawrXD_LicenseCreator.exe --create-license --tier trial --expiry-days 30 --hwid 0x$(RawrXD_LicenseCreator.exe --show-hwid) --output trial.rawrlic

  # Verify license
  RawrXD_LicenseCreator.exe --verify ent.rawrlic

LICENSE TIERS:
  Community    - Free (70B models, 32K context, no enterprise features)
  Trial        - 30-day trial (180B, 128K context, limited features)
  Professional - Paid (400B, 128K context, Pro features: 0x4A)
  Enterprise   - Full (800B, 200K context, all features: 0xFF)
  OEM          - Partner (800B, 200K context, all features: 0xFF)

)" << std::endl;
}

bool cmdGenerateKeyPair(const ArgParser& args) {
    std::string prefix = args.get("output", "rawrxd_rsa");

    LicenseGenerator gen;
    std::cout << "[*] Generating RSA-4096 keypair (this may take 10-30 seconds)..." << std::endl;

    if (!gen.GenerateKeyPair(4096)) {
        std::cerr << "[!] Failed to generate keypair: " << gen.GetLastError() << std::endl;
        return false;
    }

    std::string pubPath = prefix + "_public.blob";
    std::string privPath = prefix + "_private.blob";

    if (!gen.SaveKeyPair(pubPath, privPath)) {
        std::cerr << "[!] Failed to save keypair" << std::endl;
        return false;
    }

    std::cout << "[+] Keypair generated successfully!" << std::endl;
    std::cout << "    Public key:  " << pubPath << std::endl;
    std::cout << "    Private key: " << privPath << " (KEEP THIS SECRET!!!)" << std::endl;
    std::cout << std::endl;
    std::cout << "[!] WARNING: Store the private key in a secure location." << std::endl;
    std::cout << "[!] Anyone with the private key can create valid licenses." << std::endl;

    return true;
}

bool cmdCreateLicense(const ArgParser& args) {
    std::string tierStr = args.get("tier", "community");
    std::string outputPath = args.get("output", "license.rawrlic");
    std::string pubKeyPath = args.get("public-key", "rawrxd_rsa_public.blob");
    std::string privKeyPath = args.get("private-key", "rawrxd_rsa_private.blob");

    // Parse tier
    LicenseTier tier = LicenseTier::Community;
    if (tierStr == "trial") tier = LicenseTier::Trial;
    else if (tierStr == "pro" || tierStr == "professional") tier = LicenseTier::Professional;
    else if (tierStr == "enterprise" || tierStr == "ent") tier = LicenseTier::Enterprise;
    else if (tierStr == "oem") tier = LicenseTier::OEM;
    else if (tierStr != "community") {
        std::cerr << "[!] Invalid tier: " << tierStr << std::endl;
        return false;
    }

    // Build config
    LicenseConfig config;
    config.tier = tier;
    config.expiryDays = args.getUInt64("expiry-days", 0);
    config.hardwareHash = args.getUInt64("hwid", 0);
    config.seatCount = static_cast<uint16_t>(args.getInt("seats", 1));
    config.customerName = args.get("customer-name", "");
    config.customerEmail = args.get("customer-email", "");

    // Load keypair
    LicenseGenerator gen;
    if (!gen.LoadKeyPair(pubKeyPath, privKeyPath)) {
        std::cerr << "[!] Failed to load keypair from:" << std::endl;
        std::cerr << "    Public:  " << pubKeyPath << std::endl;
        std::cerr << "    Private: " << privKeyPath << std::endl;
        std::cerr << "[!] Run --generate-keypair first, or specify --private-key and --public-key" << std::endl;
        return false;
    }

    // Generate license
    std::cout << "[*] Creating " << LicenseGenerator::GetTierName(tier) << " license..." << std::endl;
    std::cout << "    Output: " << outputPath << std::endl;
    std::cout << "    Expiry: " << (config.expiryDays == 0 ? "Perpetual" : std::to_string(config.expiryDays) + " days") << std::endl;
    std::cout << "    HWID:   " << (config.hardwareHash == 0 ? "Floating (any machine)" : "0x" + std::to_string(config.hardwareHash)) << std::endl;
    std::cout << "    Seats:  " << config.seatCount << std::endl;

    if (!gen.GenerateLicense(config, outputPath)) {
        std::cerr << "[!] Failed to generate license: " << gen.GetLastError() << std::endl;
        return false;
    }

    std::cout << "[+] License created successfully: " << outputPath << std::endl;
    std::cout << "    Features: 0x" << std::hex << LicenseGenerator::GetFeatureMaskForTier(tier) << std::dec << std::endl;

    return true;
}

bool cmdVerifyLicense(const ArgParser& args, const std::string& licensePath) {
    std::string pubKeyPath = args.get("public-key", "rawrxd_rsa_public.blob");
    uint64_t hwid = args.getUInt64("hwid", 0);

    LicenseGenerator gen;
    if (!gen.LoadKeyPair(pubKeyPath, "")) {
        std::cerr << "[!] Failed to load public key: " << pubKeyPath << std::endl;
        return false;
    }

    std::cout << "[*] Verifying license: " << licensePath << std::endl;

    if (!gen.VerifyLicense(licensePath, hwid)) {
        std::cerr << "[!] License verification FAILED: " << gen.GetLastError() << std::endl;
        return false;
    }

    std::cout << "[+] License verification PASSED" << std::endl;
    return true;
}

bool cmdShowHWID(const ArgParser& args) {
    uint64_t hwid = LicenseGenerator::GetCurrentHardwareHash();
    std::cout << "Current Machine HWID: 0x" << std::hex << hwid << std::dec << std::endl;
    std::cout << "                      " << hwid << " (decimal)" << std::endl;
    std::cout << std::endl;
    std::cout << "Use this value with --hwid to bind licenses to this machine." << std::endl;
    return true;
}

bool cmdEmbedPublicKey(const ArgParser& args) {
    std::string pubKeyPath = args.get("public-key", "rawrxd_rsa_public.blob");
    std::string outputPath = args.get("output", "embedded_public_key.inc");

    LicenseGenerator gen;
    if (!gen.LoadKeyPair(pubKeyPath, "")) {
        std::cerr << "[!] Failed to load public key: " << pubKeyPath << std::endl;
        return false;
    }

    auto blob = gen.GetPublicKeyBlob();
    if (blob.empty()) {
        std::cerr << "[!] No public key loaded" << std::endl;
        return false;
    }

    // Write ASM include file
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        std::cerr << "[!] Failed to open output: " << outputPath << std::endl;
        return false;
    }

    out << "; Generated by RawrXD_LicenseCreator.exe\n";
    out << "; Embedded RSA-4096 Public Key for License Verification\n";
    out << "; DO NOT EDIT — Regenerate with --embed-public-key\n\n";
    out << "RSA_PUBLIC_KEY_BLOB LABEL BYTE\n";

    for (size_t i = 0; i < blob.size(); ++i) {
        if (i % 16 == 0) {
            out << "    DB  ";
        }
        out << "0" << std::hex << static_cast<int>(blob[i]) << "h";
        if (i + 1 < blob.size()) {
            out << ((i % 16 == 15) ? "\n" : ", ");
        }
    }

    out << "\nRSA_PUBLIC_KEY_SIZE EQU $ - RSA_PUBLIC_KEY_BLOB\n";
    out.close();

    std::cout << "[+] Embedded public key written to: " << outputPath << std::endl;
    std::cout << "    Include this in RawrXD_EnterpriseLicense.asm" << std::endl;

    return true;
}

// ============================================================================
// Main Entry Point
// ============================================================================
int main(int argc, char** argv) {
    std::cout << "RawrXD Enterprise License Creator v1.0\n";
    std::cout << "======================================\n\n";

    if (argc < 2) {
        showUsage();
        return 1;
    }

    ArgParser args(argc, argv);

    if (args.hasFlag("help")) {
        showUsage();
        return 0;
    }

    try {
        if (args.hasFlag("generate-keypair")) {
            return cmdGenerateKeyPair(args) ? 0 : 1;
        }

        if (args.hasFlag("create-license")) {
            return cmdCreateLicense(args) ? 0 : 1;
        }

        if (args.hasFlag("verify")) {
            std::string licensePath = args.get("verify");
            if (licensePath.empty()) {
                std::cerr << "[!] Missing license file path for --verify" << std::endl;
                return 1;
            }
            return cmdVerifyLicense(args, licensePath) ? 0 : 1;
        }

        if (args.hasFlag("show-hwid")) {
            return cmdShowHWID(args) ? 0 : 1;
        }

        if (args.hasFlag("embed-public-key")) {
            return cmdEmbedPublicKey(args) ? 0 : 1;
        }

        std::cerr << "[!] Unknown command. Use --help for usage." << std::endl;
        return 1;

    } catch (const std::exception& e) {
        std::cerr << "[!] Exception: " << e.what() << std::endl;
        return 1;
    }
}
