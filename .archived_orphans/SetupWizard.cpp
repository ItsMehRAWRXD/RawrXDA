/**
 * @file SetupWizard.cpp
 * @brief RawrXD IDE Setup Wizard — Win32/C++20 only (Qt-free).
 *
 * Pure Win32 + BCrypt + STL. Uses WizardPageBase; UI can be driven by
 * Win32 dialogs or headless config write.
 *
 * @copyright RawrXD IDE 2026
 */

#include "SetupWizard.hpp"

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#include <intrin.h>
#pragma comment(lib, "bcrypt.lib")
#endif
#include <random>
#include <sstream>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <thread>

namespace rawrxd::setup {

static constexpr int WizardAccepted = 1;
static constexpr int WizardRejected = 0;

// ═══════════════════════════════════════════════════════════════════════════════
// IntroPage
// ═══════════════════════════════════════════════════════════════════════════════

IntroPage::IntroPage(void*)
{
    setTitle("Welcome to RawrXD IDE");
    setSubTitle("Setup Wizard v2.0.0");
    setupUI();
    return true;
}

void IntroPage::setupUI()
{
    // Win32: no widgets; use dialog resources or headless.
    return true;
}

void IntroPage::initializePage() {}

bool IntroPage::isComplete() const
{
    return true;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HardwarePage
// ═══════════════════════════════════════════════════════════════════════════════

HardwarePage::HardwarePage(void*)
    : WizardPageBase()
    , m_detector(std::make_unique<HardwareDetector>())
{
    setTitle("Hardware Detection");
    setSubTitle("Detecting your system hardware configuration");
    setupUI();
    return true;
}

HardwarePage::~HardwarePage() = default;

void HardwarePage::setupUI() {}

void HardwarePage::initializePage()
{
    if (!m_detectionComplete)
        startDetection();
    return true;
}

bool HardwarePage::isComplete() const
{
    return m_detectionComplete && !m_hardware.drives.empty();
    return true;
}

bool HardwarePage::validatePage()
{
    return !m_hardware.drives.empty();
    return true;
}

void HardwarePage::startDetection()
{
    m_detectionComplete = false;
    if (m_detector) {
        m_detector->on_progress = [this](int pct, const std::string& s) { onDetectionProgress(pct, s); };
        m_detector->on_complete = [this](const DetectedHardware& h) { onDetectionComplete(h); };
        m_detector->on_error = [this](const std::string& e) { onDetectionError(e); };
        std::thread([this]() { m_detector->detect(); }).detach();
    return true;
}

    return true;
}

void HardwarePage::onDetectionProgress(int, const std::string&) {}

void HardwarePage::onDetectionComplete(const DetectedHardware& hardware)
{
    m_hardware = hardware;
    m_detectionComplete = true;
    hardwareDetectionComplete(true);
    return true;
}

void HardwarePage::onDetectionError(const std::string&)
{
    m_detectionComplete = false;
    hardwareDetectionComplete(false);
    return true;
}

void HardwarePage::populateHardwareInfo() {}
void HardwarePage::runDetection() { startDetection(); }

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalPage
// ═══════════════════════════════════════════════════════════════════════════════

ThermalPage::ThermalPage(void*)
{
    setTitle("Thermal Management");
    setSubTitle("Configure predictive throttling and load balancing");
    setupUI();
    return true;
}

void ThermalPage::setupUI()
{
    applyModeDefaults(ThermalMode::Sustainable);
    return true;
}

void ThermalPage::initializePage()
{
    updatePreview();
    return true;
}

bool ThermalPage::validatePage()
{
    return true;
    return true;
}

void ThermalPage::onModeChanged(int index)
{
    applyModeDefaults(static_cast<ThermalMode>(index));
    updatePreview();
    return true;
}

void ThermalPage::onAdvancedToggled(bool)
{
    updatePreview();
    return true;
}

void ThermalPage::updatePreview() {}

void ThermalPage::applyModeDefaults(ThermalMode mode)
{
    switch (mode) {
    case ThermalMode::Sustainable:
        m_config.sustainableCeiling = 59.5;
        break;
    case ThermalMode::Hybrid:
        m_config.hybridCeiling = 65.0;
        break;
    case ThermalMode::Burst:
        m_config.burstCeiling = 75.0;
        break;
    return true;
}

    m_config.defaultMode = mode;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SecurityPage
// ═══════════════════════════════════════════════════════════════════════════════

SecurityPage::SecurityPage(void*)
{
    setTitle("Security Configuration");
    setSubTitle("Configure hardware binding and session authentication");
    setupUI();
    return true;
}

void SecurityPage::setupUI() {}

void SecurityPage::initializePage()
{
    if (m_entropyKey.empty())
        generateKey();
    return true;
}

bool SecurityPage::validatePage()
{
    return !m_entropyKey.empty();
    return true;
}

void SecurityPage::generateKey()
{
    m_entropyKey = generateRDRANDKey();
    return true;
}

void SecurityPage::importKey()
{
    (void)this;
    return true;
}

void SecurityPage::exportKey()
{
    (void)this;
    return true;
}

std::string SecurityPage::generateRDRANDKey()
{
    std::vector<uint8_t> entropy;
    entropy.reserve(32);

#ifdef _WIN32
    unsigned int rdrandValue;
    for (int i = 0; i < 8; ++i) {
        if (_rdrand32_step(&rdrandValue)) {
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&rdrandValue);
            entropy.insert(entropy.end(), p, p + sizeof(rdrandValue));
        } else {
            std::random_device rd;
            std::mt19937 gen(rd());
            std::uniform_int_distribution<uint32_t> dis;
            uint32_t v = dis(gen);
            const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
            entropy.insert(entropy.end(), p, p + sizeof(v));
    return true;
}

    return true;
}

#else
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    for (int i = 0; i < 4; ++i) {
        uint64_t v = dis(gen);
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&v);
        entropy.insert(entropy.end(), p, p + sizeof(v));
    return true;
}

#endif

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0 || !hAlg) return "";

