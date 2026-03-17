/**
 * \file scripting_console.cpp
 * \brief Implementation of scripting console
 * \author RawrXD Team
 * \date 2026-01-22
 */

#include "scripting_console.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QLabel>
#include <QSplitter>
#include <QFont>
#include <QDebug>

using namespace RawrXD::ReverseEngineering;

ScriptingConsole::ScriptingConsole(QWidget* parent)
    : QWidget(parent), m_engine(std::make_unique<ScriptingEngine>()), m_currentLanguage("Python") {
    setupUI();
    qDebug() << "Scripting console initialized";
}

void ScriptingConsole::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Toolbar
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    toolbarLayout->addWidget(new QLabel("Language:"));
    m_languageCombo = new QComboBox();
    m_languageCombo->addItems({"Python", "Lua", "JavaScript"});
    connect(m_languageCombo, QOverload<const QString&>::of(&QComboBox::currentTextChanged),
            this, &ScriptingConsole::onLanguageChanged);
    toolbarLayout->addWidget(m_languageCombo);
    
    toolbarLayout->addStretch();
    
    m_executeButton = new QPushButton("Execute");
    connect(m_executeButton, &QPushButton::clicked, this, &ScriptingConsole::onExecuteClicked);
    toolbarLayout->addWidget(m_executeButton);
    
    m_loadButton = new QPushButton("Load Script");
    connect(m_loadButton, &QPushButton::clicked, this, &ScriptingConsole::onLoadClicked);
    toolbarLayout->addWidget(m_loadButton);
    
    m_saveButton = new QPushButton("Save Script");
    connect(m_saveButton, &QPushButton::clicked, this, &ScriptingConsole::onSaveClicked);
    toolbarLayout->addWidget(m_saveButton);
    
    m_clearButton = new QPushButton("Clear");
    connect(m_clearButton, &QPushButton::clicked, this, &ScriptingConsole::onClearClicked);
    toolbarLayout->addWidget(m_clearButton);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMaximumHeight(20);
    mainLayout->addWidget(m_progressBar);
    
    // Script and Output panes
    QSplitter* splitter = new QSplitter(Qt::Vertical);
    
    // Script editor
    m_scriptEdit = new QTextEdit();
    m_scriptEdit->setFont(QFont("Courier", 10));
    m_scriptEdit->setPlaceholderText("Write your analysis script here...\n\n"
                                      "Available variables:\n"
                                      "  - binaryPath: Path to loaded binary\n"
                                      "  - functions: List of detected functions\n"
                                      "  - symbols: Symbol information\n"
                                      "  - strings: Extracted strings\n\n"
                                      "Python example:\n"
                                      "print(f'Binary: {binaryPath}')\n"
                                      "for func in functions:\n"
                                      "    print(f'Function: {func}')\n");
    splitter->addWidget(m_scriptEdit);
    
    // Output console
    QWidget* outputWidget = new QWidget();
    QVBoxLayout* outputLayout = new QVBoxLayout(outputWidget);
    outputLayout->addWidget(new QLabel("Output:"));
    m_outputEdit = new QTextEdit();
    m_outputEdit->setReadOnly(true);
    m_outputEdit->setFont(QFont("Courier", 9));
    outputLayout->addWidget(m_outputEdit);
    splitter->addWidget(outputWidget);
    
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter);
    
    setLayout(mainLayout);
}

void ScriptingConsole::setContext(const QMap<QString, QVariant>& context) {
    m_context = context;
    
    QString contextInfo;
    for (auto it = context.constBegin(); it != context.constEnd(); ++it) {
        contextInfo += QString("- %1: %2\n").arg(it.key(), it.value().toString());
    }
    
    appendOutput(QString("Context updated:\n%1").arg(contextInfo));
}

bool ScriptingConsole::loadScript(const QString& filePath) {
    QString script = ScriptingEngine::loadScript(filePath);
    if (script.isEmpty()) {
        appendOutput("Failed to load script: " + filePath, true);
        return false;
    }
    
    m_scriptEdit->setText(script);
    appendOutput("Loaded script: " + filePath);
    return true;
}

bool ScriptingConsole::executeScript() {
    QString script = m_scriptEdit->toPlainText();
    if (script.trimmed().isEmpty()) {
        appendOutput("Error: Script is empty", true);
        return false;
    }
    
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    appendOutput(QString("Executing %1 script...\n").arg(m_currentLanguage));
    
    QString result;
    
    if (m_currentLanguage == "Python") {
        result = m_engine->executePython(script, m_context);
    } else if (m_currentLanguage == "Lua") {
        result = m_engine->executeLua(script, m_context);
    } else if (m_currentLanguage == "JavaScript") {
        result = m_engine->executeJavaScript(script, m_context);
    }
    
    m_progressBar->setValue(100);
    
    if (!m_engine->lastError().isEmpty()) {
        appendOutput("Error: " + m_engine->lastError(), true);
        m_progressBar->setVisible(false);
        return false;
    }
    
    appendOutput(result);
    m_progressBar->setVisible(false);
    
    return true;
}

void ScriptingConsole::clearOutput() {
    m_outputEdit->clear();
}

void ScriptingConsole::onExecuteClicked() {
    executeScript();
}

void ScriptingConsole::onLoadClicked() {
    QString filePath = QFileDialog::getOpenFileName(this, "Load Script", "",
                                                     "Python (*.py);;Lua (*.lua);;JavaScript (*.js);;All files (*)");
    if (!filePath.isEmpty()) {
        loadScript(filePath);
        
        // Auto-detect language from extension
        if (filePath.endsWith(".py")) {
            m_languageCombo->setCurrentText("Python");
        } else if (filePath.endsWith(".lua")) {
            m_languageCombo->setCurrentText("Lua");
        } else if (filePath.endsWith(".js")) {
            m_languageCombo->setCurrentText("JavaScript");
        }
    }
}

void ScriptingConsole::onSaveClicked() {
    QString filePath = QFileDialog::getSaveFileName(this, "Save Script", "",
                                                     "Python (*.py);;Lua (*.lua);;JavaScript (*.js);;All files (*)");
    if (!filePath.isEmpty()) {
        if (ScriptingEngine::saveScript(filePath, m_scriptEdit->toPlainText())) {
            appendOutput("Script saved: " + filePath);
        } else {
            appendOutput("Failed to save script", true);
        }
    }
}

void ScriptingConsole::onClearClicked() {
    clearOutput();
}

void ScriptingConsole::onLanguageChanged(const QString& language) {
    m_currentLanguage = language;
    appendOutput(QString("Language switched to: %1").arg(language));
}

void ScriptingConsole::appendOutput(const QString& text, bool isError) {
    QString output = text;
    if (isError) {
        output = "<font color='red'>" + text + "</font>";
        m_outputEdit->append(output);
    } else {
        m_outputEdit->append(output);
    }
}
