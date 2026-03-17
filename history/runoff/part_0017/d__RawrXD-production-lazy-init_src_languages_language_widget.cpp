// ===============================================================================
// Language Management Widget Implementation
// ===============================================================================

#include "language_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QHeaderView>
#include <QLineEdit>
#include <QMessageBox>
#include <QFileDialog>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QApplication>
#include <QDebug>

namespace RawrXD {
namespace Languages {

// ===============================================================================
// CATEGORY ITEM IMPLEMENTATION
// ===============================================================================

LanguageCategoryItem::LanguageCategoryItem(const QString& category)
    : QTreeWidgetItem(), category_(category) {
    setText(0, category);
    setIcon(0, QIcon(":/icons/folder.png"));
}

// ===============================================================================
// LANGUAGE TREE ITEM IMPLEMENTATION
// ===============================================================================

LanguageTreeItem::LanguageTreeItem(QTreeWidgetItem* parent, const LanguageConfig& config)
    : QTreeWidgetItem(parent), type_(config.type), config_(config) {
    setText(0, config.displayName);
    setText(1, config.category);
    
    QString statusText = config.isSupported ? "Supported" : "Not Supported";
    setText(2, statusText);
    
    // Set color based on support status
    if (config.isSupported) {
        setTextColor(0, QColor(0, 128, 0));
    } else {
        setTextColor(0, QColor(128, 128, 128));
    }
}

// ===============================================================================
// DETAILS PANEL IMPLEMENTATION
// ===============================================================================

LanguageDetailsPanel::LanguageDetailsPanel(QWidget* parent)
    : QWidget(parent) {
    
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // Name label
    nameLabel_ = new QLabel(this);
    nameLabel_->setStyleSheet("font-size: 16px; font-weight: bold;");
    mainLayout->addWidget(nameLabel_);
    
    // Version label
    versionLabel_ = new QLabel(this);
    mainLayout->addWidget(versionLabel_);
    
    // Description label
    descriptionLabel_ = new QLabel(this);
    descriptionLabel_->setWordWrap(true);
    descriptionLabel_->setStyleSheet("color: #666;");
    mainLayout->addWidget(descriptionLabel_);
    
    // Category label
    categoryLabel_ = new QLabel(this);
    categoryLabel_->setStyleSheet("color: #0066CC;");
    mainLayout->addWidget(categoryLabel_);
    
    // Compiler path label
    compilerPathLabel_ = new QLabel(this);
    compilerPathLabel_->setStyleSheet("font-family: monospace; color: #666;");
    mainLayout->addWidget(compilerPathLabel_);
    
    // Extensions label
    extensionsLabel_ = new QLabel(this);
    extensionsLabel_->setStyleSheet("color: #666;");
    mainLayout->addWidget(extensionsLabel_);
    
    // Status label
    statusLabel_ = new QLabel(this);
    statusLabel_->setStyleSheet("font-weight: bold; color: #008000;");
    mainLayout->addWidget(statusLabel_);
    
    // Buttons
    auto buttonLayout = new QHBoxLayout();
    
    openHomepageButton_ = new QPushButton("Open Homepage", this);
    connect(openHomepageButton_, &QPushButton::clicked, this, &LanguageDetailsPanel::onOpenHomepage);
    buttonLayout->addWidget(openHomepageButton_);
    
    reinstallButton_ = new QPushButton("Reinstall", this);
    connect(reinstallButton_, &QPushButton::clicked, this, &LanguageDetailsPanel::onReinstall);
    buttonLayout->addWidget(reinstallButton_);
    
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    mainLayout->addStretch();
    
    setStyleSheet(
        "QLabel { padding: 4px; }"
        "QPushButton { padding: 6px 12px; border-radius: 3px; background-color: #0066CC; color: white; }"
        "QPushButton:hover { background-color: #0052A3; }"
    );
}

void LanguageDetailsPanel::displayLanguage(const LanguageConfig& config) {
    nameLabel_->setText(config.displayName);
    versionLabel_->setText("Version: Unknown (Not Implemented)");
    descriptionLabel_->setText(config.description);
    categoryLabel_->setText("Category: " + config.category);
    compilerPathLabel_->setText("Compiler: " + config.compilerPath);
    
    QString extensions = "Extensions: ";
    for (const auto& ext : config.extensions) {
        extensions += ext + " ";
    }
    extensionsLabel_->setText(extensions);
    
    statusLabel_->setText(config.isSupported ? "✓ Supported" : "✗ Not Supported");
    statusLabel_->setStyleSheet(config.isSupported 
        ? "font-weight: bold; color: #008000;" 
        : "font-weight: bold; color: #CC0000;");
}

void LanguageDetailsPanel::clearDisplay() {
    nameLabel_->clear();
    versionLabel_->clear();
    descriptionLabel_->clear();
    categoryLabel_->clear();
    compilerPathLabel_->clear();
    extensionsLabel_->clear();
    statusLabel_->clear();
}

void LanguageDetailsPanel::onOpenHomepage() {
    // Would be connected to actual language data
    QDesktopServices::openUrl(QUrl("https://www.example.com"));
}

void LanguageDetailsPanel::onReinstall() {
    QMessageBox::information(this, "Reinstall", "Reinstall functionality not yet implemented");
}

// ===============================================================================
// LANGUAGE WIDGET IMPLEMENTATION
// ===============================================================================

LanguageWidget::LanguageWidget(QWidget* parent)
    : QWidget(parent), 
      currentLanguage_(LanguageType::Unknown),
      compilationProcess_(nullptr),
      isCompiling_(false) {
    
    setupUI();
    connectSignals();
    populateLanguageTree();
    loadStylesheet();
}

LanguageWidget::~LanguageWidget() {
    if (compilationProcess_) {
        compilationProcess_->kill();
        compilationProcess_->waitForFinished();
    }
}

void LanguageWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Toolbar
    auto toolbarLayout = new QHBoxLayout();
    toolbarLayout->setContentsMargins(8, 8, 8, 8);
    toolbarLayout->setSpacing(8);
    
