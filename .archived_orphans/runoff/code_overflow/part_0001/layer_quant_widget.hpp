#pragma once

#include <QWidget>
#include <QString>

QT_BEGIN_NAMESPACE
class QComboBox;
class QSpinBox;
class QPushButton;
class QLabel;
QT_END_NAMESPACE

/**
 * \class LayerQuantWidget
 * \brief Widget for managing layer quantization modes and settings
 * 
 * Allows users to select quantization modes (Q4_0, Q5_0, Q6_K, etc.) and
 * configure per-layer quantization parameters.
 */
class LayerQuantWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LayerQuantWidget(QWidget* parent = nullptr);
    ~LayerQuantWidget() override;

    /**
     * \brief Get the currently selected quantization mode
     * \return The quantization mode string (e.g., "Q4_0", "Q5_0", "Q6_K")
     */
    QString getCurrentQuantMode() const;

    /**
     * \brief Set the quantization mode
     * \param mode The quantization mode to set
     */
    void setQuantMode(const QString& mode);

signals:
    /**
     * \brief Emitted when the quantization mode is changed
     * \param mode The new quantization mode
     */
    void quantModeChanged(const QString& mode);

    /**
     * \brief Emitted when layer quantization settings are updated
     */
    void settingsUpdated();

private slots:
    void onModeChanged(int index);
    void onApplySettings();

private:
    void setupUI();

    QComboBox* m_quantModeCombo{};
    QSpinBox* m_layerStartSpin{};
    QSpinBox* m_layerEndSpin{};
    QPushButton* m_applyButton{};
    QLabel* m_statusLabel{};
    QString m_currentMode{"Q4_0"};
};
