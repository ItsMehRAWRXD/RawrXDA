// ===============================================================================
// Language Framework Integration - Main IDE Menu Integration
// ===============================================================================

#ifndef LANGUAGE_INTEGRATION_HPP
#define LANGUAGE_INTEGRATION_HPP

#include <QMainWindow>
#include <QDockWidget>
#include <QAction>
#include <QMenu>
#include <memory>
#include "language_widget.hpp"

namespace RawrXD {
namespace Languages {

/**
 * @brief Integration module for connecting language framework to main IDE
 * 
 * This module handles:
 * - Creation of language management dock widget
 * - Integration with main window menu bar
 * - File detection and auto-selection of language
 * - Context menu actions for compilation
 * - Settings and preferences
 */
class LanguageIntegration {
public:
    /**
     * @brief Initialize language integration in the main window
     * @param mainWindow The main application window
     */
    static void initialize(QMainWindow* mainWindow);
    
    /**
     * @brief Create the language management dock widget
     * @param parent The parent widget
     * @return The created language widget
     */
    static LanguageWidget* createLanguageWidget(QMainWindow* parent);
    
    /**
     * @brief Add language menu to main menu bar
     * @param mainWindow The main application window
     */
    static void createLanguageMenu(QMainWindow* mainWindow);
    
    /**
     * @brief Detect language from open file and select it
     * @param filePath The file path to detect
     */
    static void detectAndSelectLanguage(const QString& filePath);
    
    /**
     * @brief Handle file opened event
     * @param filePath The path of the opened file
     */
    static void onFileOpened(const QString& filePath);
    
    /**
     * @brief Handle compilation request
     * @param filePath The file to compile
     */
    static void onCompileFile(const QString& filePath);
    
    /**
     * @brief Get the language widget instance
     * @return Pointer to the language widget
     */
    static LanguageWidget* languageWidget() { return languageWidget_; }
    
    /**
     * @brief Show/hide the language widget
     * @param visible True to show, false to hide
     */
    static void setWidgetVisible(bool visible);
    
    /**
     * @brief Update IDE output with compilation result
     * @param success Whether compilation succeeded
     * @param output The output message
     */
    static void updateIDEOutput(bool success, const QString& output);

private:
    static LanguageWidget* languageWidget_;
    static QDockWidget* languageDockWidget_;
    static QMenu* languageMenu_;
    static QMainWindow* mainWindow_;
};

} // namespace Languages
} // namespace RawrXD

#endif // LANGUAGE_INTEGRATION_HPP
