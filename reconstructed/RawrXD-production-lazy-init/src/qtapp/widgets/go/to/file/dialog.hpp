// go_to_file_dialog.hpp - Quick file opener with fuzzy search
#pragma once

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QTimer>

class GoToFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit GoToFileDialog(const QStringList& allFiles, QWidget* parent = nullptr);
    
    QString selectedFile() const { return m_selectedFile; }
    
protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void onSearchTextChanged(const QString& text);
    void onItemActivated(QListWidgetItem* item);
    void onItemDoubleClicked(QListWidgetItem* item);

private:
    void filterFiles(const QString& pattern);
    int fuzzyMatchScore(const QString& fileName, const QString& pattern) const;
    
    QLineEdit* m_searchBox;
    QListWidget* m_fileList;
    QLabel* m_statusLabel;
    QStringList m_allFiles;
    QString m_selectedFile;
    QTimer* m_filterTimer;
};