    toolbarLayout->addWidget(new QLabel("Filter:"));
    
    filterComboBox_ = new QComboBox(this);
    filterComboBox_->addItem("All Languages");
    filterComboBox_->addItem("Systems Programming");
    filterComboBox_->addItem("Scripting");
    filterComboBox_->addItem("JVM Languages");
    filterComboBox_->addItem("Web");
    filterComboBox_->addItem(".NET");
    filterComboBox_->addItem("Enterprise");
    filterComboBox_->setMaximumWidth(200);
    toolbarLayout->addWidget(filterComboBox_);
    
    searchLineEdit_ = new QLineEdit(this);
    searchLineEdit_->setPlaceholderText("Search languages...");
    searchLineEdit_->setMaximumWidth(250);
    toolbarLayout->addWidget(searchLineEdit_);
    
    toolbarLayout->addStretch();
    
    compileButton_ = new QPushButton("Compile", this);
    compileButton_->setMaximumWidth(100);
    compileButton_->setEnabled(false);
    toolbarLayout->addWidget(compileButton_);
    
    installButton_ = new QPushButton("Install", this);
    installButton_->setMaximumWidth(100);
    installButton_->setEnabled(false);
    toolbarLayout->addWidget(installButton_);
    
    refreshButton_ = new QPushButton("Refresh", this);
    refreshButton_->setMaximumWidth(100);
    toolbarLayout->addWidget(refreshButton_);
    
    mainLayout->addLayout(toolbarLayout);
    
    // Main content splitter
    auto splitter = new QSplitter(Qt::Horizontal, this);
    
    // Left side: Language tree
    auto leftWidget = new QWidget(this);
    auto leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    languageTree_ = new QTreeWidget(this);
    languageTree_->setHeaderLabels({"Language", "Category", "Status"});
    languageTree_->setColumnCount(3);
    languageTree_->setRootIsDecorated(true);
    languageTree_->setAnimatedCollapse(true);
    languageTree_->header()->setStretchLastSection(false);
    languageTree_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    languageTree_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    languageTree_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    leftLayout->addWidget(languageTree_);
    
    // Right side: Details panel
    detailsPanel_ = new LanguageDetailsPanel(this);
    
    splitter->addWidget(leftWidget);
    splitter->addWidget(detailsPanel_);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter, 1);
    
    // Status bar
    statusLabel_ = new QLabel("Ready", this);
    statusLabel_->setStyleSheet("padding: 4px 8px; border-top: 1px solid #DDD;");
    mainLayout->addWidget(statusLabel_);
    
    // Output section
    auto outputLayout = new QVBoxLayout();
    outputLayout->setContentsMargins(8, 8, 8, 8);
    outputLayout->setSpacing(8);
    
    auto outputLabel = new QLabel("Output:", this);
    outputLabel->setStyleSheet("font-weight: bold;");
    outputLayout->addWidget(outputLabel);
    
