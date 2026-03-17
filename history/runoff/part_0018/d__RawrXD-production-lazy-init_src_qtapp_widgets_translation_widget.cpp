/**
 * @file translation_widget.cpp
 * @brief Implementation of TranslationWidget - i18n translation helper
 */

#include "translation_widget.h"
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QGroupBox>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTabWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QDirIterator>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrlQuery>
#include <QDebug>

// ============================================================================
// TranslationWorker Implementation
// ============================================================================

TranslationWorker::TranslationWorker(QObject* parent)
    : QObject(parent)
{
}

void TranslationWorker::translateText(const QString& text, const QString& fromLang, const QString& toLang, const QString& context)
{
    QMutexLocker locker(&mutex_);
    
    // For demo purposes, we'll simulate translation
    // In a real implementation, this would call a translation API like Google Translate, DeepL, etc.
    
    // Simulate API delay
    QThread::msleep(500);
    
    // Simple mock translation (replace with actual API call)
    QString translated = mockTranslate(text, fromLang, toLang);
    
    emit translationFinished(text, translated, toLang);
}

QString TranslationWorker::mockTranslate(const QString& text, const QString& fromLang, const QString& toLang)
{
    // Simple mock translation for demonstration
    // In production, this would use a real translation service
    QString result = text;
    
    if (fromLang == "en" && toLang == "es") {
        // English to Spanish examples
        if (result.contains("Open", Qt::CaseInsensitive)) return result.replace("Open", "Abrir", Qt::CaseInsensitive);
        if (result.contains("Save", Qt::CaseInsensitive)) return result.replace("Save", "Guardar", Qt::CaseInsensitive);
        if (result.contains("File", Qt::CaseInsensitive)) return result.replace("File", "Archivo", Qt::CaseInsensitive);
        if (result.contains("Edit", Qt::CaseInsensitive)) return result.replace("Edit", "Editar", Qt::CaseInsensitive);
        if (result.contains("View", Qt::CaseInsensitive)) return result.replace("View", "Ver", Qt::CaseInsensitive);
        if (result.contains("Help", Qt::CaseInsensitive)) return result.replace("Help", "Ayuda", Qt::CaseInsensitive);
    }
    
    if (fromLang == "en" && toLang == "fr") {
        // English to French examples
        if (result.contains("Open", Qt::CaseInsensitive)) return result.replace("Open", "Ouvrir", Qt::CaseInsensitive);
        if (result.contains("Save", Qt::CaseInsensitive)) return result.replace("Save", "Enregistrer", Qt::CaseInsensitive);
        if (result.contains("File", Qt::CaseInsensitive)) return result.replace("File", "Fichier", Qt::CaseInsensitive);
        if (result.contains("Edit", Qt::CaseInsensitive)) return result.replace("Edit", "Modifier", Qt::CaseInsensitive);
        if (result.contains("View", Qt::CaseInsensitive)) return result.replace("View", "Voir", Qt::CaseInsensitive);
        if (result.contains("Help", Qt::CaseInsensitive)) return result.replace("Help", "Aide", Qt::CaseInsensitive);
    }
    
    if (fromLang == "en" && toLang == "de") {
        // English to German examples
        if (result.contains("Open", Qt::CaseInsensitive)) return result.replace("Open", "Öffnen", Qt::CaseInsensitive);
        if (result.contains("Save", Qt::CaseInsensitive)) return result.replace("Save", "Speichern", Qt::CaseInsensitive);
        if (result.contains("File", Qt::CaseInsensitive)) return result.replace("File", "Datei", Qt::CaseInsensitive);
        if (result.contains("Edit", Qt::CaseInsensitive)) return result.replace("Edit", "Bearbeiten", Qt::CaseInsensitive);
        if (result.contains("View", Qt::CaseInsensitive)) return result.replace("View", "Ansicht", Qt::CaseInsensitive);
        if (result.contains("Help", Qt::CaseInsensitive)) return result.replace("Help", "Hilfe", Qt::CaseInsensitive);
    }
    
    // If no specific translation found, return original with a note
    return result + " [Translated to " + toLang + "]";
}

// ============================================================================
// TranslationWidget Implementation
// ============================================================================

TranslationWidget::TranslationWidget(QWidget* parent)
    : QWidget(parent)
    , mainLayout_(nullptr)
    , toolbarWidget_(nullptr)
    , mainSplitter_(nullptr)
    , newProjectBtn_(nullptr)
    , openProjectBtn_(nullptr)
    , saveProjectBtn_(nullptr)
    , scanBtn_(nullptr)
    , exportBtn_(nullptr)
    , importBtn_(nullptr)
    , languageCombo_(nullptr)
    , contentSplitter_(nullptr)
    , projectTree_(nullptr)
    , translationTabs_(nullptr)
    , sourceTextEdit_(nullptr)
    , projectInfoGroup_(nullptr)
    , projectNameLabel_(nullptr)
    , sourceLangLabel_(nullptr)
    , targetLangsList_(nullptr)
    , progressBar_(nullptr)
    , statusLabel_(nullptr)
    , worker_(nullptr)
    , workerThread_(nullptr)
    , autoTranslate_(false)
    , currentTranslationIndex_(0)
{
    supportedLanguages_ = {"en", "es", "fr", "de", "it", "pt", "ru", "ja", "ko", "zh", "ar", "hi"};
    
    setupUI();
    setupConnections();
    
    restoreState();
    
    qDebug() << "TranslationWidget initialized";
}

