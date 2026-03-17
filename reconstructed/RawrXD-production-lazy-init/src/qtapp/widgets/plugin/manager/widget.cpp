/**
 * @file plugin_manager_widget.cpp
 * @brief Implementation of PluginManagerWidget - Plugin management
 */

#include "plugin_manager_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QListWidget>
#include <QMessageBox>

PluginManagerWidget::PluginManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    connectSignals();
    setWindowTitle("Plugin Manager");
}

PluginManagerWidget::~PluginManagerWidget() = default;

void PluginManagerWidget::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    
    mPluginList = new QListWidget(this);
    mPluginList->addItem("Plugin 1 - Enabled");
    mPluginList->addItem("Plugin 2 - Enabled");
    mPluginList->addItem("Plugin 3 - Disabled");
    mMainLayout->addWidget(mPluginList);
    
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    mInstallButton = new QPushButton("Install", this);
    mInstallButton->setStyleSheet("background-color: #4CAF50; color: white;");
    buttonLayout->addWidget(mInstallButton);
    
    mUninstallButton = new QPushButton("Uninstall", this);
    buttonLayout->addWidget(mUninstallButton);
    
    mBrowseButton = new QPushButton("Browse Plugins", this);
    buttonLayout->addWidget(mBrowseButton);
    
    buttonLayout->addStretch();
    mMainLayout->addLayout(buttonLayout);
}

void PluginManagerWidget::connectSignals()
{
    connect(mInstallButton, &QPushButton::clicked, this, &PluginManagerWidget::onInstallPlugin);
    connect(mUninstallButton, &QPushButton::clicked, this, &PluginManagerWidget::onUninstallPlugin);
    connect(mBrowseButton, &QPushButton::clicked, this, &PluginManagerWidget::onBrowsePlugins);
}

void PluginManagerWidget::onInstallPlugin()
{
    QMessageBox::information(this, "Install", "Install plugin dialog would open here.");
}

void PluginManagerWidget::onUninstallPlugin()
{
    int row = mPluginList->currentRow();
    if (row >= 0) {
        delete mPluginList->takeItem(row);
        QMessageBox::information(this, "Uninstalled", "Plugin removed.");
    }
}

void PluginManagerWidget::onTogglePlugin()
{
    QMessageBox::information(this, "Toggle", "Plugin toggled.");
}

void PluginManagerWidget::onBrowsePlugins()
{
    QMessageBox::information(this, "Browse", "Plugin marketplace would open here.");
}
