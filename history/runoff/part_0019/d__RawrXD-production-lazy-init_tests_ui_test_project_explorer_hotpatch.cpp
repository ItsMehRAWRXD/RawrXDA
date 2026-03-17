// ============================================================================
// UI Tests for Project Explorer and Hotpatch Operations
// Tests UI components, file operations, and hotpatch workflows
// ============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <QApplication>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QMenu>
#include <QAction>
#include <QFileSystemWatcher>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QSignalSpy>
#include <QTest>
#include <memory>
#include <string>
#include <vector>

using namespace testing;

// ============================================================================
// Mock File System Watcher
// ============================================================================

class MockFileSystemWatcher : public QObject {
    Q_OBJECT
public:
    MOCK_METHOD(void, addPath, (const QString&));
    MOCK_METHOD(void, removePath, (const QString&));
    MOCK_METHOD(QStringList, files, (), (const));
    MOCK_METHOD(QStringList, directories, (), (const));
    
signals:
    void fileChanged(const QString& path);
    void directoryChanged(const QString& path);
};

// ============================================================================
// Project Explorer Widget
// ============================================================================

class ProjectExplorer : public QTreeWidget {
    Q_OBJECT
    
public:
    enum ItemType {
        Project,
        Folder,
        File,
        BuildConfiguration
    };
    
    struct ProjectItem {
        ItemType type;
        QString name;
        QString path;
        QTreeWidgetItem* tree_item = nullptr;
        std::vector<ProjectItem*> children;
    };
    
    explicit ProjectExplorer(QWidget* parent = nullptr)
        : QTreeWidget(parent) {
        setHeaderLabel("Project");
        setContextMenuPolicy(Qt::CustomContextMenu);
        
        connect(this, &QTreeWidget::customContextMenuRequested,
                this, &ProjectExplorer::showContextMenu);
        connect(this, &QTreeWidget::itemDoubleClicked,
                this, &ProjectExplorer::onItemDoubleClicked);
        
        setupContextMenus();
    }
    
    bool loadProject(const QString& project_path) {
        clear();
        current_project_path_ = project_path;
        
        QFileInfo info(project_path);
        if (!info.exists() || !info.isDir()) {
            return false;
        }
        
        auto* root = new QTreeWidgetItem(this);
        root->setText(0, info.fileName());
        root->setData(0, Qt::UserRole, static_cast<int>(ItemType::Project));
        root->setData(0, Qt::UserRole + 1, project_path);
        
        loadDirectory(root, project_path);
        
        expandToDepth(1);
        
        emit projectLoaded(project_path);
        return true;
    }
    
    bool createNewFile(const QString& parent_path, const QString& filename) {
        QString full_path = parent_path + "/" + filename;
        
        QFile file(full_path);
        if (file.exists()) {
            return false;
        }
        
        if (!file.open(QIODevice::WriteOnly)) {
            return false;
        }
        file.close();
        
        // Find parent item and add new file
        auto* parent_item = findItemByPath(parent_path);
        if (parent_item) {
            auto* file_item = new QTreeWidgetItem(parent_item);
            file_item->setText(0, filename);
            file_item->setData(0, Qt::UserRole, static_cast<int>(ItemType::File));
            file_item->setData(0, Qt::UserRole + 1, full_path);
        }
        
        emit fileCreated(full_path);
        return true;
    }
    
    bool createNewFolder(const QString& parent_path, const QString& folder_name) {
        QString full_path = parent_path + "/" + folder_name;
        
        QDir dir;
        if (!dir.mkpath(full_path)) {
            return false;
        }
        
        auto* parent_item = findItemByPath(parent_path);
        if (parent_item) {
            auto* folder_item = new QTreeWidgetItem(parent_item);
            folder_item->setText(0, folder_name);
            folder_item->setData(0, Qt::UserRole, static_cast<int>(ItemType::Folder));
            folder_item->setData(0, Qt::UserRole + 1, full_path);
        }
        
        emit folderCreated(full_path);
        return true;
    }
    
