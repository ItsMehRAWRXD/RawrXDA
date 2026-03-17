/**
 * @file snippet_manager_widget.h
 * @brief Full Snippet Manager widget for RawrXD IDE
 * @author RawrXD Team
 * 
 * Provides comprehensive code snippet management including:
 * - Built-in snippets for multiple languages
 * - User-defined custom snippets
 * - VSCode snippet format compatibility
 * - Tab stops and placeholder support
 * - Variable substitution
 * - Snippet search and categories
 */

#pragma once

#include <QWidget>
#include <QTreeWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QSplitter>
#include <QSettings>
#include <QJsonObject>
#include <QJsonArray>
#include <QDialog>
#include <QMap>

/**
 * @brief Represents a code snippet with placeholders
 */
struct Snippet {
    QString name;
    QString prefix;         // Trigger text
    QString description;
    QString body;           // Raw body with $1, ${2:placeholder}, etc.
    QString scope;          // Language scope (e.g., "cpp", "python")
    QString category;
    QStringList keywords;
    bool isBuiltIn = false;
    bool isEnabled = true;
    
    QJsonObject toJson() const;
    static Snippet fromJson(const QJsonObject& obj);
    static Snippet fromVSCodeFormat(const QString& name, const QJsonObject& obj);
    
    // Expand snippet body with variable substitution
    QString expandBody(const QMap<QString, QString>& variables = {}) const;
    
    // Get list of tab stop indices
    QVector<int> getTabStops() const;
    
    // Get placeholder text for a tab stop
    QString getPlaceholder(int tabStop) const;
};

/**
 * @class SnippetManagerWidget
 * @brief Full-featured code snippet management widget
 */
class SnippetManagerWidget : public QWidget {
    Q_OBJECT

public:
    explicit SnippetManagerWidget(QWidget* parent = nullptr);
    ~SnippetManagerWidget() override;

    // Snippet access
    QVector<Snippet> getSnippets(const QString& language = QString()) const;
    QVector<Snippet> searchSnippets(const QString& query, const QString& language = QString()) const;
    Snippet* findSnippet(const QString& prefix, const QString& language);
    Snippet* findSnippetByName(const QString& name);
    
    // Snippet management
    void addSnippet(const Snippet& snippet);
    void updateSnippet(const QString& name, const Snippet& snippet);
    void removeSnippet(const QString& name);
    void enableSnippet(const QString& name, bool enabled);
    
    // Import/Export
    void importVSCodeSnippets(const QString& path);
    void exportSnippets(const QString& path);
    void loadBuiltInSnippets();
    
    // Categories
    QStringList getCategories() const;
    QStringList getLanguages() const;

signals:
    void snippetSelected(const Snippet& snippet);
    void snippetInsertRequested(const QString& expandedBody);
    void snippetsChanged();

public slots:
    void onSearchChanged(const QString& text);
    void onLanguageFilterChanged(const QString& language);
    void onCategoryFilterChanged(const QString& category);
    void onSnippetSelected(QTreeWidgetItem* item, int column);
    void onSnippetDoubleClicked(QTreeWidgetItem* item, int column);
    void onNewSnippet();
    void onEditSnippet();
    void onDeleteSnippet();
    void onImportSnippets();
    void onExportSnippets();
    void refreshView();

private:
    void setupUI();
    void loadSnippets();
    void saveSnippets();
    void updatePreview(const Snippet& snippet);
    void populateBuiltIns();
    QString formatSnippetPreview(const Snippet& snippet) const;

private:
    // UI Components
    QSplitter* m_splitter = nullptr;
    QTreeWidget* m_snippetTree = nullptr;
    QTextEdit* m_previewEdit = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QComboBox* m_languageFilter = nullptr;
    QComboBox* m_categoryFilter = nullptr;
    QPushButton* m_newBtn = nullptr;
    QPushButton* m_editBtn = nullptr;
    QPushButton* m_deleteBtn = nullptr;
    QPushButton* m_importBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    
    // Data
    QVector<Snippet> m_snippets;
    QMap<QString, QVector<Snippet*>> m_snippetsByLanguage;
    QMap<QString, QVector<Snippet*>> m_snippetsByCategory;
    
    // Current selection
    Snippet* m_currentSnippet = nullptr;
    
    // Settings
    QSettings* m_settings = nullptr;
};

/**
 * @class SnippetEditDialog
 * @brief Dialog for creating/editing snippets
 */
class SnippetEditDialog : public QDialog {
    Q_OBJECT

public:
    explicit SnippetEditDialog(QWidget* parent = nullptr);
    
    void setSnippet(const Snippet& snippet);
    Snippet getSnippet() const;

private slots:
    void onPreviewBody();
    void validateInput();

private:
    void setupUI();
    
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_prefixEdit = nullptr;
    QLineEdit* m_descEdit = nullptr;
    QTextEdit* m_bodyEdit = nullptr;
    QComboBox* m_scopeCombo = nullptr;
    QComboBox* m_categoryCombo = nullptr;
    QLineEdit* m_keywordsEdit = nullptr;
    QTextEdit* m_previewEdit = nullptr;
    QPushButton* m_okBtn = nullptr;
};

