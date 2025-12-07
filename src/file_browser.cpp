// File Browser - File system navigation
#include "file_browser.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QFileInfo>
#include <QDir>
#include <QDebug>

FileBrowser::FileBrowser(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* title = new QLabel("File Browser", this);
    title->setStyleSheet("font-weight: bold; font-size: 12px; color: #d4d4d4;");
    layout->addWidget(title);
    
    tree_widget_ = new QTreeWidget(this);
    tree_widget_->setHeaderLabel("Files");
    tree_widget_->setStyleSheet(
        "QTreeWidget { background-color: #252526; color: #d4d4d4; border: none; }"
        "QTreeWidget::item:selected { background-color: #37373d; }");
    layout->addWidget(tree_widget_);
    
    connect(tree_widget_, &QTreeWidget::itemClicked,
            this, &FileBrowser::handleItemClicked);
    connect(tree_widget_, &QTreeWidget::itemExpanded,
            this, &FileBrowser::handleItemExpanded);
    
    loadDrives();
}

void FileBrowser::loadDrives() {
    tree_widget_->clear();
    
    // On Windows, show drives using QDir
#ifdef Q_OS_WIN
    QDir dir;
    QStringList drives;
    
    // Get all drives (C:, D:, etc.)
    for (char drive = 'A'; drive <= 'Z'; ++drive) {
        QString drivePath = QString(drive) + ":/";
        QDir testDir(drivePath);
        if (testDir.exists()) {
            drives << drivePath;
        }
    }
    
    for (const QString& drive : drives) {
        QTreeWidgetItem* driveItem = new QTreeWidgetItem();
        driveItem->setText(0, drive);
        driveItem->setData(0, Qt::UserRole, drive);
        driveItem->setData(0, Qt::UserRole + 1, "drive");
        tree_widget_->addTopLevelItem(driveItem);
        
        // Add a placeholder child to make it expandable
        QTreeWidgetItem* placeholder = new QTreeWidgetItem();
        placeholder->setText(0, "Loading...");
        driveItem->addChild(placeholder);
    }
#else
    // On other systems, show root directory
    loadDirectory(QDir::rootPath());
#endif
}

void FileBrowser::loadDirectory(const QString& dirpath) {
    QDir dir(dirpath);
    QFileInfoList entries = dir.entryInfoList(
        QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
        QDir::Name);
    
    for (const QFileInfo& info : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, info.fileName());
        item->setData(0, Qt::UserRole, info.absoluteFilePath());
        item->setData(0, Qt::UserRole + 1, info.isDir() ? "dir" : "file");
        item->setChildIndicatorPolicy(
            info.isDir() ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicator);
        tree_widget_->addTopLevelItem(item);
    }
}

void FileBrowser::handleItemClicked(QTreeWidgetItem* item, int column) {
    QString filepath = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    
    if (type == "file") {
        emit fileSelected(filepath);
    }
}

void FileBrowser::handleItemExpanded(QTreeWidgetItem* item) {
    QString filepath = item->data(0, Qt::UserRole).toString();
    QString type = item->data(0, Qt::UserRole + 1).toString();
    
    if (type == "drive" || type == "dir") {
        // Clear placeholder children
        item->takeChildren();
        
        // Load directory contents
        QDir dir(filepath);
        QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
            QDir::Name);
        
        for (const QFileInfo& info : entries) {
            QTreeWidgetItem* childItem = new QTreeWidgetItem();
            childItem->setText(0, info.fileName());
            childItem->setData(0, Qt::UserRole, info.absoluteFilePath());
            childItem->setData(0, Qt::UserRole + 1, info.isDir() ? "dir" : "file");
            childItem->setChildIndicatorPolicy(
                info.isDir() ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicator);
            item->addChild(childItem);
            
            // If it's a directory, add a placeholder child to make it expandable
            if (info.isDir()) {
                QTreeWidgetItem* placeholder = new QTreeWidgetItem();
                placeholder->setText(0, "Loading...");
                childItem->addChild(placeholder);
            }
        }
    }
}
