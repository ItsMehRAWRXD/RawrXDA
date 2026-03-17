#pragma once

#include <QtCore/QObject>
#include <QtCore/QSettings>

namespace mem {

/**
 * @class MemorySettings
 * @brief User-facing memory configuration with context token slider
 * 
 * Features:
 * - Context token slider: 4k → 1M (exponential scale for UI)
 * - GPU KV cache control
 * - Chat compression toggle
 * - Persistence to RawrXD-AgenticIDE.conf
 */
class MemorySettings : public QObject {
    Q_OBJECT
    Q_PROPERTY(int contextTokens READ contextTokens WRITE setContextTokens NOTIFY contextTokensChanged)
    Q_PROPERTY(bool useGpuKv READ useGpuKv WRITE setUseGpuKv NOTIFY useGpuKvChanged)
    Q_PROPERTY(bool compressChat READ compressChat WRITE setCompressChat NOTIFY compressChatChanged)
    Q_PROPERTY(bool enableLongTermMemory READ enableLongTermMemory WRITE setEnableLongTermMemory NOTIFY enableLongTermMemoryChanged)

public:
    explicit MemorySettings(QObject* parent = nullptr);
    ~MemorySettings();

    /**
     * @brief Get current context token size
     * @return Size in tokens [4'000, 1'000'000]
     */
    int contextTokens() const { return m_contextTokens; }

    /**
     * @brief Set context token size
     * @param tokens Size in tokens (auto-clamped to [4k, 1M])
     */
    void setContextTokens(int tokens);

    /**
     * @brief Get context tokens as slider value [0, 100]
     * @return Slider position for UI (exponential scale)
     */
    int contextTokensSlider() const;

    /**
     * @brief Set context tokens from slider value [0, 100]
     * @param sliderValue Slider position (auto-converts to exponential)
     */
    void setContextTokensFromSlider(int sliderValue);

    /**
     * @brief Get GPU KV cache usage
     */
    bool useGpuKv() const { return m_useGpuKv; }
    void setUseGpuKv(bool use);

    /**
     * @brief Get chat compression enabled
     */
    bool compressChat() const { return m_compressChat; }
    void setCompressChat(bool compress);

    /**
     * @brief Get long-term memory enabled
     */
    bool enableLongTermMemory() const { return m_enableLongTermMemory; }
    void setEnableLongTermMemory(bool enable);

    /**
     * @brief Load settings from file
     */
    void load();

    /**
     * @brief Save settings to file
     */
    void save();

    /**
     * @brief Reset to defaults
     */
    void resetToDefaults();

    /**
     * @brief Format context tokens for display
     * @return Human-readable string (e.g., "128 K")
     */
    QString contextTokensDisplayText() const;

    /**
     * @brief Get estimated VRAM usage for current context
     * @return VRAM in MB
     */
    int estimatedVramMb() const;

signals:
    void contextTokensChanged(int tokens);
    void useGpuKvChanged(bool use);
    void compressChatChanged(bool compress);
    void enableLongTermMemoryChanged(bool enable);
    void settingsChanged();

private:
    int m_contextTokens = 131'072;  // 128 K default
    bool m_useGpuKv = true;
    bool m_compressChat = true;
    bool m_enableLongTermMemory = true;
    
    std::unique_ptr<QSettings> m_settings;

    /**
     * @brief Convert slider [0,100] to exponential context size
     */
    static int sliderToContextTokens(int slider);

    /**
     * @brief Convert context size to slider [0,100]
     */
    static int contextTokensToSlider(int tokens);
};

} // namespace mem
