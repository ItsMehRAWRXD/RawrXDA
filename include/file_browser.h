#pragma once

#include <QWidget>
#include <QString>

class QTreeWidget;
class QTreeWidgetItem;

class FileBrowser : public QWidget {
    Q_OBJECT
public:
    explicit FileBrowser(QWidget* parent = nullptr);
    
    void loadDirectory(const QString& dirpath);
    void loadDrives();
    
signals:
    void fileSelected(const QString& filepath);
    
private slots:
    void handleItemClicked(QTreeWidgetItem* item, int column);
    void handleItemExpanded(QTreeWidgetItem* item);
    
private:
    QTreeWidget* tree_widget_;
};
