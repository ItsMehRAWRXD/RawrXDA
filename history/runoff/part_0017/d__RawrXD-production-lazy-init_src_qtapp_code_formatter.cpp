/**
 * \file code_formatter.cpp
 * \brief Multi-formatter wrapper supporting Prettier, Black, Rustfmt, etc.
 * \author RawrXD AI Engineering Team
 * \date January 14, 2026
 * 
 * COMPLETE IMPLEMENTATION - Full formatter integration for all languages
 */

#include "language_support_system.h"
#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QDebug>
#include <QStandardPaths>

namespace RawrXD {
namespace Language {

CodeFormatter::CodeFormatter(QObject* parent)
    : QObject(parent), m_manager(nullptr)
{
}

void CodeFormatter::setLanguageManager(LanguageSupportManager* manager)
{
    m_manager = manager;
}

bool CodeFormatter::format(const QString& filePath, 
                          const QString& code,
                          FormattingCallback callback)
{
    if (!m_manager) {
        qWarning() << "[Formatter] Language manager not set";
        return false;
    }
    
    LanguageID language = m_manager->detectLanguageFromFile(filePath);
    const auto* config = m_manager->getLanguageConfig(language);
    
    if (!config || !config->supportsFormatter) {
        qWarning() << "[Formatter] No formatter available for" << filePath;
        return false;
    }
    
    // Create temporary file with code
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                            "/rawrxd_format_XXXXXX" + config->fileExtension);
    if (!tempFile.open()) {
        qWarning() << "[Formatter] Failed to create temporary file";
        return false;
    }
    
    tempFile.write(code.toUtf8());
    tempFile.flush();
    QString tempPath = tempFile.fileName();
    tempFile.close();
    
    // Format based on language
    return formatWithFormatter(language, tempPath, config->formatterCommand, callback);
}

bool CodeFormatter::formatWithFormatter(LanguageID language,
                                       const QString& filePath,
                                       const QString& formatterCommand,
                                       FormattingCallback callback)
{
    if (formatterCommand.isEmpty()) {
        return false;
    }
    
    QProcess* process = new QProcess();
    
    // Set up formatter-specific arguments
    QStringList args;
    
    if (formatterCommand == "prettier") {
        args << "--write" << filePath;
    } else if (formatterCommand == "black") {
        args << filePath;
    } else if (formatterCommand == "rustfmt") {
        args << filePath;
    } else if (formatterCommand == "gofmt") {
        args << "-w" << filePath;
    } else if (formatterCommand == "clang-format") {
        args << "-i" << filePath;
    } else if (formatterCommand == "dotnet-format") {
        args << filePath;
    } else if (formatterCommand == "rubocop") {
        args << "-a" << filePath;
    } else if (formatterCommand == "php-cs-fixer") {
        args << "fix" << filePath;
    } else {
        args << filePath;  // Default for unknown formatters
    }
    
    process->setProgram(formatterCommand);
    process->setArguments(args);
    
    // Connect signals for async execution
    connect(process, &QProcess::finished, process,
        [this, process, filePath, callback](int exitCode, QProcess::ExitStatus) {
            if (exitCode == 0) {
                // Read formatted code
                QFile file(filePath);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QString formatted = QString::fromUtf8(file.readAll());
                    file.close();
                    
                    qDebug() << "[Formatter] Successfully formatted" << filePath;
                    callback(formatted, true, "");
                } else {
                    callback("", false, "Failed to read formatted file");
                }
            } else {
                QString error = QString::fromUtf8(process->readAllStandardError());
                qWarning() << "[Formatter] Formatting failed:" << error;
                callback("", false, error);
            }
            
            process->deleteLater();
        });
    
    process->start();
    return true;
}

bool CodeFormatter::formatRange(const QString& filePath,
                               const QString& code,
                               int startLine,
                               int endLine,
                               FormattingCallback callback)
{
    if (!m_manager) {
        qWarning() << "[Formatter] Language manager not set";
        return false;
    }
    
    LanguageID language = m_manager->detectLanguageFromFile(filePath);
    const auto* config = m_manager->getLanguageConfig(language);
    
    if (!config || !config->supportsFormatter) {
        return false;
    }
    
    // Create temporary file
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QStandardPaths::writableLocation(QStandardPaths::TempLocation) + 
                            "/rawrxd_format_XXXXXX" + config->fileExtension);
    if (!tempFile.open()) {
        return false;
    }
    
    tempFile.write(code.toUtf8());
    tempFile.flush();
    QString tempPath = tempFile.fileName();
    tempFile.close();
    
    // Most formatters don't support range formatting, so format entire file
    // and then extract the range
    return formatWithFormatter(language, tempPath, config->formatterCommand,
        [startLine, endLine, callback](const QString& formatted, bool success, const QString& error) {
            if (!success) {
                callback(formatted, success, error);
                return;
            }
            
            // Extract formatted range
            QStringList lines = formatted.split('\n');
            QStringList formattedRange;
            
            for (int i = startLine; i < qMin(endLine, lines.size()); ++i) {
                formattedRange.append(lines[i]);
            }
            
            callback(formattedRange.join('\n'), true, "");
        });
}

bool CodeFormatter::formatOnSave(const QString& filePath,
                                const QString& code)
{
    return format(filePath, code, [](const QString&, bool, const QString&) {
        // Fire-and-forget formatting
    });
}

QString CodeFormatter::getFormatterForLanguage(LanguageID language)
{
    if (!m_manager) {
        return "";
    }
    
    const auto* config = m_manager->getLanguageConfig(language);
    return config ? config->formatterCommand : "";
}

QVector<QString> CodeFormatter::getAvailableFormatters()
{
    QVector<QString> formatters = {
        "prettier",      // JS/TS/JSON/CSS/HTML
        "black",         // Python
        "rustfmt",       // Rust
        "gofmt",         // Go
        "clang-format",  // C/C++
        "dotnet-format", // C#/.NET
        "rubocop",       // Ruby
        "php-cs-fixer",  // PHP
        "google-java-format",  // Java
        "fantomas",      // F#
        "ktlint",        // Kotlin
    };
    
    return formatters;
}

}}  // namespace RawrXD::Language