TranslationWidget::~TranslationWidget()
{
    saveState();
    
    if (workerThread_) {
        workerThread_->quit();
        workerThread_->wait();
    }
}

void TranslationWidget::setupUI()
{
    mainLayout_ = new QVBoxLayout(this);
    mainLayout_->setContentsMargins(0, 0, 0, 0);
    mainLayout_->setSpacing(0);
    
    setupToolbar();
    setupMainArea();
    setupSidebar();
    
    // Set initial splitter sizes
    mainSplitter_->setSizes({200, 600});
    contentSplitter_->setSizes({300, 400});
}

void TranslationWidget::setupToolbar()
{
    toolbarWidget_ = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget_);
    toolbarLayout->setContentsMargins(4, 2, 4, 2);
    
    newProjectBtn_ = new QPushButton(tr("New Project"), toolbarWidget_);
    openProjectBtn_ = new QPushButton(tr("Open Project"), toolbarWidget_);
    saveProjectBtn_ = new QPushButton(tr("Save Project"), toolbarWidget_);
    
    toolbarLayout->addWidget(newProjectBtn_);
    toolbarLayout->addWidget(openProjectBtn_);
    toolbarLayout->addWidget(saveProjectBtn_);
    
    // Use spacing instead of addSeparator (not available in QBoxLayout)
    toolbarLayout->addSpacing(10);
    
    scanBtn_ = new QPushButton(tr("Scan Directory"), toolbarWidget_);
    exportBtn_ = new QPushButton(tr("Export"), toolbarWidget_);
    importBtn_ = new QPushButton(tr("Import"), toolbarWidget_);
    
    toolbarLayout->addWidget(scanBtn_);
    toolbarLayout->addWidget(exportBtn_);
    toolbarLayout->addWidget(importBtn_);
    
    toolbarLayout->addStretch();
    
    languageCombo_ = new QComboBox(toolbarWidget_);
    languageCombo_->addItem(tr("All Languages"), "all");
    for (const QString& lang : supportedLanguages_) {
        languageCombo_->addItem(getLanguageName(lang), lang);
    }
    
    toolbarLayout->addWidget(new QLabel(tr("Filter:"), toolbarWidget_));
    toolbarLayout->addWidget(languageCombo_);
    
    mainLayout_->addWidget(toolbarWidget_);
}

void TranslationWidget::setupMainArea()
{
    mainSplitter_ = new QSplitter(Qt::Horizontal, this);
    mainLayout_->addWidget(mainSplitter_);
    
    // Project tree
    projectTree_ = new QTreeWidget(this);
    projectTree_->setHeaderLabel(tr("Translation Keys"));
    projectTree_->setRootIsDecorated(true);
    projectTree_->setAlternatingRowColors(true);
    projectTree_->setContextMenuPolicy(Qt::CustomContextMenu);
    
    mainSplitter_->addWidget(projectTree_);
    
    // Content area
    contentSplitter_ = new QSplitter(Qt::Vertical, this);
    mainSplitter_->addWidget(contentSplitter_);
    
    // Translation tabs
    translationTabs_ = new QTabWidget(this);
    contentSplitter_->addWidget(translationTabs_);
    
    // Source text editor
    sourceTextEdit_ = new QTextEdit(this);
    sourceTextEdit_->setPlaceholderText(tr("Source text will appear here..."));
    sourceTextEdit_->setReadOnly(true);
    
    QWidget* sourceWidget = new QWidget(this);
    QVBoxLayout* sourceLayout = new QVBoxLayout(sourceWidget);
    sourceLayout->addWidget(new QLabel(tr("Source Text:"), sourceWidget));
    sourceLayout->addWidget(sourceTextEdit_);
    
    translationTabs_->addTab(sourceWidget, tr("Source"));
}

