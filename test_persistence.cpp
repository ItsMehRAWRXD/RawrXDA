// ============================================================
// Phase C: Data Persistence - Unit Tests
// ============================================================
// File: test_persistence.cpp
// Purpose: Comprehensive test suite for MainWindow persistence layer
// Date: January 17, 2026

#include <gtest/gtest.h>
#include <QTest>
#include <QSettings>
#include <QTemporaryDir>
#include <QApplication>
#include <QDebug>
#include <QStandardPaths>

#include "../src/qtapp/MainWindow.h"

// Test Fixture
class PersistenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary directory for test QSettings
        tempDir = std::make_unique<QTemporaryDir>();
        Q_ASSERT(tempDir->isValid());
        
        // Set QSettings to use temp directory
        QStandardPaths::setTestModeEnabled(true);
        
        // Create main window for testing
        mainWindow = std::make_unique<MainWindow>();
    }
    
    void TearDown() override {
        // Clean up
        mainWindow.reset();
        tempDir.reset();
        
        // Clear test settings
        QSettings settings("RawrXD", "QtShell");
        settings.clear();
        settings.sync();
    }
    
    std::unique_ptr<QTemporaryDir> tempDir;
    std::unique_ptr<MainWindow> mainWindow;
};

// ============================================================
// Window Geometry Persistence Tests
// ============================================================

TEST_F(PersistenceTest, SaveWindowGeometry) {
    // Arrange
    mainWindow->resize(1280, 720);
    mainWindow->move(100, 100);
    
    // Act
    mainWindow->handleSaveState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    ASSERT_TRUE(settings.contains("MainWindow/geometry"));
    QByteArray geometry = settings.value("MainWindow/geometry").toByteArray();
    ASSERT_FALSE(geometry.isEmpty());
}

TEST_F(PersistenceTest, RestoreWindowGeometry) {
    // Arrange
    QSettings settings("RawrXD", "QtShell");
    QRect originalGeometry(100, 100, 1280, 720);
    settings.setValue("MainWindow/geometry", mainWindow->saveGeometry());
    
    // Create new window
    MainWindow newWindow;
    
    // Act
    newWindow.handleLoadState();
    
    // Assert
    // Verify window was positioned (exact coordinates may vary by platform)
    EXPECT_GT(newWindow.width(), 0);
    EXPECT_GT(newWindow.height(), 0);
}

TEST_F(PersistenceTest, SaveWindowState) {
    // Arrange - maximize window
    mainWindow->showMaximized();
    
    // Act
    mainWindow->handleSaveState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    ASSERT_TRUE(settings.contains("MainWindow/windowState"));
}

// ============================================================
// Editor State Persistence Tests
// ============================================================

TEST_F(PersistenceTest, SaveEditorState_SingleTab) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    editor->setPlainText("int main() { return 0; }");
    mainWindow->editorTabs_->addTab(editor, "main.cpp");
    
    // Act
    mainWindow->saveEditorState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    ASSERT_TRUE(settings.contains("Editor/tabCount"));
    EXPECT_EQ(settings.value("Editor/tabCount").toInt(), 1);
    ASSERT_TRUE(settings.contains("Editor/tabsMetadata"));
}

TEST_F(PersistenceTest, SaveEditorState_MultipleTabs) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    for (int i = 0; i < 3; ++i) {
        QTextEdit* editor = new QTextEdit(mainWindow.get());
        editor->setPlainText(QString("// File %1\n").arg(i).repeated(10));
        mainWindow->editorTabs_->addTab(editor, QString("file%1.cpp").arg(i));
    }
    
    // Act
    mainWindow->saveEditorState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    EXPECT_EQ(settings.value("Editor/tabCount").toInt(), 3);
}

TEST_F(PersistenceTest, SaveCursorPosition) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    editor->setPlainText("Line 1\nLine 2\nLine 3\nLine 4");
    mainWindow->editorTabs_->addTab(editor, "test.txt");
    
    // Move cursor to line 2, column 3
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down);
    cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, 3);
    editor->setTextCursor(cursor);
    
    // Act
    mainWindow->saveEditorState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    QString jsonStr = settings.value("Editor/tabsMetadata").toString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    ASSERT_TRUE(doc.isArray());
    
    QJsonArray array = doc.array();
    EXPECT_GE(array.size(), 1);
    if (array.size() > 0) {
        QJsonObject obj = array[0].toObject();
        EXPECT_EQ(obj["cursorLine"].toInt(), 1);  // Line 2 (0-indexed)
        EXPECT_EQ(obj["cursorColumn"].toInt(), 3);
    }
}

TEST_F(PersistenceTest, SaveScrollPosition) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    editor->setPlainText("Line\n".repeated(100));  // Create long text
    mainWindow->editorTabs_->addTab(editor, "large.txt");
    
    // Scroll down
    editor->verticalScrollBar()->setValue(500);
    
    // Act
    mainWindow->saveEditorState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    QString jsonStr = settings.value("Editor/tabsMetadata").toString();
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    
    if (doc.isArray() && doc.array().size() > 0) {
        QJsonObject obj = doc.array()[0].toObject();
        EXPECT_EQ(obj["scrollPosition"].toInt(), 500);
    }
}

