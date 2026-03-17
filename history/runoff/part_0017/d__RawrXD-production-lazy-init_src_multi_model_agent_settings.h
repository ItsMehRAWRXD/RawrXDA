// ════════════════════════════════════════════════════════════════════════════════
// MULTI-MODEL AGENT SETTINGS DIALOG
// ════════════════════════════════════════════════════════════════════════════════

#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QMessageBox>
#include <QSettings>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>

namespace RawrXD {
namespace IDE {

class MultiModelAgentSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MultiModelAgentSettingsDialog(QWidget* parent = nullptr);
    ~MultiModelAgentSettingsDialog();

    void loadSettings();
    void saveSettings();

signals:
    void settingsChanged();

private slots:
    void onAccept();
    void onApply();
    void onReset();
    void onTestConnection();

private:
    void setupUI();
    void setupApiKeysTab();
    void setupGeneralTab();
    void setupAdvancedTab();

    bool testApiConnection(const QString& provider, const QString& apiKey);
    void updateModelCombo();

    // UI Components
    QTabWidget* tabWidget;

    // API Keys Tab
    QWidget* apiKeysTab;
    QGridLayout* apiKeysLayout;

    // OpenAI
    QLabel* openAILabel;
    QLineEdit* openAIApiKeyEdit;
    QPushButton* openAITestBtn;

    // Anthropic
    QLabel* anthropicLabel;
    QLineEdit* anthropicApiKeyEdit;
    QPushButton* anthropicTestBtn;

    // Google
    QLabel* googleLabel;
    QLineEdit* googleApiKeyEdit;
    QPushButton* googleTestBtn;

    // General Tab
    QWidget* generalTab;
    QVBoxLayout* generalLayout;

    QCheckBox* browserModeCheck;
    QCheckBox* autoStartAgentsCheck;
    QSpinBox* maxAgentsSpin;
    QComboBox* defaultProviderCombo;
    QComboBox* defaultModelCombo;

    // Advanced Tab
    QWidget* advancedTab;
    QVBoxLayout* advancedLayout;

    QSpinBox* responseTimeoutSpin;
    QCheckBox* enableParallelExecutionCheck;
    QCheckBox* enableQualityScoringCheck;
    QTextEdit* customPromptsEdit;

    // Dialog buttons
    QHBoxLayout* buttonLayout;
    QPushButton* okBtn;
    QPushButton* applyBtn;
    QPushButton* resetBtn;
    QPushButton* cancelBtn;

    // Settings storage
    QSettings* settings;
};

} // namespace IDE
} // namespace RawrXD