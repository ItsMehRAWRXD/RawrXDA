/**
 * @file plugin_manager_widget.h
 * @brief Header for PluginManagerWidget - Plugin management
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QPushButton;
class QListWidget;
class QCheckBox;

class PluginManagerWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit PluginManagerWidget(QWidget* parent = nullptr);
    ~PluginManagerWidget();
    
private slots:
    void onInstallPlugin();
    void onUninstallPlugin();
    void onTogglePlugin();
    void onBrowsePlugins();
    
private:
    void setupUI();
    void connectSignals();
    
    QVBoxLayout* mMainLayout;
    QPushButton* mInstallButton;
    QPushButton* mUninstallButton;
    QPushButton* mBrowseButton;
    QListWidget* mPluginList;
};

#endif // PLUGIN_MANAGER_WIDGET_H