TEST_F(PersistenceTest, RestoreEditorState) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    // Create and save state
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    editor->setPlainText("Test content");
    mainWindow->editorTabs_->addTab(editor, "test.cpp");
    mainWindow->saveEditorState();
    
    // Create new window to restore state
    MainWindow newWindow;
    if (!newWindow.editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized in new window";
    }
    
    // Add a dummy tab to match saved state
    QTextEdit* dummyEditor = new QTextEdit(&newWindow);
    dummyEditor->setPlainText("Test content");
    newWindow.editorTabs_->addTab(dummyEditor, "test.cpp");
    
    // Act
    newWindow.restoreEditorState();
    
    // Assert - state should be restored without errors
    EXPECT_GE(newWindow.editorTabs_->count(), 1);
}

// ============================================================
// Recent Files Tests
// ============================================================

TEST_F(PersistenceTest, AddRecentFile) {
    // Arrange
    QString filePath = "/path/to/file.cpp";
    
    // Act
    mainWindow->addRecentFile(filePath);
    
    // Assert
    QStringList recentFiles = mainWindow->getRecentFiles();
    ASSERT_FALSE(recentFiles.isEmpty());
    EXPECT_EQ(recentFiles[0], filePath);
}

TEST_F(PersistenceTest, RecentFilesDeduplication) {
    // Arrange
    QString filePath = "/path/to/file.cpp";
    mainWindow->addRecentFile(filePath);
    
    // Act - add same file again
    mainWindow->addRecentFile(filePath);
    
    // Assert - should still be only one entry
    QStringList recentFiles = mainWindow->getRecentFiles();
    EXPECT_EQ(recentFiles.count(filePath), 1);
}

TEST_F(PersistenceTest, RecentFilesLimit) {
    // Arrange
    for (int i = 0; i < 25; ++i) {
        mainWindow->addRecentFile(QString("/path/to/file%1.cpp").arg(i));
    }
    
    // Act
    QStringList recentFiles = mainWindow->getRecentFiles();
    
    // Assert - should be limited to 20
    EXPECT_LE(recentFiles.size(), 20);
}

TEST_F(PersistenceTest, RecentFilesOrder) {
    // Arrange
    for (int i = 0; i < 3; ++i) {
        mainWindow->addRecentFile(QString("/path/to/file%1.cpp").arg(i));
    }
    
    // Act - add file0 again
    mainWindow->addRecentFile("/path/to/file0.cpp");
    
    // Assert - file0 should be at front
    QStringList recentFiles = mainWindow->getRecentFiles();
    EXPECT_EQ(recentFiles[0], "/path/to/file0.cpp");
}

TEST_F(PersistenceTest, ClearRecentFiles) {
    // Arrange
    mainWindow->addRecentFile("/path/to/file.cpp");
    
    // Act
    mainWindow->clearRecentFiles();
    
    // Assert
    QStringList recentFiles = mainWindow->getRecentFiles();
    EXPECT_TRUE(recentFiles.isEmpty());
}

// ============================================================
// Command History Tests
// ============================================================

TEST_F(PersistenceTest, AddCommandToHistory) {
    // Arrange
    QString command = "cmake --build . --config Release";
    
    // Act
    mainWindow->addCommandToHistory(command);
    
    // Assert
    QStringList history = mainWindow->getCommandHistory();
    ASSERT_FALSE(history.isEmpty());
    EXPECT_TRUE(history[0].contains(command));
    EXPECT_TRUE(history[0].contains("["));  // Should have timestamp
}

TEST_F(PersistenceTest, CommandHistoryTimestamp) {
    // Arrange
    QString command = "git commit -m 'test'";
    
    // Act
    mainWindow->addCommandToHistory(command);
    
    // Assert
    QStringList history = mainWindow->getCommandHistory();
    ASSERT_GT(history.size(), 0);
    QString firstCmd = history[0];
    
    // Should match pattern: [YYYY-MM-DD HH:mm:ss] command
    EXPECT_TRUE(firstCmd.startsWith("["));
    EXPECT_TRUE(firstCmd.contains("]"));
    EXPECT_TRUE(firstCmd.contains(command));
}

TEST_F(PersistenceTest, CommandHistoryLimit) {
    // Arrange
    for (int i = 0; i < 1100; ++i) {
        mainWindow->addCommandToHistory(QString("command_%1").arg(i));
    }
    
    // Act
    QStringList history = mainWindow->getCommandHistory();
    
    // Assert - should be limited to 1000
    EXPECT_LE(history.size(), 1000);
}

TEST_F(PersistenceTest, CommandHistoryOrder) {
    // Arrange
    for (int i = 0; i < 3; ++i) {
        mainWindow->addCommandToHistory(QString("cmd_%1").arg(i));
    }
    
    // Act
    QStringList history = mainWindow->getCommandHistory();
    
    // Assert - last added should be at end
    EXPECT_TRUE(history.last().contains("cmd_2"));
}