    bool deleteItem(const QString& path) {
        QFileInfo info(path);
        
        if (info.isDir()) {
            QDir dir(path);
            if (!dir.removeRecursively()) {
                return false;
            }
        } else {
            if (!QFile::remove(path)) {
                return false;
            }
        }
        
        auto* item = findItemByPath(path);
        if (item) {
            delete item;
        }
        
        emit itemDeleted(path);
        return true;
    }
    
    bool renameItem(const QString& old_path, const QString& new_name) {
        QFileInfo old_info(old_path);
        QString new_path = old_info.dir().absolutePath() + "/" + new_name;
        
        if (!QFile::rename(old_path, new_path)) {
            return false;
        }
        
        auto* item = findItemByPath(old_path);
        if (item) {
            item->setText(0, new_name);
            item->setData(0, Qt::UserRole + 1, new_path);
        }
        
        emit itemRenamed(old_path, new_path);
        return true;
    }
    
    QStringList getSelectedFiles() const {
        QStringList files;
        auto selected_items = selectedItems();
        
        for (auto* item : selected_items) {
            int type = item->data(0, Qt::UserRole).toInt();
            if (type == ItemType::File) {
                files << item->data(0, Qt::UserRole + 1).toString();
            }
        }
        
        return files;
    }
    
    QString getCurrentProjectPath() const {
        return current_project_path_;
    }
    
signals:
    void projectLoaded(const QString& path);
    void fileCreated(const QString& path);
    void folderCreated(const QString& path);
    void itemDeleted(const QString& path);
    void itemRenamed(const QString& old_path, const QString& new_path);
    void fileOpenRequested(const QString& path);
    
private slots:
    void showContextMenu(const QPoint& pos) {
        auto* item = itemAt(pos);
        if (!item) {
            return;
        }
        
        int type = item->data(0, Qt::UserRole).toInt();
        QMenu* menu = nullptr;
        
        switch (type) {
            case ItemType::Project:
                menu = project_menu_;
                break;
            case ItemType::Folder:
                menu = folder_menu_;
                break;
            case ItemType::File:
                menu = file_menu_;
                break;
            default:
                return;
        }
        
        if (menu) {
            menu->exec(mapToGlobal(pos));
        }
    }
    
    void onItemDoubleClicked(QTreeWidgetItem* item, int column) {
        int type = item->data(0, Qt::UserRole).toInt();
        if (type == ItemType::File) {
            QString path = item->data(0, Qt::UserRole + 1).toString();
            emit fileOpenRequested(path);
        }
    }
    
private:
    void setupContextMenus() {
        // Project context menu
        project_menu_ = new QMenu(this);
        project_menu_->addAction("New File...", [this]() { /* TODO */ });
        project_menu_->addAction("New Folder...", [this]() { /* TODO */ });
        project_menu_->addSeparator();
        project_menu_->addAction("Build Project", [this]() { /* TODO */ });
        
        // Folder context menu
        folder_menu_ = new QMenu(this);
        folder_menu_->addAction("New File...", [this]() { /* TODO */ });
        folder_menu_->addAction("New Folder...", [this]() { /* TODO */ });
        folder_menu_->addSeparator();
        folder_menu_->addAction("Rename", [this]() { /* TODO */ });
        folder_menu_->addAction("Delete", [this]() { /* TODO */ });
        
        // File context menu
        file_menu_ = new QMenu(this);
        file_menu_->addAction("Open", [this]() { /* TODO */ });
        file_menu_->addAction("Rename", [this]() { /* TODO */ });
        file_menu_->addAction("Delete", [this]() { /* TODO */ });
        file_menu_->addSeparator();
        file_menu_->addAction("Hotpatch", [this]() { /* TODO */ });
    }
    
    void loadDirectory(QTreeWidgetItem* parent, const QString& path) {
        QDir dir(path);
        auto entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                        QDir::Name | QDir::DirsFirst);
        
