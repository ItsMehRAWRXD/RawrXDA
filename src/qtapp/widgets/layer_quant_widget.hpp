#pragma once


// REMOVED_QT: // QT_BEGIN_NAMESPACE


// REMOVED_QT: // QT_END_NAMESPACE

/**
 * \class LayerQuantWidget
 * \brief Widget for managing layer quantization modes and settings
 * 
 * Allows users to select quantization modes (Q4_0, Q5_0, Q6_K, etc.) and
 * configure per-layer quantization parameters.
 */
class LayerQuantWidget : public void
{

public:
    explicit LayerQuantWidget(void* parent = nullptr);
    ~LayerQuantWidget() override;

    /**
     * \brief Get the currently selected quantization mode
     * \return The quantization mode string (e.g., "Q4_0", "Q5_0", "Q6_K")
     */
    std::string getCurrentQuantMode() const;

    /**
     * \brief Set the quantization mode
     * \param mode The quantization mode to set
     */
    void setQuantMode(const std::string& mode);

    /**
     * \brief Emitted when the quantization mode is changed
     * \param mode The new quantization mode
     */
    void quantModeChanged(const std::string& mode);

    /**
     * \brief Emitted when layer quantization settings are updated
     */
    void settingsUpdated();

private:
    void onModeChanged(int index);
    void onApplySettings();

private:
    void setupUI();

    void* m_quantModeCombo{};
    void* m_layerStartSpin{};
    void* m_layerEndSpin{};
    void* m_applyButton{};
    void* m_statusLabel{};
    std::string m_currentMode{"Q4_0"};
};