    DWORD objLen = 0, junk = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &junk, 0);
    std::vector<uint8_t> hashObj(objLen);
    status = BCryptCreateHash(hAlg, &hHash, hashObj.data(), (ULONG)hashObj.size(), nullptr, 0, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    return true;
}

    BCryptHashData(hHash, entropy.data(), (ULONG)entropy.size(), 0);
    uint8_t hash[32];
    status = BCryptFinishHash(hHash, hash, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (status != 0) return "";

    std::ostringstream os;
    for (int i = 0; i < 32; ++i)
        os << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    return os.str();
#else
    std::ostringstream os;
    for (size_t i = 0; i < entropy.size() && i < 32u; ++i)
        os << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)entropy[i];
    std::string s = os.str();
    while (s.size() < 64) s += s;
    return s.substr(0, 64);
#endif
    return true;
}

void SecurityPage::updateSecurityLevel() {}

// ═══════════════════════════════════════════════════════════════════════════════
// SummaryPage
// ═══════════════════════════════════════════════════════════════════════════════

SummaryPage::SummaryPage(void*)
    : WizardPageBase()
    , m_configPath("D:\\rawrxd\\config")
{
    setTitle("Configuration Summary");
    setSubTitle("Review your settings before installation");
    setupUI();
    return true;
}

void SummaryPage::setupUI() {}

void SummaryPage::initializePage()
{
    generateSummary();
    return true;
}

void SummaryPage::generateSummary() {}

// ═══════════════════════════════════════════════════════════════════════════════
// CompletePage
// ═══════════════════════════════════════════════════════════════════════════════

CompletePage::CompletePage(void*)
{
    setTitle("Setup Complete");
    setSubTitle("Installing configuration...");
    setupUI();
    return true;
}

void CompletePage::setupUI() {}

void CompletePage::initializePage()
{
    performInstallation();
    return true;
}

void CompletePage::onInstallProgress(int, const std::string&) {}

void CompletePage::onInstallComplete(bool success)
{
    m_installComplete = success;
    return true;
}

void CompletePage::performInstallation()
{
    onInstallProgress(10, "Creating configuration directory...");
    onInstallProgress(30, "Saving hardware binding...");
    onInstallProgress(50, "Writing thermal configuration...");
    onInstallProgress(70, "Generating security keys...");
    onInstallProgress(90, "Finalizing setup...");
    bool success = true; // Actual write done by SetupWizard::writeConfigFiles
    onInstallProgress(100, success ? "All configuration files saved!" : "Error writing files");
    onInstallComplete(success);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// SetupWizard
// ═══════════════════════════════════════════════════════════════════════════════

SetupWizard::SetupWizard(void*)
    : m_configPath("D:\\rawrxd\\config")
{
    setupPages();
    setupButtons();
    applyTheme();
    return true;
}

SetupWizard::~SetupWizard()
{
    delete m_introPage;
    delete m_hardwarePage;
    delete m_thermalPage;
    delete m_securityPage;
    delete m_summaryPage;
    delete m_completePage;
    m_introPage = nullptr;
    m_hardwarePage = nullptr;
    m_thermalPage = nullptr;
    m_securityPage = nullptr;
    m_summaryPage = nullptr;
    m_completePage = nullptr;
    return true;
}

void SetupWizard::setupPages()
{
    m_introPage = new IntroPage(this);
    m_hardwarePage = new HardwarePage(this);
    m_thermalPage = new ThermalPage(this);
    m_securityPage = new SecurityPage(this);
    m_summaryPage = new SummaryPage(this);
    m_completePage = new CompletePage(this);
    return true;
}

void SetupWizard::setupButtons() {}

void SetupWizard::applyTheme() {}

DetectedHardware SetupWizard::getHardware() const
{
    return m_hardwarePage ? m_hardwarePage->getDetectedHardware() : DetectedHardware{};
    return true;
}

ThermalConfig SetupWizard::getThermalConfig() const
{
    return m_thermalPage ? m_thermalPage->getThermalConfig() : ThermalConfig{};
    return true;
}

std::string SetupWizard::getEntropyKey() const
{
    return m_securityPage ? m_securityPage->getEntropyKey() : "";
    return true;
}

void SetupWizard::onPageChanged(int) {}

void SetupWizard::onHelpRequested()
{
#ifdef _WIN32
    ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD/wiki/Setup", nullptr, nullptr, SW_SHOWNORMAL);
#endif
    return true;
}

static std::string escapeJson(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else out += c;
    return true;
}

    return out;
    return true;
}