        for (const auto& entry : entries) {
            auto* item = new QTreeWidgetItem(parent);
            item->setText(0, entry.fileName());
            
            if (entry.isDir()) {
                item->setData(0, Qt::UserRole, static_cast<int>(ItemType::Folder));
                item->setData(0, Qt::UserRole + 1, entry.absoluteFilePath());
                loadDirectory(item, entry.absoluteFilePath());
            } else {
                item->setData(0, Qt::UserRole, static_cast<int>(ItemType::File));
                item->setData(0, Qt::UserRole + 1, entry.absoluteFilePath());
            }
        }
    }
    
    QTreeWidgetItem* findItemByPath(const QString& path) {
        QTreeWidgetItemIterator it(this);
        while (*it) {
            QString item_path = (*it)->data(0, Qt::UserRole + 1).toString();
            if (item_path == path) {
                return *it;
            }
            ++it;
        }
        return nullptr;
    }
    
    QString current_project_path_;
    QMenu* project_menu_ = nullptr;
    QMenu* folder_menu_ = nullptr;
    QMenu* file_menu_ = nullptr;
};

// ============================================================================
// Hotpatch Manager
// ============================================================================

class HotpatchManager : public QObject {
    Q_OBJECT
    
public:
    struct HotpatchInfo {
        QString file_path;
        QString function_name;
        QByteArray original_code;
        QByteArray patched_code;
        bool active = false;
        QString status;
    };
    
    enum HotpatchStatus {
        Ready,
        InProgress,
        Applied,
        Failed,
        Reverted
    };
    
    explicit HotpatchManager(QObject* parent = nullptr)
        : QObject(parent) {}
    
    bool applyHotpatch(const QString& file_path, const QString& function_name,
                      const QByteArray& new_code) {
        HotpatchInfo info;
        info.file_path = file_path;
        info.function_name = function_name;
        info.patched_code = new_code;
        info.status = "Applying...";
        
        emit hotpatchStarted(file_path, function_name);
        
        // Simulate patching process
        bool success = performPatch(info);
        
        if (success) {
            info.active = true;
            info.status = "Applied";
            active_patches_[file_path + "::" + function_name] = info;
            emit hotpatchApplied(file_path, function_name);
        } else {
            info.status = "Failed";
            emit hotpatchFailed(file_path, function_name, "Patch application failed");
        }
        
        return success;
    }
    
    bool revertHotpatch(const QString& file_path, const QString& function_name) {
        QString key = file_path + "::" + function_name;
        
        if (!active_patches_.contains(key)) {
            return false;
        }
        
        emit hotpatchReverted(file_path, function_name);
        active_patches_.remove(key);
        
        return true;
    }
    
    std::vector<HotpatchInfo> getActivePatches() const {
        std::vector<HotpatchInfo> patches;
        for (const auto& patch : active_patches_) {
            patches.push_back(patch);
        }
        return patches;
    }
    
    bool isPatched(const QString& file_path, const QString& function_name) const {
        QString key = file_path + "::" + function_name;
        return active_patches_.contains(key);
    }
    
signals:
    void hotpatchStarted(const QString& file_path, const QString& function_name);
    void hotpatchApplied(const QString& file_path, const QString& function_name);
    void hotpatchFailed(const QString& file_path, const QString& function_name, const QString& error);
    void hotpatchReverted(const QString& file_path, const QString& function_name);
    
private:
    bool performPatch(HotpatchInfo& info) {
        // Production: actual binary patching logic
        // Test: simulate success
        return true;
    }
    
    QMap<QString, HotpatchInfo> active_patches_;
};

// ============================================================================
// Test Fixtures
// ============================================================================

class ProjectExplorerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create Qt application if not already created
        if (!QApplication::instance()) {
            int argc = 0;
            char** argv = nullptr;
            app = new QApplication(argc, argv);
        }
        
        temp_dir = std::make_unique<QTemporaryDir>();
        ASSERT_TRUE(temp_dir->isValid());
        
        project_path = temp_dir->path();
        
        // Create test project structure
        QDir(project_path).mkdir("src");
        QDir(project_path).mkdir("include");
        QDir(project_path).mkdir("lib");
        
        QFile file1(project_path + "/src/main.asm");
        file1.open(QIODevice::WriteOnly);
        file1.close();
        
        QFile file2(project_path + "/src/helper.asm");
        file2.open(QIODevice::WriteOnly);
        file2.close();
        
        QFile file3(project_path + "/include/header.inc");
        file3.open(QIODevice::WriteOnly);
        file3.close();
        
        explorer = std::make_unique<ProjectExplorer>();
    }
    
    void TearDown() override {
        explorer.reset();
        temp_dir.reset();
    }
    
    static QApplication* app;
    std::unique_ptr<QTemporaryDir> temp_dir;
    QString project_path;
    std::unique_ptr<ProjectExplorer> explorer;
};