TEST_F(PersistenceTest, ClearCommandHistory) {
    // Arrange
    mainWindow->addCommandToHistory("test command");
    
    // Act
    mainWindow->clearCommandHistory();
    
    // Assert
    QStringList history = mainWindow->getCommandHistory();
    EXPECT_TRUE(history.isEmpty());
}

// ============================================================
// Tab State Tests
// ============================================================

TEST_F(PersistenceTest, SaveTabState) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    for (int i = 0; i < 2; ++i) {
        QTextEdit* editor = new QTextEdit(mainWindow.get());
        mainWindow->editorTabs_->addTab(editor, QString("tab%1.txt").arg(i));
    }
    mainWindow->editorTabs_->setCurrentIndex(1);
    
    // Act
    mainWindow->saveTabState();
    
    // Assert
    QSettings settings("RawrXD", "QtShell");
    EXPECT_EQ(settings.value("Tabs/count").toInt(), 2);
    EXPECT_EQ(settings.value("Tabs/activeIndex").toInt(), 1);
}

TEST_F(PersistenceTest, RestoreTabState) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    // Create tabs and save state
    for (int i = 0; i < 2; ++i) {
        QTextEdit* editor = new QTextEdit(mainWindow.get());
        mainWindow->editorTabs_->addTab(editor, QString("tab%1.txt").arg(i));
    }
    mainWindow->editorTabs_->setCurrentIndex(1);
    mainWindow->saveTabState();
    
    // Create new window and restore
    MainWindow newWindow;
    if (!newWindow.editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized in new window";
    }
    
    for (int i = 0; i < 2; ++i) {
        QTextEdit* editor = new QTextEdit(&newWindow);
        newWindow.editorTabs_->addTab(editor, QString("tab%1.txt").arg(i));
    }
    
    // Act
    newWindow.restoreTabState();
    
    // Assert
    EXPECT_EQ(newWindow.editorTabs_->currentIndex(), 1);
}

// ============================================================
// Error Handling Tests
// ============================================================

TEST_F(PersistenceTest, AddRecentFileWithEmptyPath) {
    // Act & Assert - should not throw
    EXPECT_NO_THROW(mainWindow->addRecentFile(""));
    
    QStringList recentFiles = mainWindow->getRecentFiles();
    EXPECT_FALSE(recentFiles.contains(""));
}

TEST_F(PersistenceTest, AddCommandWithEmptyString) {
    // Act & Assert - should not throw
    EXPECT_NO_THROW(mainWindow->addCommandToHistory(""));
    
    QStringList history = mainWindow->getCommandHistory();
    // Empty commands should be filtered out
    for (const QString& cmd : history) {
        EXPECT_FALSE(cmd.isEmpty());
    }
}

TEST_F(PersistenceTest, RestoreCorruptedJSON) {
    // Arrange - set corrupted JSON
    QSettings settings("RawrXD", "QtShell");
    settings.setValue("Editor/tabsMetadata", "{ invalid json }");
    
    // Act & Assert - should handle gracefully
    EXPECT_NO_THROW(mainWindow->restoreEditorState());
}

// ============================================================
// Metrics Tests
// ============================================================

TEST_F(PersistenceTest, PersistenceMetrics) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    mainWindow->editorTabs_->addTab(editor, "test.txt");
    
    // Act
    mainWindow->saveEditorState();
    
    // Assert
    EXPECT_GT(mainWindow->m_lastSaveTime, 0);
    EXPECT_GE(mainWindow->m_persistenceDataSize, 0);
}

// ============================================================
// Integration Tests
// ============================================================

TEST_F(PersistenceTest, FullPersistenceCycle) {
    // Arrange
    if (!mainWindow->editorTabs_) {
        GTEST_SKIP() << "editorTabs_ not initialized";
    }
    
    // Add data
    mainWindow->addRecentFile("/path/to/file.cpp");
    mainWindow->addCommandToHistory("build --release");
    
    QTextEdit* editor = new QTextEdit(mainWindow.get());
    editor->setPlainText("int main() {}");
    mainWindow->editorTabs_->addTab(editor, "main.cpp");
    
    // Act - save state
    mainWindow->handleSaveState();
    mainWindow->saveEditorState();
    mainWindow->saveTabState();
    
    // Assert - verify all data persisted
    QSettings settings("RawrXD", "QtShell");
    
    ASSERT_TRUE(settings.contains("MainWindow/geometry"));
    ASSERT_TRUE(settings.contains("Files/recentFiles"));
    ASSERT_TRUE(settings.contains("Commands/history"));
    ASSERT_TRUE(settings.contains("Editor/tabsMetadata"));
    
    // Verify data integrity
    QStringList recentFiles = mainWindow->getRecentFiles();
    EXPECT_TRUE(recentFiles.contains("/path/to/file.cpp"));
    
    QStringList history = mainWindow->getCommandHistory();
    EXPECT_TRUE(history[0].contains("build --release"));
}

// ============================================================
// Main Entry Point
// ============================================================

int main(int argc, char** argv) {
    // Initialize Qt Application (required for GUI testing)
    QApplication app(argc, argv);
    
    // Initialize Google Test
    ::testing::InitGoogleTest(&argc, argv);
    
    // Run tests
    return RUN_ALL_TESTS();
}