void TranslationWidget::setupSidebar()
{
    QWidget* sidebarWidget = new QWidget(this);
    QVBoxLayout* sidebarLayout = new QVBoxLayout(sidebarWidget);
    
    // Project info
    projectInfoGroup_ = new QGroupBox(tr("Project Information"), sidebarWidget);
    QVBoxLayout* infoLayout = new QVBoxLayout(projectInfoGroup_);
    
    projectNameLabel_ = new QLabel(tr("No project loaded"), projectInfoGroup_);
    sourceLangLabel_ = new QLabel(tr("Source: N/A"), projectInfoGroup_);
    
    infoLayout->addWidget(new QLabel(tr("Project:"), projectInfoGroup_));
    infoLayout->addWidget(projectNameLabel_);
    infoLayout->addWidget(new QLabel(tr("Source Language:"), projectInfoGroup_));
    infoLayout->addWidget(sourceLangLabel_);
    
    infoLayout->addWidget(new QLabel(tr("Target Languages:"), projectInfoGroup_));
    targetLangsList_ = new QListWidget(projectInfoGroup_);
    targetLangsList_->setMaximumHeight(100);
    infoLayout->addWidget(targetLangsList_);
    
    sidebarLayout->addWidget(projectInfoGroup_);
    
    // Progress and status
    progressBar_ = new QProgressBar(sidebarWidget);
    progressBar_->setVisible(false);
    sidebarLayout->addWidget(progressBar_);
    
    statusLabel_ = new QLabel(tr("Ready"), sidebarWidget);
    statusLabel_->setWordWrap(true);
    sidebarLayout->addWidget(statusLabel_);
    
    sidebarLayout->addStretch();
    
    mainSplitter_->addWidget(sidebarWidget);
}

void TranslationWidget::setupConnections()
{
    // Toolbar actions
    connect(newProjectBtn_, &QPushButton::clicked, this, &TranslationWidget::onNewProject);
    connect(openProjectBtn_, &QPushButton::clicked, this, &TranslationWidget::onOpenProject);
    connect(saveProjectBtn_, &QPushButton::clicked, this, &TranslationWidget::onSaveProject);
    connect(scanBtn_, &QPushButton::clicked, this, &TranslationWidget::onScanDirectory);
    connect(exportBtn_, &QPushButton::clicked, this, &TranslationWidget::onExportTranslations);
    connect(importBtn_, &QPushButton::clicked, this, &TranslationWidget::onImportTranslations);
    
    // Project tree
    connect(projectTree_, &QTreeWidget::itemDoubleClicked, this, &TranslationWidget::onEntryDoubleClicked);
    connect(projectTree_, &QWidget::customContextMenuRequested, this, &TranslationWidget::onContextMenuRequested);
    
    // Language filter
    connect(languageCombo_, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TranslationWidget::onLanguageChanged);
    
    // Worker connections
    if (!workerThread_) {
        workerThread_ = new QThread(this);
        worker_ = new TranslationWorker();
        worker_->moveToThread(workerThread_);
        workerThread_->start();
    }
    
    connect(worker_, &TranslationWorker::translationFinished, this, &TranslationWidget::onTranslationFinished);
    connect(worker_, &TranslationWorker::translationError, this, &TranslationWidget::onTranslationError);
}

bool TranslationWidget::createProject(const QString& name, const QString& sourceLang, const QList<QString>& targetLangs)
{
    currentProject_.name = name;
    currentProject_.sourceLanguage = sourceLang;
    currentProject_.targetLanguages = targetLangs;
    currentProject_.entries.clear();
    currentProject_.projectPath.clear();
    
    updateProjectTree();
    updateLanguageTabs();
    updateProjectInfo();
    
    emit projectLoaded(name);
    return true;
}

bool TranslationWidget::loadProject(const QString& projectPath)
{
    QFile file(projectPath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit error(tr("Failed to open project file: %1").arg(projectPath));
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        emit error(tr("Invalid project file format"));
        return false;
    }
    
    QJsonObject obj = doc.object();
    
    currentProject_.name = obj["name"].toString();
    currentProject_.sourceLanguage = obj["sourceLanguage"].toString();
    currentProject_.projectPath = projectPath;
    
    // Load target languages
    QJsonArray langsArray = obj["targetLanguages"].toArray();
    currentProject_.targetLanguages.clear();
    for (const QJsonValue& val : langsArray) {
        currentProject_.targetLanguages.append(val.toString());
    }
    
    // Load entries
    QJsonObject entriesObj = obj["entries"].toObject();
    currentProject_.entries.clear();
    
    for (const QString& key : entriesObj.keys()) {
        QJsonObject entryObj = entriesObj[key].toObject();
        
        TranslationEntry entry;
        entry.key = key;
        entry.sourceText = entryObj["sourceText"].toString();
        entry.context = entryObj["context"].toString();
        entry.filePath = entryObj["filePath"].toString();
        entry.lineNumber = entryObj["lineNumber"].toInt();
        
        QJsonObject translationsObj = entryObj["translations"].toObject();
        for (const QString& lang : translationsObj.keys()) {
            entry.translations[lang] = translationsObj[lang].toString();
        }
        
        currentProject_.entries[key] = entry;
    }
    
    updateProjectTree();
    updateLanguageTabs();
    updateProjectInfo();
    
    emit projectLoaded(currentProject_.name);
    return true;
}