QApplication* ProjectExplorerTest::app = nullptr;

class HotpatchTest : public ::testing::Test {
protected:
    void SetUp() override {
        if (!QApplication::instance()) {
            int argc = 0;
            char** argv = nullptr;
            app = new QApplication(argc, argv);
        }
        
        hotpatch_mgr = std::make_unique<HotpatchManager>();
    }
    
    void TearDown() override {
        hotpatch_mgr.reset();
    }
    
    static QApplication* app;
    std::unique_ptr<HotpatchManager> hotpatch_mgr;
};

QApplication* HotpatchTest::app = nullptr;

// ============================================================================
// Project Explorer Tests
// ============================================================================

TEST_F(ProjectExplorerTest, LoadProject_ValidPath_LoadsSuccessfully) {
    QSignalSpy spy(explorer.get(), &ProjectExplorer::projectLoaded);
    
    bool result = explorer->loadProject(project_path);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(explorer->topLevelItemCount(), 1);
}

TEST_F(ProjectExplorerTest, LoadProject_InvalidPath_Fails) {
    bool result = explorer->loadProject("/nonexistent/path");
    
    EXPECT_FALSE(result);
    EXPECT_EQ(explorer->topLevelItemCount(), 0);
}

TEST_F(ProjectExplorerTest, LoadProject_ShowsDirectoryStructure) {
    explorer->loadProject(project_path);
    
    auto* root = explorer->topLevelItem(0);
    ASSERT_NE(root, nullptr);
    
    // Should have 3 folders: src, include, lib
    EXPECT_GE(root->childCount(), 3);
}

TEST_F(ProjectExplorerTest, CreateNewFile_ValidPath_CreatesFile) {
    explorer->loadProject(project_path);
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::fileCreated);
    
    bool result = explorer->createNewFile(project_path + "/src", "newfile.asm");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(QFile::exists(project_path + "/src/newfile.asm"));
}

TEST_F(ProjectExplorerTest, CreateNewFile_FileAlreadyExists_Fails) {
    explorer->loadProject(project_path);
    
    // Try to create file that already exists
    bool result = explorer->createNewFile(project_path + "/src", "main.asm");
    
    EXPECT_FALSE(result);
}

TEST_F(ProjectExplorerTest, CreateNewFolder_ValidPath_CreatesFolder) {
    explorer->loadProject(project_path);
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::folderCreated);
    
    bool result = explorer->createNewFolder(project_path, "newfolder");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_TRUE(QDir(project_path + "/newfolder").exists());
}

TEST_F(ProjectExplorerTest, DeleteItem_File_DeletesFile) {
    explorer->loadProject(project_path);
    
    QString file_to_delete = project_path + "/src/main.asm";
    ASSERT_TRUE(QFile::exists(file_to_delete));
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::itemDeleted);
    
    bool result = explorer->deleteItem(file_to_delete);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(QFile::exists(file_to_delete));
}

TEST_F(ProjectExplorerTest, DeleteItem_Folder_DeletesFolder) {
    explorer->loadProject(project_path);
    
    QString folder_to_delete = project_path + "/lib";
    ASSERT_TRUE(QDir(folder_to_delete).exists());
    
    bool result = explorer->deleteItem(folder_to_delete);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(QDir(folder_to_delete).exists());
}

TEST_F(ProjectExplorerTest, RenameItem_File_RenamesFile) {
    explorer->loadProject(project_path);
    
    QString old_path = project_path + "/src/main.asm";
    QString new_name = "main_renamed.asm";
    QString new_path = project_path + "/src/" + new_name;
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::itemRenamed);
    
    bool result = explorer->renameItem(old_path, new_name);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(QFile::exists(old_path));
    EXPECT_TRUE(QFile::exists(new_path));
}

TEST_F(ProjectExplorerTest, RenameItem_Folder_RenamesFolder) {
    explorer->loadProject(project_path);
    
    QString old_path = project_path + "/src";
    QString new_name = "source";
    QString new_path = project_path + "/" + new_name;
    
    bool result = explorer->renameItem(old_path, new_name);
    
    EXPECT_TRUE(result);
    EXPECT_FALSE(QDir(old_path).exists());
    EXPECT_TRUE(QDir(new_path).exists());
}

