/*
 * MainWindow_Widget_Integration.cpp
 * 
 * Production widget integration for MainWindow dock widgets.
 * This file should be included in MainWindow.cpp during setupDockWidgets()
 * 
 * Provides production-ready implementations for all subsystem widgets
 */

#pragma once

// Include production widget definitions
#include "Subsystems_Production.h"
#include <QDockWidget>
#include <QSettings>
#include <QString>

/**
 * @class WidgetFactory
 * @brief Factory for creating and managing production widgets
 */
class WidgetFactory {
public:
    static WidgetFactory& instance() {
        static WidgetFactory factory;
        return factory;
    }
    
    // Create debug widgets
    RunDebugWidget* createRunDebugWidget(QWidget* parent = nullptr) {
        auto widget = new RunDebugWidget(parent);
        QSettings settings("RawrXD", "IDE");
        // Restore state if available
        return widget;
    }
    
    ProfilerWidget* createProfilerWidget(QWidget* parent = nullptr) {
        return new ProfilerWidget(parent);
    }
    
    TestExplorerWidget* createTestExplorerWidget(QWidget* parent = nullptr) {
        return new TestExplorerWidget(parent);
    }
    
    // Create development tools
    DatabaseToolWidget* createDatabaseToolWidget(QWidget* parent = nullptr) {
        return new DatabaseToolWidget(parent);
    }
    
    DockerToolWidget* createDockerToolWidget(QWidget* parent = nullptr) {
        return new DockerToolWidget(parent);
    }
    
    CloudExplorerWidget* createCloudExplorerWidget(QWidget* parent = nullptr) {
        return new CloudExplorerWidget(parent);
    }
    
    PackageManagerWidget* createPackageManagerWidget(QWidget* parent = nullptr) {
        return new PackageManagerWidget(parent);
    }
    
    // Create documentation & design widgets
    DocumentationWidget* createDocumentationWidget(QWidget* parent = nullptr) {
        return new DocumentationWidget(parent);
    }
    
    UMLViewWidget* createUMLViewWidget(QWidget* parent = nullptr) {
        return new UMLViewWidget(parent);
    }
    
    ImageToolWidget* createImageToolWidget(QWidget* parent = nullptr) {
        return new ImageToolWidget(parent);
    }
    
    DesignToCodeWidget* createDesignToCodeWidget(QWidget* parent = nullptr) {
        return new DesignToCodeWidget(parent);
    }
    
    ColorPickerWidget* createColorPickerWidget(QWidget* parent = nullptr) {
        return new ColorPickerWidget(parent);
    }
    
    // Create collaboration widgets
    AudioCallWidget* createAudioCallWidget(QWidget* parent = nullptr) {
        return new AudioCallWidget(parent);
    }
    
    ScreenShareWidget* createScreenShareWidget(QWidget* parent = nullptr) {
        return new ScreenShareWidget(parent);
    }
    
    WhiteboardWidget* createWhiteboardWidget(QWidget* parent = nullptr) {
        return new WhiteboardWidget(parent);
    }
    
    // Create productivity widgets
    TimeTrackerWidget* createTimeTrackerWidget(QWidget* parent = nullptr) {
        return new TimeTrackerWidget(parent);
    }
    
    PomodoroWidget* createPomodoroWidget(QWidget* parent = nullptr) {
        return new PomodoroWidget(parent);
    }
    
    // Create code intelligence widgets
    CodeMinimap* createCodeMinimap(QWidget* parent = nullptr) {
        return new CodeMinimap(parent);
    }
    
    BreadcrumbBar* createBreadcrumbBar(QWidget* parent = nullptr) {
        return new BreadcrumbBar(parent);
    }
    
    SearchResultWidget* createSearchResultWidget(QWidget* parent = nullptr) {
        return new SearchResultWidget(parent);
    }
    
    BookmarkWidget* createBookmarkWidget(QWidget* parent = nullptr) {
        return new BookmarkWidget(parent);
    }
    
    TodoWidget* createTodoWidget(QWidget* parent = nullptr) {
        return new TodoWidget(parent);
    }
    
private:
    WidgetFactory() = default;
    WidgetFactory(const WidgetFactory&) = delete;
    WidgetFactory& operator=(const WidgetFactory&) = delete;
};

/**
 * @brief Initializes all production widgets in MainWindow dock areas
 * 
 * This function should be called from MainWindow::setupDockWidgets()
 * 
 * @param mainWindow The MainWindow instance
 * @param restoreState Whether to restore previous widget states
 */