    outputWindow_ = new QTextEdit(this);
    outputWindow_->setReadOnly(true);
    outputWindow_->setMaximumHeight(150);
    outputWindow_->setStyleSheet(
        "QTextEdit { background-color: #1E1E1E; color: #00FF00; font-family: 'Courier New'; }"
    );
    outputLayout->addWidget(outputWindow_);
    
    compilationProgress_ = new QProgressBar(this);
    compilationProgress_->setMaximumHeight(20);
    compilationProgress_->setVisible(false);
    outputLayout->addWidget(compilationProgress_);
    
    auto outputButtonLayout = new QHBoxLayout();
    outputButtonLayout->addStretch();
    
    auto clearOutputButton = new QPushButton("Clear Output", this);
    clearOutputButton->setMaximumWidth(120);
    connect(clearOutputButton, &QPushButton::clicked, this, &LanguageWidget::onOutputCleared);
    outputButtonLayout->addWidget(clearOutputButton);
    
    auto exportOutputButton = new QPushButton("Export Output", this);
    exportOutputButton->setMaximumWidth(120);
    connect(exportOutputButton, &QPushButton::clicked, this, &LanguageWidget::onOutputExported);
    outputButtonLayout->addWidget(exportOutputButton);
    
    outputLayout->addLayout(outputButtonLayout);
    
    mainLayout->addLayout(outputLayout);
}

void LanguageWidget::connectSignals() {
    connect(languageTree_, &QTreeWidget::itemSelectionChanged, 
            this, &LanguageWidget::onLanguageSelectionChanged);
    
    connect(compileButton_, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Select Source File", "", "Source Files (*.*)");
        if (!fileName.isEmpty()) {
            startCompilation(fileName);
        }
    });
    
    connect(installButton_, &QPushButton::clicked, this, &LanguageWidget::onInstallButtonClicked);
    
    connect(refreshButton_, &QPushButton::clicked, this, &LanguageWidget::onRefreshButtonClicked);
    
    connect(searchLineEdit_, &QLineEdit::textChanged, this, &LanguageWidget::onSearchTextChanged);
    
    connect(filterComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this]() { populateLanguageTree(); });
}

void LanguageWidget::populateLanguageTree() {
    languageTree_->clear();
    categoryItems_.clear();
    
    auto languages = LanguageFactory::getAllLanguages();
    
    // Group by category
    QMap<QString, QVector<LanguageConfig>> categorized;
    for (const auto& lang : languages) {
        categorized[lang.category].push_back(lang);
    }
    
    // Create tree
    for (auto it = categorized.begin(); it != categorized.end(); ++it) {
        auto categoryItem = new LanguageCategoryItem(it.key());
        languageTree_->addTopLevelItem(categoryItem);
        categoryItems_[it.key()] = categoryItem;
        
        for (const auto& lang : it.value()) {
            new LanguageTreeItem(categoryItem, lang);
        }
    }
    
    // Expand first level
    for (auto item : categoryItems_.values()) {
        languageTree_->expandItem(item);
    }
}

void LanguageWidget::selectLanguage(LanguageType type) {
    // Find and select the language item
    for (int i = 0; i < languageTree_->topLevelItemCount(); ++i) {
        auto categoryItem = languageTree_->topLevelItem(i);
        for (int j = 0; j < categoryItem->childCount(); ++j) {
            auto langItem = dynamic_cast<LanguageTreeItem*>(categoryItem->child(j));
            if (langItem && langItem->languageType() == type) {
                languageTree_->setCurrentItem(langItem);
                currentLanguage_ = type;
                detailsPanel_->displayLanguage(langItem->config());
                emit languageSelected(type);
                return;
            }
        }
    }
}

void LanguageWidget::refreshLanguageList() {
    populateLanguageTree();
    statusLabel_->setText("Language list refreshed");
}

void LanguageWidget::displayCompilationStatus(const QString& status, bool success) {
    statusLabel_->setText(status);
    statusLabel_->setStyleSheet(QString("padding: 4px 8px; border-top: 1px solid #DDD; color: %1;")
        .arg(success ? "green" : "red"));
}

void LanguageWidget::appendOutput(const QString& text) {
    outputWindow_->append(text);
}

void LanguageWidget::clearOutput() {
    outputWindow_->clear();
}

void LanguageWidget::setCompilationInProgress(bool inProgress) {
    isCompiling_ = inProgress;
    compileButton_->setEnabled(!inProgress);
    compilationProgress_->setVisible(inProgress);
    if (inProgress) {
        compilationProgress_->setValue(0);
    }
}