bool TranslationWidget::saveProject(const QString& projectPath)
{
    QString savePath = projectPath.isEmpty() ? currentProjectPath_ : projectPath;
    
    if (savePath.isEmpty()) {
        savePath = QFileDialog::getSaveFileName(this, tr("Save Translation Project"), 
                                              QString(), tr("Translation Projects (*.i18n)"));
        if (savePath.isEmpty()) {
            return false;
        }
    }
    
    QJsonObject obj;
    obj["name"] = currentProject_.name;
    obj["sourceLanguage"] = currentProject_.sourceLanguage;
    
    QJsonArray langsArray;
    for (const QString& lang : currentProject_.targetLanguages) {
        langsArray.append(lang);
    }
    obj["targetLanguages"] = langsArray;
    
    QJsonObject entriesObj;
    for (const auto& entry : currentProject_.entries) {
        QJsonObject entryObj;
        entryObj["sourceText"] = entry.sourceText;
        entryObj["context"] = entry.context;
        entryObj["filePath"] = entry.filePath;
        entryObj["lineNumber"] = entry.lineNumber;
        
        QJsonObject translationsObj;
        for (auto it = entry.translations.begin(); it != entry.translations.end(); ++it) {
            translationsObj[it.key()] = it.value();
        }
        entryObj["translations"] = translationsObj;
        
        entriesObj[entry.key] = entryObj;
    }
    obj["entries"] = entriesObj;
    
    QJsonDocument doc(obj);
    
    QFile file(savePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit error(tr("Failed to save project file: %1").arg(savePath));
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    currentProjectPath_ = savePath;
    emit projectSaved(savePath);
    return true;
}

void TranslationWidget::scanForTranslatableStrings(const QString& directory)
{
    QDir dir(directory);
    if (!dir.exists()) {
        emit error(tr("Directory does not exist: %1").arg(directory));
        return;
    }
    
    statusLabel_->setText(tr("Scanning for translatable strings..."));
    progressBar_->setVisible(true);
    progressBar_->setRange(0, 0); // Indeterminate progress
    
    QApplication::processEvents();
    
    // Common patterns for translatable strings
    QList<QRegularExpression> patterns = {
        QRegularExpression("tr\\(\"([^\"]*)\""),  // Qt tr() calls
        QRegularExpression("QObject::tr\\(\"([^\"]*)\""), // QObject::tr() calls
        QRegularExpression("_\\(\"([^\"]*)\""),   // gettext _() calls
        QRegularExpression("gettext\\(\"([^\"]*)\""), // gettext() calls
        QRegularExpression("i18n\\(\"([^\"]*)\""), // KDE i18n calls
    };
    
    QStringList fileExtensions = {"*.cpp", "*.h", "*.hpp", "*.cxx", "*.cc", "*.js", "*.ts", "*.py", "*.java"};
    
    int totalFiles = 0;
    for (const QString& ext : fileExtensions) {
        QDirIterator it(directory, QStringList() << ext, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            totalFiles++;
        }
    }
    
    progressBar_->setRange(0, totalFiles);
    progressBar_->setValue(0);
    
    int processedFiles = 0;
    
    for (const QString& ext : fileExtensions) {
        QDirIterator it(directory, QStringList() << ext, QDir::Files, QDirIterator::Subdirectories);
        
        while (it.hasNext()) {
            it.next();
            const QString filePath = it.filePath();
            
            QFile file(filePath);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                continue;
            }
            
            QString content = QString::fromUtf8(file.readAll());
            file.close();
            
            QStringList lines = content.split('\n');
            int lineNumber = 1;
            
            for (const QString& line : lines) {
                for (const QRegularExpression& pattern : patterns) {
                    QRegularExpressionMatchIterator matches = pattern.globalMatch(line);
                    while (matches.hasNext()) {
                        QRegularExpressionMatch match = matches.next();
                        if (match.hasMatch()) {
                            QString extractedText = match.captured(1);
                            if (!extractedText.isEmpty()) {
                                QString key = QString("%1_%2").arg(QFileInfo(filePath).baseName()).arg(lineNumber);
                                addTranslationEntry(key, extractedText, "", filePath, lineNumber);
                            }
                        }
                    }
                }
                lineNumber++;
            }
            
            processedFiles++;
            progressBar_->setValue(processedFiles);
            QApplication::processEvents();
        }
    }
    
    progressBar_->setVisible(false);
    statusLabel_->setText(tr("Scan completed. Found %1 translatable strings.").arg(currentProject_.entries.size()));
    
    updateProjectTree();
}