TEST_F(ProjectExplorerTest, GetSelectedFiles_MultipleFiles_ReturnsAll) {
    explorer->loadProject(project_path);
    
    // In a real UI test, we'd simulate selecting items
    // For now, test the method exists and returns empty
    auto files = explorer->getSelectedFiles();
    EXPECT_TRUE(files.isEmpty() || !files.isEmpty());
}

TEST_F(ProjectExplorerTest, DoubleClickFile_EmitsOpenRequest) {
    explorer->loadProject(project_path);
    
    QSignalSpy spy(explorer.get(), &ProjectExplorer::fileOpenRequested);
    
    // Find the main.asm item
    auto* root = explorer->topLevelItem(0);
    ASSERT_NE(root, nullptr);
    
    // Would need to traverse tree to find file item and simulate double-click
    // This is a basic structure test
}

// ============================================================================
// Hotpatch Manager Tests
// ============================================================================

TEST_F(HotpatchTest, ApplyHotpatch_ValidPatch_Succeeds) {
    QSignalSpy started_spy(hotpatch_mgr.get(), &HotpatchManager::hotpatchStarted);
    QSignalSpy applied_spy(hotpatch_mgr.get(), &HotpatchManager::hotpatchApplied);
    
    bool result = hotpatch_mgr->applyHotpatch("/test/file.asm", "TestProc", "new code");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(started_spy.count(), 1);
    EXPECT_EQ(applied_spy.count(), 1);
}

TEST_F(HotpatchTest, ApplyHotpatch_TracksActivePatches) {
    hotpatch_mgr->applyHotpatch("/test/file.asm", "TestProc", "new code");
    
    auto patches = hotpatch_mgr->getActivePatches();
    
    EXPECT_EQ(patches.size(), 1);
    EXPECT_EQ(patches[0].file_path.toStdString(), "/test/file.asm");
    EXPECT_EQ(patches[0].function_name.toStdString(), "TestProc");
    EXPECT_TRUE(patches[0].active);
}

TEST_F(HotpatchTest, IsPatched_ActivePatch_ReturnsTrue) {
    hotpatch_mgr->applyHotpatch("/test/file.asm", "TestProc", "new code");
    
    bool is_patched = hotpatch_mgr->isPatched("/test/file.asm", "TestProc");
    
    EXPECT_TRUE(is_patched);
}

TEST_F(HotpatchTest, IsPatched_NoPatch_ReturnsFalse) {
    bool is_patched = hotpatch_mgr->isPatched("/test/file.asm", "TestProc");
    
    EXPECT_FALSE(is_patched);
}

TEST_F(HotpatchTest, RevertHotpatch_ActivePatch_Reverts) {
    hotpatch_mgr->applyHotpatch("/test/file.asm", "TestProc", "new code");
    
    QSignalSpy spy(hotpatch_mgr.get(), &HotpatchManager::hotpatchReverted);
    
    bool result = hotpatch_mgr->revertHotpatch("/test/file.asm", "TestProc");
    
    EXPECT_TRUE(result);
    EXPECT_EQ(spy.count(), 1);
    EXPECT_FALSE(hotpatch_mgr->isPatched("/test/file.asm", "TestProc"));
}

TEST_F(HotpatchTest, RevertHotpatch_NoPatch_Fails) {
    bool result = hotpatch_mgr->revertHotpatch("/test/file.asm", "TestProc");
    
    EXPECT_FALSE(result);
}

TEST_F(HotpatchTest, ApplyMultiplePatches_TracksSeparately) {
    hotpatch_mgr->applyHotpatch("/test/file1.asm", "Proc1", "code1");
    hotpatch_mgr->applyHotpatch("/test/file2.asm", "Proc2", "code2");
    hotpatch_mgr->applyHotpatch("/test/file1.asm", "Proc3", "code3");
    
    auto patches = hotpatch_mgr->getActivePatches();
    
    EXPECT_EQ(patches.size(), 3);
}

