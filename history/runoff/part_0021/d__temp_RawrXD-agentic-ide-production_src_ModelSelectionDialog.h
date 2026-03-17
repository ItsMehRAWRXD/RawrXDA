#ifndef MODELSELECTIONDIALOG_H
#define MODELSELECTIONDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QString>
#include "ModelLoaderBridge.h"

class ModelSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModelSelectionDialog(ModelLoaderBridge* loader, QWidget* parent = nullptr);
    
    QString getSelectedModel() const;
    bool isModelLoaded() const;

private slots:
    void onLoadModel();
    void onModelDoubleClicked(QListWidgetItem* item);
    void onRefreshList();
    void onModelLoaded(const QString& name, uint64_t sizeBytes);

private:
    void setupUI();
    void populateModelList();
    
    ModelLoaderBridge* m_loader;
    QListWidget* m_modelList;
    QPushButton* m_loadButton;
    QPushButton* m_refreshButton;
    QPushButton* m_okButton;
    QLabel* m_statusLabel;
    QString m_selectedModel;
    bool m_modelLoaded = false;
};

#endif // MODELSELECTIONDIALOG_H