void TranslationWidget::exportTranslations(const QString& format)
{
    if (currentProject_.entries.isEmpty()) {
        QMessageBox::information(this, tr("Export"), tr("No translations to export."));
        return;
    }
    
    QString fileName = QFileDialog::getSaveFileName(this, tr("Export Translations"), 
                                                  QString(), tr("JSON files (*.json);;CSV files (*.csv);;All files (*)"));
    if (fileName.isEmpty()) {
        return;
    }
    
    if (format == "json" || fileName.endsWith(".json")) {
        exportToJson(fileName);
    } else if (format == "csv" || fileName.endsWith(".csv")) {
        exportToCsv(fileName);
    }
}

void TranslationWidget::importTranslations(const QString& filePath)
{
    QString importPath = filePath;
    if (importPath.isEmpty()) {
        importPath = QFileDialog::getOpenFileName(this, tr("Import Translations"), 
                                                QString(), tr("JSON files (*.json);;CSV files (*.csv);;All files (*)"));
        if (importPath.isEmpty()) {
            return;
        }
    }
    
    if (importPath.endsWith(".json")) {
        importFromJson(importPath);
    } else if (importPath.endsWith(".csv")) {
        importFromCsv(importPath);
    }
}

void TranslationWidget::setApiKey(const QString& key)
{
    apiKey_ = key;
    if (worker_) {
        worker_->setApiKey(key);
    }
}

void TranslationWidget::refresh()
{
    updateProjectTree();
    updateLanguageTabs();
    updateProjectInfo();
}

void TranslationWidget::onTranslationFinished(const QString& original, const QString& translated, const QString& targetLang)
{
    // Find the entry that was being translated
    for (auto& entry : currentProject_.entries) {
        if (entry.sourceText == original) {
            entry.translations[targetLang] = translated;
            emit translationCompleted(entry.key, targetLang);
            break;
        }
    }
    
    updateTranslationEditor();
    currentTranslationIndex_++;
    updateProgress();
}

void TranslationWidget::onTranslationError(const QString& error)
{
    emit this->error(error);
    currentTranslationIndex_++;
    updateProgress();
}

void TranslationWidget::onNewProject()
{
    bool ok;
    QString name = QInputDialog::getText(this, tr("New Translation Project"), 
                                       tr("Project name:"), QLineEdit::Normal, "", &ok);
    if (!ok || name.isEmpty()) {
        return;
    }
    
    QString sourceLang = QInputDialog::getItem(this, tr("Source Language"), 
                                             tr("Select source language:"), 
                                             getSupportedLanguages(), 0, false, &ok);
    if (!ok) return;
    
    QList<QString> targetLangs;
    QStringList availableLangs = getSupportedLanguages();
    availableLangs.removeAll(sourceLang);
    
    bool finished = false;
    while (!finished) {
        QString lang = QInputDialog::getItem(this, tr("Target Languages"), 
                                           tr("Select target language (Cancel when done):"), 
                                           availableLangs, 0, true, &ok);
        if (!ok) {
            finished = true;
        } else {
            targetLangs.append(getLanguageCode(lang));
            availableLangs.removeAll(lang);
            if (availableLangs.isEmpty()) finished = true;
        }
    }
    
    if (targetLangs.isEmpty()) {
        QMessageBox::warning(this, tr("Warning"), tr("No target languages selected."));
        return;
    }
    
    createProject(name, getLanguageCode(sourceLang), targetLangs);
}

void TranslationWidget::onOpenProject()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open Translation Project"), 
                                                  QString(), tr("Translation Projects (*.i18n);;All files (*)"));
    if (!fileName.isEmpty()) {
        loadProject(fileName);
    }
}

void TranslationWidget::onSaveProject()
{
    saveProject();
}

void TranslationWidget::onScanDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(this, tr("Select Directory to Scan"));
    if (!directory.isEmpty()) {
        scanForTranslatableStrings(directory);
    }
}

void TranslationWidget::onExportTranslations()
{
    exportTranslations();
}

void TranslationWidget::onImportTranslations()
{
    importTranslations(QString());
}

void TranslationWidget::onAddTranslation()
{
    bool ok;
    QString key = QInputDialog::getText(this, tr("Add Translation"), 
                                      tr("Translation key:"), QLineEdit::Normal, "", &ok);
    if (!ok || key.isEmpty()) return;
    
    QString sourceText = QInputDialog::getText(this, tr("Add Translation"), 
                                             tr("Source text:"), QLineEdit::Normal, "", &ok);
    if (!ok || sourceText.isEmpty()) return;
    
    addTranslationEntry(key, sourceText);
    updateProjectTree();
}

void TranslationWidget::onRemoveTranslation()
{
    QList<QTreeWidgetItem*> selected = projectTree_->selectedItems();
    if (selected.isEmpty()) return;
    
    QTreeWidgetItem* item = selected.first();
    QString key = item->text(0);
    
    if (QMessageBox::question(this, tr("Remove Translation"), 
                            tr("Remove translation '%1'?").arg(key)) == QMessageBox::Yes) {
        removeTranslationEntry(key);
        updateProjectTree();
    }
}

