/**
 * \file scripting_console.h
 * \brief Console UI for script execution and debugging
 * \author RawrXD Team
 * \date 2026-01-22
 */

#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPushButton>
#include <QComboBox>
#include <QProgressBar>
#include "scripting_engine.h"

namespace RawrXD {
namespace ReverseEngineering {

/**
 * \class ScriptingConsole
 * \brief Interactive scripting console for custom analysis
 */
class ScriptingConsole : public QWidget {
    Q_OBJECT

public:
    /**
     * \brief Construct scripting console
     * \param parent Parent widget
     */
    explicit ScriptingConsole(QWidget* parent = nullptr);
    
    /**
     * \brief Set analysis context for scripts
     * \param context Context map (binary path, functions, etc.)
     */
    void setContext(const QMap<QString, QVariant>& context);
    
    /**
     * \brief Load script from file
     * \param filePath Path to script
     * \return True if successful
     */
    bool loadScript(const QString& filePath);
    
    /**
     * \brief Execute current script
     * \return True if successful
     */
    bool executeScript();
    
    /**
     * \brief Clear output console
     */
    void clearOutput();

private slots:
    void onExecuteClicked();
    void onLoadClicked();
    void onSaveClicked();
    void onClearClicked();
    void onLanguageChanged(const QString& language);

private:
    void setupUI();
    void appendOutput(const QString& text, bool isError = false);
    
    QTextEdit* m_scriptEdit;
    QTextEdit* m_outputEdit;
    QComboBox* m_languageCombo;
    QPushButton* m_executeButton;
    QPushButton* m_loadButton;
    QPushButton* m_saveButton;
    QPushButton* m_clearButton;
    QProgressBar* m_progressBar;
    
    std::unique_ptr<ScriptingEngine> m_engine;
    QMap<QString, QVariant> m_context;
    QString m_currentLanguage;
};

} // namespace ReverseEngineering
} // namespace RawrXD
