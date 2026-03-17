// ===============================================================================
// Language Framework Integration Implementation
// ===============================================================================

#include "language_integration.hpp"
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QAction>
#include <QActionGroup>
#include <QFileInfo>
#include <QDebug>

namespace RawrXD {
namespace Languages {

// Static member initialization
LanguageWidget* LanguageIntegration::languageWidget_ = nullptr;
QDockWidget* LanguageIntegration::languageDockWidget_ = nullptr;
QMenu* LanguageIntegration::languageMenu_ = nullptr;
QMainWindow* LanguageIntegration::mainWindow_ = nullptr;

void LanguageIntegration::initialize(QMainWindow* mainWindow) {
    mainWindow_ = mainWindow;
    
    // Initialize language manager
    LanguageManager::instance().initialize();
    
    // Create language widget
    languageWidget_ = createLanguageWidget(mainWindow);
    
    // Create dock widget
    languageDockWidget_ = new QDockWidget("Languages", mainWindow);
    languageDockWidget_->setWidget(languageWidget_);
    languageDockWidget_->setObjectName("LanguageDockWidget");
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, languageDockWidget_);
    
    // Create menu
    createLanguageMenu(mainWindow);
    
    // Connect signals
    if (languageWidget_) {
        QObject::connect(languageWidget_, &LanguageWidget::languageSelected,
            [](LanguageType type) {
                qDebug() << "Language selected:" << LanguageUtils::languageTypeToString(type);
            });
        
        QObject::connect(languageWidget_, &LanguageWidget::compileRequested,
            [](const QString& sourceFile) {
                onCompileFile(sourceFile);
            });
        
        QObject::connect(languageWidget_, &LanguageWidget::outputUpdated,
            [](const QString& text) {
                qDebug() << "Compilation output:" << text;
            });
    }
}

LanguageWidget* LanguageIntegration::createLanguageWidget(QMainWindow* parent) {
    auto widget = new LanguageWidget(parent);
    return widget;
}

void LanguageIntegration::createLanguageMenu(QMainWindow* mainWindow) {
    if (!mainWindow->menuBar()) return;
    
    // Create Languages menu
    languageMenu_ = mainWindow->menuBar()->addMenu("&Languages");
    
    // Get all available languages
    auto languages = LanguageFactory::getAllLanguages();
    
    // Group by category
    QMap<QString, QVector<LanguageConfig>> categorized;
    for (const auto& lang : languages) {
        categorized[lang.category].push_back(lang);
    }
    
    // Create submenu for each category
    for (auto it = categorized.begin(); it != categorized.end(); ++it) {
        QMenu* categoryMenu = languageMenu_->addMenu(it.key());
        
        for (const auto& lang : it.value()) {
            QAction* action = categoryMenu->addAction(lang.displayName);
            action->setToolTip(lang.description);
            action->setEnabled(lang.isSupported);
            
            QObject::connect(action, &QAction::triggered, [lang]() {
                if (languageWidget_) {
                    languageWidget_->selectLanguage(lang.type);
                }
            });
        }
    }
    
    languageMenu_->addSeparator();
    
    // Add "Manage Languages" action
    QAction* manageAction = languageMenu_->addAction("&Manage Languages...");
    QObject::connect(manageAction, &QAction::triggered, [mainWindow]() {
        if (languageDockWidget_) {
            languageDockWidget_->show();
            languageDockWidget_->raise();
        }
    });
    
    // Add "Detect Language" action
    QAction* detectAction = languageMenu_->addAction("&Detect Language from File");
    QObject::connect(detectAction, &QAction::triggered, []() {
        // Would be connected to current editor file
        qDebug() << "Detect language action triggered";
    });
    
    languageMenu_->addSeparator();
    
    // Add "Compile" action
    QAction* compileAction = languageMenu_->addAction("&Compile Current File");
    compileAction->setShortcut(Qt::CTRL + Qt::Key_B);
    QObject::connect(compileAction, &QAction::triggered, []() {
        if (languageWidget_) {
            qDebug() << "Compile action triggered";
        }
    });
    
    // Add "Run" action
    QAction* runAction = languageMenu_->addAction("&Run Current File");
    runAction->setShortcut(Qt::CTRL + Qt::Key_R);
    QObject::connect(runAction, &QAction::triggered, []() {
        qDebug() << "Run action triggered";
    });
    
    languageMenu_->addSeparator();
    
    // Add "Language Settings" action
    QAction* settingsAction = languageMenu_->addAction("&Language Settings...");
    QObject::connect(settingsAction, &QAction::triggered, []() {
        qDebug() << "Language settings action triggered";
    });
}

void LanguageIntegration::detectAndSelectLanguage(const QString& filePath) {
    if (!languageWidget_) return;
    
    LanguageType type = LanguageFactory::detectLanguageFromFile(filePath);
    if (type != LanguageType::Unknown) {
        languageWidget_->selectLanguage(type);
    }
}

void LanguageIntegration::onFileOpened(const QString& filePath) {
    detectAndSelectLanguage(filePath);
}

void LanguageIntegration::onCompileFile(const QString& filePath) {
    if (!languageWidget_) return;
    
    languageWidget_->setCompilationInProgress(true);
    languageWidget_->clearOutput();
    
    auto result = LanguageManager::instance().compileFile(filePath);
    
    if (result.success) {
        languageWidget_->displayCompilationStatus("Compilation successful", true);
        languageWidget_->appendOutput("Compilation completed successfully");
        languageWidget_->appendOutput("Output: " + result.outputFile);
        languageWidget_->appendOutput(QString("Time: %1ms").arg(result.compilationTime));
    } else {
        languageWidget_->displayCompilationStatus("Compilation failed", false);
        languageWidget_->appendOutput("Error: " + result.errorMessage);
    }
    
    languageWidget_->setCompilationInProgress(false);
}

void LanguageIntegration::setWidgetVisible(bool visible) {
    if (languageDockWidget_) {
        if (visible) {
            languageDockWidget_->show();
            languageDockWidget_->raise();
        } else {
            languageDockWidget_->hide();
        }
    }
}

void LanguageIntegration::updateIDEOutput(bool success, const QString& output) {
    if (!languageWidget_) return;
    
    languageWidget_->displayCompilationStatus(output, success);
    languageWidget_->appendOutput(output);
}

} // namespace Languages
} // namespace RawrXD