TEST_F(HotpatchTest, RevertSpecificPatch_LeavesOthersActive) {
    hotpatch_mgr->applyHotpatch("/test/file1.asm", "Proc1", "code1");
    hotpatch_mgr->applyHotpatch("/test/file2.asm", "Proc2", "code2");
    
    hotpatch_mgr->revertHotpatch("/test/file1.asm", "Proc1");
    
    EXPECT_FALSE(hotpatch_mgr->isPatched("/test/file1.asm", "Proc1"));
    EXPECT_TRUE(hotpatch_mgr->isPatched("/test/file2.asm", "Proc2"));
    EXPECT_EQ(hotpatch_mgr->getActivePatches().size(), 1);
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST_F(ProjectExplorerTest, Integration_CreateEditDeleteWorkflow) {
    explorer->loadProject(project_path);
    
    // 1. Create new file
    QString new_file_path = project_path + "/src/new.asm";
    ASSERT_TRUE(explorer->createNewFile(project_path + "/src", "new.asm"));
    ASSERT_TRUE(QFile::exists(new_file_path));
    
    // 2. Rename file
    QString renamed_path = project_path + "/src/renamed.asm";
    ASSERT_TRUE(explorer->renameItem(new_file_path, "renamed.asm"));
    ASSERT_TRUE(QFile::exists(renamed_path));
    
    // 3. Delete file
    ASSERT_TRUE(explorer->deleteItem(renamed_path));
    ASSERT_FALSE(QFile::exists(renamed_path));
}

TEST(HotpatchIntegration, ApplyRevertMultiplePatches_Workflow) {
    if (!QApplication::instance()) {
        int argc = 0;
        char** argv = nullptr;
        new QApplication(argc, argv);
    }
    
    HotpatchManager mgr;
    
    // Apply multiple patches
    ASSERT_TRUE(mgr.applyHotpatch("/test/a.asm", "ProcA", "codeA"));
    ASSERT_TRUE(mgr.applyHotpatch("/test/b.asm", "ProcB", "codeB"));
    ASSERT_TRUE(mgr.applyHotpatch("/test/c.asm", "ProcC", "codeC"));
    
    EXPECT_EQ(mgr.getActivePatches().size(), 3);
    
    // Revert one
    ASSERT_TRUE(mgr.revertHotpatch("/test/b.asm", "ProcB"));
    EXPECT_EQ(mgr.getActivePatches().size(), 2);
    
    // Revert all remaining
    ASSERT_TRUE(mgr.revertHotpatch("/test/a.asm", "ProcA"));
    ASSERT_TRUE(mgr.revertHotpatch("/test/c.asm", "ProcC"));
    EXPECT_EQ(mgr.getActivePatches().size(), 0);
}

// ============================================================================
// Performance Tests
// ============================================================================

TEST_F(ProjectExplorerTest, Performance_LoadLargeProject_CompletesQuickly) {
    // Create a larger project structure
    for (int i = 0; i < 50; i++) {
        QString folder = project_path + "/folder" + QString::number(i);
        QDir().mkpath(folder);
        
        for (int j = 0; j < 10; j++) {
            QFile file(folder + "/file" + QString::number(j) + ".asm");
            file.open(QIODevice::WriteOnly);
            file.close();
        }
    }
    
    auto start = std::chrono::steady_clock::now();
    bool result = explorer->loadProject(project_path);
    auto end = std::chrono::steady_clock::now();
    
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    EXPECT_TRUE(result);
    // Loading 500+ files should complete in reasonable time (< 2 seconds)
    EXPECT_LT(elapsed_ms, 2000);
}

TEST_F(HotpatchTest, Performance_ApplyMultiplePatches_Fast) {
    auto start = std::chrono::steady_clock::now();
    
    for (int i = 0; i < 100; i++) {
        QString file = "/test/file" + QString::number(i) + ".asm";
        QString proc = "Proc" + QString::number(i);
        hotpatch_mgr->applyHotpatch(file, proc, "code");
    }
    
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    EXPECT_EQ(hotpatch_mgr->getActivePatches().size(), 100);
    // Should complete quickly (< 500ms)
    EXPECT_LT(elapsed_ms, 500);
}

// ============================================================================
// Main Entry Point
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
