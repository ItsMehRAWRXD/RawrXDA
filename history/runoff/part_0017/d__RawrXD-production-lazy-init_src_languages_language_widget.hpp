// ===============================================================================
// Language Management Widget - Qt GUI Component
// ===============================================================================

#ifndef LANGUAGE_WIDGET_HPP
#define LANGUAGE_WIDGET_HPP

#include <QWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QListWidget>
#include <QTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <memory>
#include "language_framework.hpp"

namespace RawrXD {
namespace Languages {

/**
 * @brief Language category tree widget item for hierarchical display
 */
class LanguageCategoryItem : public QTreeWidgetItem {
public:
    LanguageCategoryItem(const QString& category);
    QString categoryName() const { return category_; }
    
private:
    QString category_;
};

/**
 * @brief Language tree widget item for individual language display
 */
class LanguageTreeItem : public QTreeWidgetItem {
public:
    LanguageTreeItem(QTreeWidgetItem* parent, const LanguageConfig& config);
    LanguageType languageType() const { return type_; }
    LanguageConfig config() const { return config_; }
    
private:
    LanguageType type_;
    LanguageConfig config_;
};

/**
 * @brief Details panel for language information and actions
 */
class LanguageDetailsPanel : public QWidget {
    Q_OBJECT
    
public:
    explicit LanguageDetailsPanel(QWidget* parent = nullptr);
    
    void displayLanguage(const LanguageConfig& config);
    void clearDisplay();

private:
    QLabel* nameLabel_;
    QLabel* versionLabel_;
    QLabel* descriptionLabel_;
    QLabel* categoryLabel_;
    QLabel* compilerPathLabel_;
    QLabel* extensionsLabel_;
    QLabel* statusLabel_;
    QPushButton* openHomepageButton_;
    QPushButton* reinstallButton_;
    
private slots:
    void onOpenHomepage();
    void onReinstall();
};

/**
 * @brief Main language management widget
 * 
 * Features:
 * - Hierarchical language browser (Categories → Languages → Details)
 * - Language installation/management interface
 * - Compilation status display
 * - Output window integration
 * - Real-time language detection
 * - Quick access to language tools
 */
class LanguageWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit LanguageWidget(QWidget* parent = nullptr);
    ~LanguageWidget();
    
    // Language selection and management
    void selectLanguage(LanguageType type);
    LanguageType selectedLanguage() const { return currentLanguage_; }
    
    // Display and refresh
    void refreshLanguageList();
    void displayCompilationStatus(const QString& status, bool success = true);
    
    // Output management
    void appendOutput(const QString& text);
    void clearOutput();
    void setOutputHighlight(bool highlight);
    
    // Compilation integration
    void setCompilationInProgress(bool inProgress);
    void updateCompilationProgress(int percentage);
    
signals:
    void languageSelected(LanguageType type);
    void compileRequested(const QString& sourceFile);
    void installRequested(LanguageType type);
    void outputUpdated(const QString& text);
    
private slots:
    void onLanguageSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onCategoryExpanded(QTreeWidgetItem* item);
    void onCategoryCollapsed(QTreeWidgetItem* item);
    void onCompileButtonClicked();
    void onInstallButtonClicked();
    void onRefreshButtonClicked();
    void onSearchTextChanged(const QString& text);
    void onOutputCleared();
    void onOutputExported();
    void onCompilationProgressUpdated(int percentage);
    
private:
    // UI Setup
    void setupUI();
    void connectSignals();
    void createMenuBar();
    
    // Language management
    void populateLanguageTree();
    void filterLanguages(const QString& filter);
    void loadLanguageIcon(QTreeWidgetItem* item, LanguageType type);
    
    // Compilation helpers
    void startCompilation(const QString& sourceFile);
    void finishCompilation(bool success, const QString& message);
    
    // UI Components
    QTreeWidget* languageTree_;
    LanguageDetailsPanel* detailsPanel_;
    QTextEdit* outputWindow_;
    QPushButton* compileButton_;
    QPushButton* installButton_;
    QPushButton* refreshButton_;
    QComboBox* filterComboBox_;
    QLineEdit* searchLineEdit_;
    QProgressBar* compilationProgress_;
    QLabel* statusLabel_;
    
    // State
    LanguageType currentLanguage_;
    QMap<QString, QTreeWidgetItem*> categoryItems_;
    std::shared_ptr<QProcess> compilationProcess_;
    bool isCompiling_;
    
    // Theme support
    void loadStylesheet();
    void updateTheme(const QString& theme);
};

} // namespace Languages
} // namespace RawrXD

#endif // LANGUAGE_WIDGET_HPP