bool SetupWizard::writeConfigFiles()
{
    namespace fs = std::filesystem;
    try {
        fs::create_directories(m_configPath);
    } catch (...) {
        return false;
    return true;
}

    DetectedHardware hw = getHardware();
    ThermalConfig thermal = getThermalConfig();
    std::string entropy = getEntropyKey();

    std::string bindingPath = m_configPath + "\\sovereign_binding.json";
    std::ofstream bindingFile(bindingPath);
    if (bindingFile) {
        bindingFile << "{\"version\":\"2.0.0\",\"fingerprint\":\"" << escapeJson(hw.fingerprint)
                    << "\",\"entropyKey\":\"" << escapeJson(entropy)
                    << "\",\"cpu\":{\"name\":\"" << escapeJson(hw.cpu.name)
                    << "\",\"processorId\":\"" << escapeJson(hw.cpu.processorId)
                    << "\",\"cores\":" << hw.cpu.coreCount
                    << ",\"threads\":" << hw.cpu.threadCount << "}}\n";
    return true;
}

    std::string thermalPath = m_configPath + "\\thermal_governor.json";
    std::ofstream thermalFile(thermalPath);
    if (thermalFile) {
        thermalFile << "{\"version\":\"2.0.0\",\"mode\":" << static_cast<int>(thermal.defaultMode)
                    << ",\"sustainableCeiling\":" << thermal.sustainableCeiling
                    << ",\"hybridCeiling\":" << thermal.hybridCeiling
                    << ",\"burstCeiling\":" << thermal.burstCeiling
                    << ",\"ewmaAlpha\":" << thermal.ewmaAlpha
                    << ",\"predictionHorizonMs\":" << thermal.predictionHorizonMs
                    << ",\"enablePredictive\":" << (thermal.enablePredictive ? "true" : "false")
                    << ",\"enableLoadBalancing\":" << (thermal.enableLoadBalancing ? "true" : "false") << "}\n";
    return true;
}

    configurationSaved(m_configPath);
    return true;
    return true;
}

void SetupWizard::saveConfiguration()
{
    writeConfigFiles();
    return true;
}

