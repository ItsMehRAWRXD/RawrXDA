/**
 * @file compiler_panel.h
 * @brief Compiler Panel UI for PowerShell Compiler Integration in Agentic IDE
 */

#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QListWidget>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QLineEdit>
#include <QTabWidget>
#include "powershell_compiler_manager.h"

/**
 * @class CompilerPanel
 * @brief UI panel for managing and executing PowerShell compilers
 */
class CompilerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CompilerPanel(QWidget *parent = nullptr);
    ~CompilerPanel();

    // Public methods
    void setCurrentSourceCode(const QString &code);
    void setCurrentFilePath(const QString &filePath);
    QString getSelectedLanguage() const;
    
    // Configuration
    void loadConfiguration();
    void saveConfiguration();

public slots:
    void onCompileClicked();
    void onCompileFileClicked();
    void onLanguageChanged(int index);
    void onCompilerSettingsChanged();
    void onCompilationFinished(const CompilationResult &result);
    void onCompilationError(const QString &language, const QString &error);

private:
    // UI Components
    QVBoxLayout *m_mainLayout;
    QTabWidget *m_tabWidget;
    
    // Compiler Selection Tab
    QWidget *m_compilerTab;
    QComboBox *m_languageComboBox;
    QLabel *m_compilerInfoLabel;
    QPushButton *m_compileButton;
    QPushButton *m_compileFileButton;
    QProgressBar *m_progressBar;
    QLabel *m_statusLabel;
    
    // Settings Tab
    QWidget *m_settingsTab;
    QSpinBox *m_timeoutSpinBox;
    QLineEdit *m_compilersPathEdit;
    QPushButton *m_browsePathButton;
    QPushButton *m_reloadCompilersButton;
    QCheckBox *m_autoDetectCheckBox;
    QCheckBox *m_showOutputCheckBox;
    
    // Output Tab
    QWidget *m_outputTab;
    QTextEdit *m_outputTextEdit;
    QPushButton *m_clearOutputButton;
    QPushButton *m_copyOutputButton;
    
    // Statistics Tab
    QWidget *m_statsTab;
    QListWidget *m_statsListWidget;
    QPushButton *m_resetStatsButton;
    
    // Data
    PowerShellCompilerManager *m_compilerManager;
    QString m_currentSourceCode;
    QString m_currentFilePath;
    
    // Internal Methods
    void setupUI();
    void setupCompilerTab();
    void setupSettingsTab();
    void setupOutputTab();
    void setupStatsTab();
    void populateLanguageComboBox();
    void updateCompilerInfo();
    void updateStats();
    void logOutput(const QString &message, bool isError = false);
};

/**
 * @brief Global instance accessor for CompilerPanel
 * @return Singleton instance
 */
CompilerPanel* getCompilerPanel();