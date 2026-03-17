#ifndef EXTENSION_PANEL_H
#define EXTENSION_PANEL_H

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "extension_manager.h"

namespace IDE {

class ExtensionPanel : public QWidget {
    Q_OBJECT

public:
    explicit ExtensionPanel(QWidget* parent = nullptr);
    ~ExtensionPanel() override;

    void refreshExtensionList();

signals:
    void extensionEnabled(const QString& name);
    void extensionDisabled(const QString& name);
    void extensionInstalled(const QString& name);

private slots:
    void onExtensionSelected(QListWidgetItem* item);
    void onCreateClicked();
    void onInstallClicked();
    void onEnableClicked();
    void onDisableClicked();
    void onUninstallClicked();
    void onRemoveClicked();
    void onRefreshClicked();

private:
    void setupUI();
    void updateExtensionDetails();
    QString getCurrentExtensionName() const;
    void showMessage(const QString& message, bool isError = false);

    // UI Components
    QListWidget* extensionList_;
    QLabel* statusLabel_;
    QLabel* detailsLabel_;
    QPushButton* createBtn_;
    QPushButton* installBtn_;
    QPushButton* enableBtn_;
    QPushButton* disableBtn_;
    QPushButton* uninstallBtn_;
    QPushButton* removeBtn_;
    QPushButton* refreshBtn_;

    ExtensionManager& extManager_;
};

} // namespace IDE

#endif // EXTENSION_PANEL_H
