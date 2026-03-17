#include "MemorySettings.hpp"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <cmath>

namespace mem {

MemorySettings::MemorySettings(QObject* parent)
    : QObject(parent) {
    QString configPath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath("RawrXD-AgenticIDE.conf");
    m_settings = std::make_unique<QSettings>(configPath, QSettings::IniFormat, this);
    load();
}

MemorySettings::~MemorySettings() {
    save();
}

void MemorySettings::setContextTokens(int tokens) {
    int clamped = std::clamp(tokens, 4'000, 1'000'000);
    if (clamped == m_contextTokens)
        return;

    m_contextTokens = clamped;
    emit contextTokensChanged(clamped);
    emit settingsChanged();
    save();
}

int MemorySettings::contextTokensSlider() const {
    return contextTokensToSlider(m_contextTokens);
}

void MemorySettings::setContextTokensFromSlider(int sliderValue) {
    int tokens = sliderToContextTokens(sliderValue);
    setContextTokens(tokens);
}

void MemorySettings::setUseGpuKv(bool use) {
    if (use == m_useGpuKv)
        return;

    m_useGpuKv = use;
    emit useGpuKvChanged(use);
    emit settingsChanged();
    save();
}

void MemorySettings::setCompressChat(bool compress) {
    if (compress == m_compressChat)
        return;

    m_compressChat = compress;
    emit compressChatChanged(compress);
    emit settingsChanged();
    save();
}

void MemorySettings::setEnableLongTermMemory(bool enable) {
    if (enable == m_enableLongTermMemory)
        return;

    m_enableLongTermMemory = enable;
    emit enableLongTermMemoryChanged(enable);
    emit settingsChanged();
    save();
}

void MemorySettings::load() {
    if (!m_settings) return;

    m_settings->beginGroup("memory");
    m_contextTokens = m_settings->value("context_tokens", 131'072).toInt();
    m_useGpuKv = m_settings->value("use_gpu_kv", true).toBool();
    m_compressChat = m_settings->value("compress_chat", true).toBool();
    m_enableLongTermMemory = m_settings->value("enable_long_term_memory", true).toBool();
    m_settings->endGroup();

    // Ensure valid range
    m_contextTokens = std::clamp(m_contextTokens, 4'000, 1'000'000);
}

void MemorySettings::save() {
    if (!m_settings) return;

    m_settings->beginGroup("memory");
    m_settings->setValue("context_tokens", m_contextTokens);
    m_settings->setValue("use_gpu_kv", m_useGpuKv);
    m_settings->setValue("compress_chat", m_compressChat);
    m_settings->setValue("enable_long_term_memory", m_enableLongTermMemory);
    m_settings->endGroup();
    m_settings->sync();
}

void MemorySettings::resetToDefaults() {
    setContextTokens(131'072);
    setUseGpuKv(true);
    setCompressChat(true);
    setEnableLongTermMemory(true);
}

QString MemorySettings::contextTokensDisplayText() const {
    if (m_contextTokens >= 1'000'000) {
        return QString::number(m_contextTokens / 1'000'000) + " M";
    } else if (m_contextTokens >= 1'000) {
        return QString::number(m_contextTokens / 1'000) + " K";
    }
    return QString::number(m_contextTokens);
}

int MemorySettings::estimatedVramMb() const {
    // Rough estimate: ~1 MB per 1k tokens @ Q8_0
    return m_contextTokens / 1'000;
}

int MemorySettings::sliderToContextTokens(int slider) {
    // Map slider [0,100] to exponential [4k, 1M]
    // Formula: 4000 * 2^(slider/10)
    double exponent = slider / 10.0;
    return static_cast<int>(4'000 * std::pow(2.0, exponent));
}

int MemorySettings::contextTokensToSlider(int tokens) {
    // Inverse: slider = 10 * log2(tokens/4000)
    double ratio = static_cast<double>(tokens) / 4'000;
    return static_cast<int>(10.0 * std::log2(ratio));
}

} // namespace mem