void TranslationWidget::onTranslateSelected()
{
    QList<QTreeWidgetItem*> selected = projectTree_->selectedItems();
    if (selected.isEmpty()) return;
    
    QTreeWidgetItem* item = selected.first();
    QString key = item->text(0);
    
    if (!currentProject_.entries.contains(key)) return;
    
    const TranslationEntry& entry = currentProject_.entries[key];
    
    for (const QString& targetLang : currentProject_.targetLanguages) {
        if (!entry.translations.contains(targetLang) || entry.translations[targetLang].isEmpty()) {
            QMetaObject::invokeMethod(worker_, "translateText", 
                                    Qt::QueuedConnection,
                                    Q_ARG(QString, entry.sourceText),
                                    Q_ARG(QString, currentProject_.sourceLanguage),
                                    Q_ARG(QString, targetLang),
                                    Q_ARG(QString, entry.context));
        }
    }
}

void TranslationWidget::onBatchTranslate()
{
    if (currentProject_.entries.isEmpty()) {
        QMessageBox::information(this, tr("Batch Translate"), tr("No entries to translate."));
        return;
    }
    
    currentTranslationIndex_ = 0;
    progressBar_->setVisible(true);
    progressBar_->setRange(0, currentProject_.entries.size() * currentProject_.targetLanguages.size());
    progressBar_->setValue(0);
    
    statusLabel_->setText(tr("Translating..."));
    
    for (const auto& entry : currentProject_.entries) {
        for (const QString& targetLang : currentProject_.targetLanguages) {
            if (!entry.translations.contains(targetLang) || entry.translations[targetLang].isEmpty()) {
                QMetaObject::invokeMethod(worker_, "translateText", 
                                        Qt::QueuedConnection,
                                        Q_ARG(QString, entry.sourceText),
                                        Q_ARG(QString, currentProject_.sourceLanguage),
                                        Q_ARG(QString, targetLang),
                                        Q_ARG(QString, entry.context));
            } else {
                currentTranslationIndex_++;
                updateProgress();
            }
        }
    }
}

void TranslationWidget::onLanguageChanged()
{
    updateProjectTree();
}

void TranslationWidget::onEntryDoubleClicked(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column)
    if (!item) return;
    
    QString key = item->text(0);
    if (currentProject_.entries.contains(key)) {
        updateTranslationEditor();
    }
}

void TranslationWidget::onFilterChanged(const QString& text)
{
    // Implement filtering logic
    Q_UNUSED(text)
}

void TranslationWidget::onContextMenuRequested(const QPoint& pos)
{
    QTreeWidgetItem* item = projectTree_->itemAt(pos);
    
    QMenu menu(this);
    
    if (item) {
        menu.addAction(tr("Translate Selected"), this, &TranslationWidget::onTranslateSelected);
        menu.addAction(tr("Remove Translation"), this, &TranslationWidget::onRemoveTranslation);
        menu.addSeparator();
    }
    
    menu.addAction(tr("Add Translation"), this, &TranslationWidget::onAddTranslation);
    menu.addAction(tr("Batch Translate All"), this, &TranslationWidget::onBatchTranslate);
    
    if (!menu.isEmpty()) {
        menu.exec(projectTree_->mapToGlobal(pos));
    }
}

void TranslationWidget::updateProjectTree()
{
    projectTree_->clear();
    
    QString filterLang = languageCombo_->currentData().toString();
    
    for (const auto& entry : currentProject_.entries) {
        // Apply language filter
        if (filterLang != "all") {
            if (!entry.translations.contains(filterLang) || entry.translations[filterLang].isEmpty()) {
                continue;
            }
        }
        
        QTreeWidgetItem* item = new QTreeWidgetItem(projectTree_);
        item->setText(0, entry.key);
        
        // Add status indicators
        QString status;
        int translatedCount = 0;
        for (const QString& lang : currentProject_.targetLanguages) {
            if (entry.translations.contains(lang) && !entry.translations[lang].isEmpty()) {
                translatedCount++;
            }
        }
        
        if (translatedCount == 0) {
            status = tr("Untranslated");
            item->setIcon(0, QIcon(":/icons/untranslated.png"));
        } else if (translatedCount == currentProject_.targetLanguages.size()) {
            status = tr("Complete");
            item->setIcon(0, QIcon(":/icons/complete.png"));
        } else {
            status = tr("Partial (%1/%2)").arg(translatedCount).arg(currentProject_.targetLanguages.size());
            item->setIcon(0, QIcon(":/icons/partial.png"));
        }
        
        item->setText(1, status);
        item->setText(2, entry.sourceText);
    }
    
    projectTree_->resizeColumnToContents(0);
}

