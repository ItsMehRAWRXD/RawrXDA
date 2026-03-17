/**
 * @file translation_widget.h
 * @brief Header for TranslationWidget - i18n translation helper
 */

#pragma once

#include <QWidget>
#include <QMap>
#include <QString>
#include <QList>
#include <QThread>
#include <QMutex>

class QVBoxLayout;
class QHBoxLayout;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QGroupBox;
class QSplitter;
class QListWidget;
class QLineEdit;
class QCheckBox;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;

struct TranslationEntry {
    QString key;
    QString sourceText;
    QMap<QString, QString> translations; // language -> translated text
    QString context;
    QString filePath;
    int lineNumber;
};

struct TranslationProject {
    QString name;
    QString sourceLanguage;
    QList<QString> targetLanguages;
    QMap<QString, TranslationEntry> entries;
    QString projectPath;
};

class TranslationWorker : public QObject {
    Q_OBJECT
public:
    explicit TranslationWorker(QObject* parent = nullptr);
    ~TranslationWorker() override = default;

    void setApiKey(const QString& key) { apiKey_ = key; }
    void translateText(const QString& text, const QString& fromLang, const QString& toLang, const QString& context = "");

signals:
    void translationFinished(const QString& original, const QString& translated, const QString& targetLang);
    void translationError(const QString& error);
    void progressUpdated(int current, int total);

private:
    QString mockTranslate(const QString& text, const QString& fromLang, const QString& toLang);
    QString apiKey_;
    QMutex mutex_;
};

class TranslationWidget : public QWidget {
    Q_OBJECT
public:
    explicit TranslationWidget(QWidget* parent = nullptr);
    ~TranslationWidget() override;

    // Project management
    bool createProject(const QString& name, const QString& sourceLang, const QList<QString>& targetLangs);
    bool loadProject(const QString& projectPath);
    bool saveProject(const QString& projectPath = QString());

    // Translation operations
    void scanForTranslatableStrings(const QString& directory);
    void exportTranslations(const QString& format = "json");
    void importTranslations(const QString& filePath);

    // Settings
    void setApiKey(const QString& key);
    void setAutoTranslate(bool enabled) { autoTranslate_ = enabled; }

signals:
    void projectLoaded(const QString& projectName);
    void projectSaved(const QString& projectPath);
    void translationCompleted(const QString& key, const QString& language);
    void error(const QString& message);

public slots:
    void refresh();
    void onTranslationFinished(const QString& original, const QString& translated, const QString& targetLang);
    void onTranslationError(const QString& error);

private slots:
    void onNewProject();
    void onOpenProject();
    void onSaveProject();
    void onScanDirectory();
    void onExportTranslations();
    void onImportTranslations();
    void onAddTranslation();
    void onRemoveTranslation();
    void onTranslateSelected();
    void onBatchTranslate();
    void onLanguageChanged();
    void onEntryDoubleClicked(QTreeWidgetItem* item, int column);
    void onFilterChanged(const QString& text);
    void onContextMenuRequested(const QPoint& pos);

private:
    void setupUI();
    void setupToolbar();
    void setupMainArea();
    void setupSidebar();
    void setupConnections();

    void updateProjectTree();
    void updateTranslationEditor();
    void updateLanguageTabs();
    void updateProgress();
    void updateProjectInfo();

    void addTranslationEntry(const QString& key, const QString& sourceText,
                           const QString& context = "", const QString& filePath = "", int lineNumber = -1);
    void removeTranslationEntry(const QString& key);

    QString detectLanguage(const QString& text) const;
    QList<QString> getSupportedLanguages() const;
    QString getLanguageName(const QString& code) const;
    QString getLanguageCode(const QString& name) const;

    void saveState();
    void restoreState();

    // Import/Export helpers
    void exportToJson(const QString& filePath);
    void exportToCsv(const QString& filePath);
    void importFromJson(const QString& filePath);
    void importFromCsv(const QString& filePath);

    // UI components
    QVBoxLayout* mainLayout_;
    QWidget* toolbarWidget_;
    QSplitter* mainSplitter_;

    // Toolbar
    QPushButton* newProjectBtn_;
    QPushButton* openProjectBtn_;
    QPushButton* saveProjectBtn_;
    QPushButton* scanBtn_;
    QPushButton* exportBtn_;
    QPushButton* importBtn_;
    QComboBox* languageCombo_;

    // Main area
    QSplitter* contentSplitter_;
    QTreeWidget* projectTree_;
    QTabWidget* translationTabs_;
    QTextEdit* sourceTextEdit_;
    QMap<QString, QTextEdit*> translationEditors_; // language -> editor

    // Sidebar
    QGroupBox* projectInfoGroup_;
    QLabel* projectNameLabel_;
    QLabel* sourceLangLabel_;
    QListWidget* targetLangsList_;
    QProgressBar* progressBar_;
    QLabel* statusLabel_;

    // Translation worker
    TranslationWorker* worker_;
    QThread* workerThread_;

    // Data
    TranslationProject currentProject_;
    QString currentProjectPath_;
    QString apiKey_;
    bool autoTranslate_;
    int currentTranslationIndex_;
    QStringList supportedLanguages_;
};
