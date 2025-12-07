#pragma once

#include <QWidget>
#include <QString>

class QTabWidget;

class MultiTabEditor : public QWidget {
    Q_OBJECT
public:
    explicit MultiTabEditor(QWidget* parent = nullptr);
    
public slots:
    void openFile(const QString& filepath);
    void newFile();
    void saveCurrentFile();
    void undo();
    void redo();
    void find();
    void replace();

    QString getCurrentText() const;
    QString getSelectedText() const;
    QString getCurrentFilePath() const;private:
    QTabWidget* tab_widget_;
    QMap<QWidget*, QString> tab_file_paths_;  // Maps editor widget to file path
};