void TranslationWidget::updateTranslationEditor()
{
    QList<QTreeWidgetItem*> selected = projectTree_->selectedItems();
    if (selected.isEmpty()) return;
    
    QTreeWidgetItem* item = selected.first();
    QString key = item->text(0);
    
    if (!currentProject_.entries.contains(key)) return;
    
    const TranslationEntry& entry = currentProject_.entries[key];
    
    sourceTextEdit_->setText(entry.sourceText);
    
    // Update translation editors
    for (const QString& lang : currentProject_.targetLanguages) {
        if (translationEditors_.contains(lang)) {
            QString translation = entry.translations.value(lang, "");
            translationEditors_[lang]->setText(translation);
        }
    }
}

void TranslationWidget::updateLanguageTabs()
{
    // Clear existing translation tabs (keep source tab)
    while (translationTabs_->count() > 1) {
        QWidget* widget = translationTabs_->widget(1);
        translationTabs_->removeTab(1);
        delete widget;
    }
    
    translationEditors_.clear();
    
    // Add translation tabs for each target language
    for (const QString& lang : currentProject_.targetLanguages) {
        QTextEdit* editor = new QTextEdit(this);
        editor->setPlaceholderText(tr("Translation for %1...").arg(getLanguageName(lang)));
        
        QWidget* tabWidget = new QWidget(this);
        QVBoxLayout* tabLayout = new QVBoxLayout(tabWidget);
        tabLayout->addWidget(new QLabel(tr("Translation (%1):").arg(getLanguageName(lang)), tabWidget));
        tabLayout->addWidget(editor);
        
        translationTabs_->addTab(tabWidget, getLanguageName(lang));
        translationEditors_[lang] = editor;
        
        // Connect text changes to update the project data
        connect(editor, &QTextEdit::textChanged, [this, lang, key = QString()]() {
            QList<QTreeWidgetItem*> selected = projectTree_->selectedItems();
            if (!selected.isEmpty()) {
                QString currentKey = selected.first()->text(0);
                if (currentProject_.entries.contains(currentKey)) {
                    currentProject_.entries[currentKey].translations[lang] = translationEditors_[lang]->toPlainText();
                }
            }
        });
    }
}

void TranslationWidget::updateProgress()
{
    progressBar_->setValue(currentTranslationIndex_);
    
    if (currentTranslationIndex_ >= progressBar_->maximum()) {
        progressBar_->setVisible(false);
        statusLabel_->setText(tr("Translation completed."));
    }
}

void TranslationWidget::addTranslationEntry(const QString& key, const QString& sourceText, 
                                          const QString& context, const QString& filePath, int lineNumber)
{
    TranslationEntry entry;
    entry.key = key;
    entry.sourceText = sourceText;
    entry.context = context;
    entry.filePath = filePath;
    entry.lineNumber = lineNumber;
    
    currentProject_.entries[key] = entry;
}

void TranslationWidget::removeTranslationEntry(const QString& key)
{
    currentProject_.entries.remove(key);
}

QString TranslationWidget::detectLanguage(const QString& text) const
{
    // Simple language detection (very basic)
    // In production, use a proper language detection library
    
    if (text.contains("el ") || text.contains("la ")) return "es";
    if (text.contains("le ") || text.contains("la ")) return "fr";
    if (text.contains("der ") || text.contains("die ") || text.contains("das ")) return "de";
    
    return "en"; // Default to English
}

QList<QString> TranslationWidget::getSupportedLanguages() const
{
    QList<QString> names;
    for (const QString& code : supportedLanguages_) {
        names.append(getLanguageName(code));
    }
    return names;
}

QString TranslationWidget::getLanguageName(const QString& code) const
{
    static QMap<QString, QString> languageNames = {
        {"en", "English"}, {"es", "Spanish"}, {"fr", "French"}, {"de", "German"},
        {"it", "Italian"}, {"pt", "Portuguese"}, {"ru", "Russian"}, {"ja", "Japanese"},
        {"ko", "Korean"}, {"zh", "Chinese"}, {"ar", "Arabic"}, {"hi", "Hindi"}
    };
    
    return languageNames.value(code, code.toUpper());
}

QString TranslationWidget::getLanguageCode(const QString& name) const
{
    static QMap<QString, QString> languageCodes = {
        {"English", "en"}, {"Spanish", "es"}, {"French", "fr"}, {"German", "de"},
        {"Italian", "it"}, {"Portuguese", "pt"}, {"Russian", "ru"}, {"Japanese", "ja"},
        {"Korean", "ko"}, {"Chinese", "zh"}, {"Arabic", "ar"}, {"Hindi", "hi"}
    };
    
    return languageCodes.value(name, "en");
}