inline void initializeProductionWidgets(MainWindow* mainWindow, bool restoreState = true) {
    if (!mainWindow) return;
    
    WidgetFactory& factory = WidgetFactory::instance();
    QSettings settings("RawrXD", "IDE");
    
    // Bottom right area - Debug widgets
    QDockWidget* debugDock = new QDockWidget("Run & Debug", mainWindow);
    debugDock->setWidget(factory.createRunDebugWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, debugDock);
    
    QDockWidget* profilerDock = new QDockWidget("Profiler", mainWindow);
    profilerDock->setWidget(factory.createProfilerWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, profilerDock);
    profilerDock->hide();
    
    QDockWidget* testDock = new QDockWidget("Test Explorer", mainWindow);
    testDock->setWidget(factory.createTestExplorerWidget());
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, testDock);
    testDock->hide();
    
    // Right area - Development tools
    QDockWidget* databaseDock = new QDockWidget("Database", mainWindow);
    databaseDock->setWidget(factory.createDatabaseToolWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, databaseDock);
    databaseDock->hide();
    
    QDockWidget* dockerDock = new QDockWidget("Docker", mainWindow);
    dockerDock->setWidget(factory.createDockerToolWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, dockerDock);
    dockerDock->hide();
    
    QDockWidget* cloudDock = new QDockWidget("Cloud Explorer", mainWindow);
    cloudDock->setWidget(factory.createCloudExplorerWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, cloudDock);
    cloudDock->hide();
    
    QDockWidget* packageDock = new QDockWidget("Package Manager", mainWindow);
    packageDock->setWidget(factory.createPackageManagerWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, packageDock);
    packageDock->hide();
    
    // Documentation & Design widgets
    QDockWidget* docDock = new QDockWidget("Documentation", mainWindow);
    docDock->setWidget(factory.createDocumentationWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, docDock);
    docDock->hide();
    
    QDockWidget* umlDock = new QDockWidget("UML View", mainWindow);
    umlDock->setWidget(factory.createUMLViewWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, umlDock);
    umlDock->hide();
    
    QDockWidget* imageDock = new QDockWidget("Image Tool", mainWindow);
    imageDock->setWidget(factory.createImageToolWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, imageDock);
    imageDock->hide();
    
    QDockWidget* designDock = new QDockWidget("Design to Code", mainWindow);
    designDock->setWidget(factory.createDesignToCodeWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, designDock);
    designDock->hide();
    
    QDockWidget* colorDock = new QDockWidget("Color Picker", mainWindow);
    colorDock->setWidget(factory.createColorPickerWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, colorDock);
    colorDock->hide();
    
    // Collaboration widgets
    QDockWidget* audioDock = new QDockWidget("Audio Call", mainWindow);
    audioDock->setWidget(factory.createAudioCallWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, audioDock);
    audioDock->hide();
    
    QDockWidget* screenDock = new QDockWidget("Screen Share", mainWindow);
    screenDock->setWidget(factory.createScreenShareWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, screenDock);
    screenDock->hide();
    
    QDockWidget* whiteboardDock = new QDockWidget("Whiteboard", mainWindow);
    whiteboardDock->setWidget(factory.createWhiteboardWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, whiteboardDock);
    whiteboardDock->hide();
    
    // Productivity widgets
    QDockWidget* timerDock = new QDockWidget("Time Tracker", mainWindow);
    timerDock->setWidget(factory.createTimeTrackerWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, timerDock);
    timerDock->hide();
    
    QDockWidget* pomodoroDock = new QDockWidget("Pomodoro", mainWindow);
    pomodoroDock->setWidget(factory.createPomodoroWidget());
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, pomodoroDock);
    pomodoroDock->hide();
    
    // Code intelligence widgets (sidebar)
    QDockWidget* minimapDock = new QDockWidget("Minimap", mainWindow);
    minimapDock->setWidget(factory.createCodeMinimap());
    minimapDock->setMaximumWidth(100);
    mainWindow->addDockWidget(Qt::RightDockWidgetArea, minimapDock);
    minimapDock->hide();
    
    QDockWidget* breadcrumbDock = new QDockWidget("Breadcrumb", mainWindow);
    breadcrumbDock->setWidget(factory.createBreadcrumbBar());
    mainWindow->addDockWidget(Qt::TopDockWidgetArea, breadcrumbDock);
    breadcrumbDock->hide();
    
    QDockWidget* searchDock = new QDockWidget("Search Results", mainWindow);
    searchDock->setWidget(factory.createSearchResultWidget());
    mainWindow->addDockWidget(Qt::BottomDockWidgetArea, searchDock);
    searchDock->hide();
    
    QDockWidget* bookmarkDock = new QDockWidget("Bookmarks", mainWindow);
    bookmarkDock->setWidget(factory.createBookmarkWidget());
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, bookmarkDock);
    bookmarkDock->hide();
    
    QDockWidget* todoDock = new QDockWidget("TODO", mainWindow);
    todoDock->setWidget(factory.createTodoWidget());
    mainWindow->addDockWidget(Qt::LeftDockWidgetArea, todoDock);
    todoDock->hide();
    
    // Restore layout if requested
    if (restoreState) {
        QByteArray geometry = settings.value("geometry/mainwindow", QByteArray()).toByteArray();
        QByteArray windowState = settings.value("geometry/windowstate", QByteArray()).toByteArray();
        
        if (!geometry.isEmpty()) {
            mainWindow->restoreGeometry(geometry);
        }
        if (!windowState.isEmpty()) {
            mainWindow->restoreState(windowState);
        }
    }
}