void LanguageWidget::updateCompilationProgress(int percentage) {
    compilationProgress_->setValue(percentage);
}

void LanguageWidget::onLanguageSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous) {
    Q_UNUSED(previous);
    
    auto langItem = dynamic_cast<LanguageTreeItem*>(current);
    if (langItem) {
        currentLanguage_ = langItem->languageType();
        detailsPanel_->displayLanguage(langItem->config());
        compileButton_->setEnabled(true);
        installButton_->setEnabled(true);
        emit languageSelected(currentLanguage_);
    } else {
        detailsPanel_->clearDisplay();
        compileButton_->setEnabled(false);
        installButton_->setEnabled(false);
    }
}

void LanguageWidget::onCategoryExpanded(QTreeWidgetItem* item) {
    Q_UNUSED(item);
}

void LanguageWidget::onCategoryCollapsed(QTreeWidgetItem* item) {
    Q_UNUSED(item);
}

void LanguageWidget::onCompileButtonClicked() {
    // Handled in connectSignals with lambda
}

void LanguageWidget::onInstallButtonClicked() {
    emit installRequested(currentLanguage_);
    QMessageBox::information(this, "Install", 
        "Install functionality not yet implemented for this language");
}

void LanguageWidget::onRefreshButtonClicked() {
    refreshLanguageList();
}

void LanguageWidget::onSearchTextChanged(const QString& text) {
    filterLanguages(text);
}

void LanguageWidget::filterLanguages(const QString& filter) {
    for (auto categoryItem : categoryItems_.values()) {
        bool categoryHasMatch = false;
        
        for (int i = 0; i < categoryItem->childCount(); ++i) {
            auto langItem = dynamic_cast<LanguageTreeItem*>(categoryItem->child(i));
            if (langItem) {
                bool matches = langItem->config().displayName.contains(filter, Qt::CaseInsensitive) ||
                              langItem->config().description.contains(filter, Qt::CaseInsensitive);
                langItem->setHidden(!matches);
                if (matches) categoryHasMatch = true;
            }
        }
        
        categoryItem->setHidden(!categoryHasMatch);
    }
}

void LanguageWidget::onOutputCleared() {
    clearOutput();
    statusLabel_->setText("Output cleared");
}

void LanguageWidget::onOutputExported() {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Export Output", "", "Text Files (*.txt)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            file.write(outputWindow_->toPlainText().toUtf8());
            file.close();
            statusLabel_->setText("Output exported to: " + fileName);
        }
    }
}

void LanguageWidget::onCompilationProgressUpdated(int percentage) {
    updateCompilationProgress(percentage);
}

void LanguageWidget::startCompilation(const QString& sourceFile) {
    if (isCompiling_) {
        QMessageBox::warning(this, "Compilation", "Compilation already in progress");
        return;
    }
    
    setCompilationInProgress(true);
    clearOutput();
    appendOutput("Starting compilation: " + sourceFile);
    
    auto result = LanguageManager::instance().compileFile(sourceFile);
    
    if (result.success) {
        displayCompilationStatus("Compilation successful", true);
        appendOutput("Compilation completed successfully");
        appendOutput("Output: " + result.outputFile);
        appendOutput(QString("Time: %1ms").arg(result.compilationTime));
        updateCompilationProgress(100);
    } else {
        displayCompilationStatus("Compilation failed", false);
        appendOutput("Error: " + result.errorMessage);
        updateCompilationProgress(0);
    }
    
    setCompilationInProgress(false);
}

void LanguageWidget::finishCompilation(bool success, const QString& message) {
    displayCompilationStatus(message, success);
    appendOutput(message);
    setCompilationInProgress(false);
}

void LanguageWidget::loadStylesheet() {
    // Default dark theme
    setStyleSheet(
        "QWidget { background-color: #F5F5F5; }"
        "QTreeWidget { background-color: white; border: 1px solid #DDD; }"
        "QTextEdit { background-color: #1E1E1E; color: #00FF00; }"
        "QPushButton { background-color: #0066CC; color: white; border-radius: 3px; padding: 6px 12px; }"
        "QPushButton:hover { background-color: #0052A3; }"
        "QPushButton:pressed { background-color: #003D7A; }"
        "QComboBox { background-color: white; border: 1px solid #DDD; padding: 4px; }"
        "QLineEdit { background-color: white; border: 1px solid #DDD; padding: 4px; }"
    );
}

void LanguageWidget::updateTheme(const QString& theme) {
    Q_UNUSED(theme);
    // Theme update logic would go here
}

} // namespace Languages
} // namespace RawrXD