void TranslationWidget::updateProjectInfo()
{
    projectNameLabel_->setText(currentProject_.name.isEmpty() ? tr("No project loaded") : currentProject_.name);
    sourceLangLabel_->setText(tr("Source: %1").arg(getLanguageName(currentProject_.sourceLanguage)));
    
    targetLangsList_->clear();
    for (const QString& lang : currentProject_.targetLanguages) {
        targetLangsList_->addItem(getLanguageName(lang));
    }
}

void TranslationWidget::saveState()
{
    QSettings settings;
    settings.beginGroup("TranslationWidget");
    settings.setValue("mainSplitterSizes", mainSplitter_->saveState());
    settings.setValue("contentSplitterSizes", contentSplitter_->saveState());
    settings.setValue("apiKey", apiKey_);
    settings.setValue("autoTranslate", autoTranslate_);
    settings.endGroup();
}

void TranslationWidget::restoreState()
{
    QSettings settings;
    settings.beginGroup("TranslationWidget");
    
    if (settings.contains("mainSplitterSizes")) {
        mainSplitter_->restoreState(settings.value("mainSplitterSizes").toByteArray());
    }
    
    if (settings.contains("contentSplitterSizes")) {
        contentSplitter_->restoreState(settings.value("contentSplitterSizes").toByteArray());
    }
    
    apiKey_ = settings.value("apiKey").toString();
    autoTranslate_ = settings.value("autoTranslate", false).toBool();
    
    if (worker_) {
        worker_->setApiKey(apiKey_);
    }
    
    settings.endGroup();
}

void TranslationWidget::exportToJson(const QString& filePath)
{
    QJsonObject obj;
    obj["project"] = currentProject_.name;
    obj["sourceLanguage"] = currentProject_.sourceLanguage;
    
    QJsonArray entriesArray;
    for (const auto& entry : currentProject_.entries) {
        QJsonObject entryObj;
        entryObj["key"] = entry.key;
        entryObj["source"] = entry.sourceText;
        entryObj["context"] = entry.context;
        
        QJsonObject translationsObj;
        for (auto it = entry.translations.begin(); it != entry.translations.end(); ++it) {
            translationsObj[it.key()] = it.value();
        }
        entryObj["translations"] = translationsObj;
        
        entriesArray.append(entryObj);
    }
    obj["entries"] = entriesArray;
    
    QJsonDocument doc(obj);
    
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
    }
}

void TranslationWidget::exportToCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream out(&file);
    
    // Header
    QStringList header = {"Key", "Source", "Context"};
    header.append(currentProject_.targetLanguages);
    out << header.join(",") << "\n";
    
    // Data
    for (const auto& entry : currentProject_.entries) {
        QStringList row;
        row << entry.key << entry.sourceText << entry.context;
        
        for (const QString& lang : currentProject_.targetLanguages) {
            row << entry.translations.value(lang, "");
        }
        
        out << row.join(",") << "\n";
    }
    
    file.close();
}

void TranslationWidget::importFromJson(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (!doc.isObject()) {
        return;
    }
    
    QJsonObject obj = doc.object();
    QJsonArray entriesArray = obj["entries"].toArray();
    
    for (const QJsonValue& val : entriesArray) {
        QJsonObject entryObj = val.toObject();
        
        QString key = entryObj["key"].toString();
        QString source = entryObj["source"].toString();
        QString context = entryObj["context"].toString();
        
        addTranslationEntry(key, source, context);
        
        QJsonObject translationsObj = entryObj["translations"].toObject();
        for (const QString& lang : translationsObj.keys()) {
            currentProject_.entries[key].translations[lang] = translationsObj[lang].toString();
        }
    }
    
    updateProjectTree();
    updateLanguageTabs();
}

void TranslationWidget::importFromCsv(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return;
    }
    
    QTextStream in(&file);
    QStringList lines = in.readAll().split('\n');
    file.close();
    
    if (lines.isEmpty()) return;
    
    // Parse header
    QStringList header = lines.first().split(',');
    lines.removeFirst();
    
    // Find language columns (skip Key, Source, Context)
    QStringList languages;
    for (int i = 3; i < header.size(); ++i) {
        languages.append(header[i].trimmed());
    }
    
    // Parse data
    for (const QString& line : lines) {
        if (line.trimmed().isEmpty()) continue;
        
        QStringList fields = line.split(',');
        if (fields.size() < 3) continue;
        
        QString key = fields[0].trimmed();
        QString source = fields[1].trimmed();
        QString context = fields[2].trimmed();
        
        addTranslationEntry(key, source, context);
        
        // Add translations
        for (int i = 0; i < languages.size() && i + 3 < fields.size(); ++i) {
            QString lang = languages[i];
            QString translation = fields[i + 3].trimmed();
            if (!translation.isEmpty()) {
                currentProject_.entries[key].translations[lang] = translation;
            }
        }
    }
    
    updateProjectTree();
    updateLanguageTabs();
}