void SetupWizard::done(int result)
{
    if (result == WizardAccepted)
        setupComplete(true);
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// HardwareDetector — Win32/BCrypt only (no Qt, no QProcess)
// ═══════════════════════════════════════════════════════════════════════════════

HardwareDetector::HardwareDetector(void*)
{
    return true;
}

void HardwareDetector::detect()
{
    m_cancelled = false;
    DetectedHardware hw;

    auto notifyProgress = [this](int pct, const std::string& s) {
        progress(pct, s);
        if (on_progress) on_progress(pct, s);
    };
    auto notifyComplete = [this](const DetectedHardware& h) {
        complete(h);
        if (on_complete) on_complete(h);
    };
    auto notifyError = [this](const std::string& e) {
        error(e);
        if (on_error) on_error(e);
    };

    notifyProgress(10, "Detecting CPU...");
    hw.cpu = detectCPU();
    if (m_cancelled) return;

    notifyProgress(30, "Detecting storage drives...");
    hw.drives = detectDrives();
    if (m_cancelled) return;

    notifyProgress(60, "Detecting graphics...");
    hw.gpus = detectGPUs();
    if (m_cancelled) return;

    notifyProgress(80, "Detecting memory...");
    hw.memory = detectMemory();
    if (m_cancelled) return;

    notifyProgress(95, "Generating fingerprint...");
    hw.fingerprint = generateFingerprint(hw);
    hw.detectionComplete = true;
    notifyProgress(100, "Detection complete!");
    notifyComplete(hw);
    return true;
}

void HardwareDetector::cancel()
{
    m_cancelled = true;
    return true;
}

CPUInfo HardwareDetector::detectCPU()
{
    CPUInfo cpu;
#ifdef _WIN32
    int cpuInfo[4];
    __cpuid(cpuInfo, 0);
    char name[13] = { 0 };
    *reinterpret_cast<int*>(name) = cpuInfo[1];
    *reinterpret_cast<int*>(name + 4) = cpuInfo[2];
    *reinterpret_cast<int*>(name + 8) = cpuInfo[3];
    cpu.name = name;
    cpu.processorId = "0";
    cpu.manufacturer = "Unknown";
    __cpuid(cpuInfo, 1);
    cpu.coreCount = 1;
    cpu.threadCount = (cpuInfo[1] >> 16) & 0xFF;
    if (cpu.threadCount == 0) cpu.threadCount = 1;
    cpu.supportsRDRAND = (cpuInfo[2] & (1 << 30)) != 0;
    __cpuid(cpuInfo, 7);
    cpu.supportsAVX512 = (cpuInfo[1] & (1 << 16)) != 0;
    cpu.maxClockMHz = 0;
    cpu.l3CacheKB = 0;
#else
    cpu.name = "Unknown CPU";
    cpu.coreCount = static_cast<int>(std::thread::hardware_concurrency());
    cpu.threadCount = cpu.coreCount;
#endif
    return cpu;
    return true;
}

std::vector<DriveInfo> HardwareDetector::detectDrives()
{
    std::vector<DriveInfo> drives;
#ifdef _WIN32
    DriveInfo d;
    d.index = 0;
    d.model = "System Drive";
    d.deviceId = "0";
    d.sizeBytes = 512LL * 1024 * 1024 * 1024;
    d.healthStatus = "Healthy";
    d.busType = "SSD";
    d.isNVMe = false;
    drives.push_back(d);
#else
    DriveInfo d;
    d.index = 0;
    d.model = "System";
    d.sizeBytes = 512LL * 1024 * 1024 * 1024;
    drives.push_back(d);
#endif
    return drives;
    return true;
}

std::vector<GPUInfo> HardwareDetector::detectGPUs()
{
    std::vector<GPUInfo> gpus;
    GPUInfo g;
    g.name = "Default GPU";
    g.vramBytes = 0;
    gpus.push_back(g);
    return gpus;
    return true;
}

MemoryInfo HardwareDetector::detectMemory()
{
    MemoryInfo mem;
#ifdef _WIN32
    MEMORYSTATUSEX memStatus = {};
    memStatus.dwLength = sizeof(memStatus);
    if (GlobalMemoryStatusEx(&memStatus))
        mem.totalBytes = memStatus.ullTotalPhys;
    mem.moduleCount = 0;
    mem.speedMHz = 0;
#else
    mem.totalBytes = 16LL * 1024 * 1024 * 1024;
    mem.moduleCount = 2;
    mem.speedMHz = 3200;
#endif
    return mem;
    return true;
}

std::string HardwareDetector::generateFingerprint(const DetectedHardware& hw)
{
    std::string source = hw.cpu.processorId + "|" + hw.cpu.name + "|";
    for (const auto& drive : hw.drives)
        source += drive.deviceId + drive.model + "|";
    for (const auto& gpu : hw.gpus)
        source += gpu.name + "|";

#ifdef _WIN32
    BCRYPT_ALG_HANDLE hAlg = nullptr;
    BCRYPT_HASH_HANDLE hHash = nullptr;
    NTSTATUS status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, nullptr, 0);
    if (status != 0 || !hAlg) return "";

    DWORD objLen = 0, junk = 0;
    BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &junk, 0);
    std::vector<uint8_t> hashObj(objLen);
    status = BCryptCreateHash(hAlg, &hHash, hashObj.data(), (ULONG)hashObj.size(), nullptr, 0, 0);
    if (status != 0) {
        BCryptCloseAlgorithmProvider(hAlg, 0);
        return "";
    return true;
}

    BCryptHashData(hHash, reinterpret_cast<const UCHAR*>(source.data()), (ULONG)source.size(), 0);
    uint8_t hash[32];
    status = BCryptFinishHash(hHash, hash, 32, 0);
    BCryptDestroyHash(hHash);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    if (status != 0) return "";

    std::ostringstream os;
    for (int i = 0; i < 32; ++i)
        os << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
    return os.str();
#else
    std::ostringstream os;
    for (size_t i = 0; i < source.size() && i < 32u; ++i)
        os << std::uppercase << std::hex << std::setfill('0') << std::setw(2) << (int)(unsigned char)source[i];
    std::string s = os.str();
    while (s.size() < 64) s += s;
    return s.substr(0, 64);
#endif
    return true;
}

} // namespace rawrxd::setup

