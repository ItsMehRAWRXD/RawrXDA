/**
 * @file docker_tool_widget.cpp
 * @brief Implementation of Docker management widget
 */

#include "docker_tool_widget.h"
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QInputDialog>
#include <QStyle>
#include <QApplication>
#include <QCheckBox>
#include <QScrollBar>

// ============================================================================
// ContainerStatsWidget Implementation
// ============================================================================

ContainerStatsWidget::ContainerStatsWidget(QWidget* parent)
    : QWidget(parent)
    , m_statsProcess(new QProcess(this))
    , m_updateTimer(new QTimer(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    m_containerNameLabel = new QLabel(tr("No container selected"));
    m_containerNameLabel->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    QFormLayout* statsLayout = new QFormLayout();
    
    m_cpuBar = new QProgressBar();
    m_cpuBar->setRange(0, 100);
    m_cpuBar->setTextVisible(true);
    m_cpuBar->setFormat("%v%");
    
    m_memoryBar = new QProgressBar();
    m_memoryBar->setRange(0, 100);
    m_memoryBar->setTextVisible(true);
    m_memoryBar->setFormat("%v%");
    
    m_cpuLabel = new QLabel("0%");
    m_memoryLabel = new QLabel("0 B / 0 B");
    m_networkLabel = new QLabel("↓ 0 B / ↑ 0 B");
    m_blockIOLabel = new QLabel("Read: 0 B / Write: 0 B");
    
    statsLayout->addRow(tr("CPU:"), m_cpuBar);
    statsLayout->addRow(tr("Memory:"), m_memoryBar);
    statsLayout->addRow(tr("Memory Usage:"), m_memoryLabel);
    statsLayout->addRow(tr("Network I/O:"), m_networkLabel);
    statsLayout->addRow(tr("Block I/O:"), m_blockIOLabel);
    
    layout->addWidget(m_containerNameLabel);
    layout->addLayout(statsLayout);
    layout->addStretch();
    
    connect(m_statsProcess, &QProcess::readyReadStandardOutput,
            this, &ContainerStatsWidget::onStatsReceived);
    connect(m_updateTimer, &QTimer::timeout, [this]() {
        if (!m_containerId.isEmpty()) {
            startMonitoring();
        }
    });
}

void ContainerStatsWidget::setContainer(const QString& containerId) {
    stopMonitoring();
    m_containerId = containerId;
    if (!containerId.isEmpty()) {
        startMonitoring();
    }
}

void ContainerStatsWidget::startMonitoring() {
    if (m_containerId.isEmpty()) return;
    
    m_statsProcess->start("docker", {"stats", "--no-stream", "--format", 
        "{{json .}}", m_containerId});
}

void ContainerStatsWidget::stopMonitoring() {
    m_statsProcess->terminate();
    m_updateTimer->stop();
}

void ContainerStatsWidget::onStatsReceived() {
    QString output = m_statsProcess->readAllStandardOutput();
    QJsonDocument doc = QJsonDocument::fromJson(output.toUtf8());
    
    if (doc.isObject()) {
        QJsonObject obj = doc.object();
        
        DockerContainer container;
        container.id = m_containerId;
        container.name = obj["Name"].toString();
        
        // Parse CPU percentage
        QString cpuStr = obj["CPUPerc"].toString();
        cpuStr.remove('%');
        container.cpuPercent = cpuStr.toDouble();
        
        // Parse memory
        QString memStr = obj["MemUsage"].toString();
        // Format: "100MiB / 1GiB"
        
        updateDisplay(container);
        emit statsUpdated(container);
    }
    
    // Schedule next update
    m_updateTimer->start(2000);
}

void ContainerStatsWidget::updateDisplay(const DockerContainer& container) {
    m_containerNameLabel->setText(container.name.isEmpty() ? container.id : container.name);
    m_cpuBar->setValue(static_cast<int>(container.cpuPercent));
    m_cpuLabel->setText(QString("%1%").arg(container.cpuPercent, 0, 'f', 1));
    
    if (container.memoryLimit > 0) {
        int memPercent = static_cast<int>((container.memoryUsage * 100) / container.memoryLimit);
        m_memoryBar->setValue(memPercent);
    }
}

// ============================================================================
// ContainerLogsWidget Implementation
// ============================================================================

ContainerLogsWidget::ContainerLogsWidget(QWidget* parent)
    : QWidget(parent)
    , m_logsProcess(new QProcess(this))
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Filter logs..."));
    m_filterEdit->setClearButtonEnabled(true);
    
    m_followBtn = new QPushButton(tr("Follow"));
    m_followBtn->setCheckable(true);
    
    m_timestampsBtn = new QPushButton(tr("Timestamps"));
    m_timestampsBtn->setCheckable(true);
    m_timestampsBtn->setChecked(true);
    
    m_tailCombo = new QComboBox();
    m_tailCombo->addItem(tr("Last 100 lines"), 100);
    m_tailCombo->addItem(tr("Last 500 lines"), 500);
    m_tailCombo->addItem(tr("Last 1000 lines"), 1000);
    m_tailCombo->addItem(tr("All"), -1);
    
    m_clearBtn = new QPushButton(tr("Clear"));
    m_refreshBtn = new QPushButton(tr("Refresh"));
    
    toolbar->addWidget(m_filterEdit, 1);
    toolbar->addWidget(m_tailCombo);
    toolbar->addWidget(m_timestampsBtn);
    toolbar->addWidget(m_followBtn);
    toolbar->addWidget(m_clearBtn);
    toolbar->addWidget(m_refreshBtn);
    
    // Logs view
    m_logsView = new QTextEdit();
    m_logsView->setReadOnly(true);
    m_logsView->setFont(QFont("Consolas", 10));
    m_logsView->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #333;"
        "}"
    );
    
    layout->addLayout(toolbar);
    layout->addWidget(m_logsView, 1);
    
    connect(m_filterEdit, &QLineEdit::textChanged,
            this, &ContainerLogsWidget::onFilterChanged);
    connect(m_followBtn, &QPushButton::toggled,
            this, &ContainerLogsWidget::onFollowToggled);
    connect(m_timestampsBtn, &QPushButton::toggled,
            this, &ContainerLogsWidget::onTimestampsToggled);
    connect(m_clearBtn, &QPushButton::clicked,
            this, &ContainerLogsWidget::clearLogs);
    connect(m_refreshBtn, &QPushButton::clicked,
            this, &ContainerLogsWidget::refresh);
    connect(m_logsProcess, &QProcess::readyReadStandardOutput,
            this, &ContainerLogsWidget::onLogsReceived);
}

void ContainerLogsWidget::setContainer(const QString& containerId) {
    m_containerId = containerId;
    if (!containerId.isEmpty()) {
        refresh();
    }
}

void ContainerLogsWidget::setFollow(bool follow) {
    m_follow = follow;
    m_followBtn->setChecked(follow);
}

void ContainerLogsWidget::setTailLines(int lines) {
    m_tailLines = lines;
}

void ContainerLogsWidget::clearLogs() {
    m_logsView->clear();
}

void ContainerLogsWidget::refresh() {
    if (m_containerId.isEmpty()) return;
    
    m_logsProcess->terminate();
    m_logsProcess->waitForFinished(1000);
    
    QStringList args = {"logs"};
    
    if (m_tailLines > 0) {
        args << "--tail" << QString::number(m_tailLines);
    }
    
    if (m_showTimestamps) {
        args << "--timestamps";
    }
    
    if (m_follow) {
        args << "--follow";
    }
    
    args << m_containerId;
    
    if (!m_follow) {
        clearLogs();
    }
    
    m_logsProcess->start("docker", args);
}

void ContainerLogsWidget::onLogsReceived() {
    QString output = m_logsProcess->readAllStandardOutput();
    QString error = m_logsProcess->readAllStandardError();
    
    if (!output.isEmpty()) {
        m_logsView->append(output);
        emit logReceived(output);
    }
    if (!error.isEmpty()) {
        m_logsView->append(QString("<span style='color: #ff6b6b;'>%1</span>").arg(error));
    }
    
    if (m_follow) {
        m_logsView->verticalScrollBar()->setValue(
            m_logsView->verticalScrollBar()->maximum());
    }
}

void ContainerLogsWidget::onFilterChanged(const QString& filter) {
    // Simple text highlighting filter
    QString text = m_logsView->toPlainText();
    m_logsView->clear();
    
    if (filter.isEmpty()) {
        m_logsView->setPlainText(text);
        return;
    }
    
    QStringList lines = text.split('\n');
    for (const QString& line : lines) {
        if (line.contains(filter, Qt::CaseInsensitive)) {
            QString highlighted = line;
            highlighted.replace(filter, QString("<mark>%1</mark>").arg(filter), Qt::CaseInsensitive);
            m_logsView->append(highlighted);
        }
    }
}

void ContainerLogsWidget::onFollowToggled(bool checked) {
    m_follow = checked;
    refresh();
}

void ContainerLogsWidget::onTimestampsToggled(bool checked) {
    m_showTimestamps = checked;
    refresh();
}

// ============================================================================
// DockerToolWidget Implementation
// ============================================================================

DockerToolWidget::DockerToolWidget(QWidget* parent)
    : QWidget(parent)
    , m_autoRefreshTimer(new QTimer(this))
{
    setupUI();
    setupConnections();
    checkDockerStatus();
    
    m_autoRefreshTimer->setInterval(10000);  // 10 seconds
    connect(m_autoRefreshTimer, &QTimer::timeout, this, &DockerToolWidget::onAutoRefresh);
}

DockerToolWidget::~DockerToolWidget() {
    if (m_currentProcess) {
        m_currentProcess->terminate();
        m_currentProcess->waitForFinished(3000);
    }
}

void DockerToolWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    
    // Status bar at top
    QHBoxLayout* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Checking Docker status..."));
    m_dockerVersionLabel = new QLabel();
    m_refreshBtn = new QPushButton(tr("Refresh"));
    m_refreshBtn->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    
    statusLayout->addWidget(m_statusLabel);
    statusLayout->addStretch();
    statusLayout->addWidget(m_dockerVersionLabel);
    statusLayout->addWidget(m_refreshBtn);
    mainLayout->addLayout(statusLayout);
    
    // Tab widget
    m_tabWidget = new QTabWidget();
    
    setupContainersTab();
    setupImagesTab();
    setupVolumesTab();
    setupNetworksTab();
    setupComposeTab();
    
    mainLayout->addWidget(m_tabWidget, 1);
}

void DockerToolWidget::setupContainersTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_containerFilterCombo = new QComboBox();
    m_containerFilterCombo->addItem(tr("All Containers"), "all");
    m_containerFilterCombo->addItem(tr("Running"), "running");
    m_containerFilterCombo->addItem(tr("Stopped"), "exited");
    m_containerFilterCombo->addItem(tr("Paused"), "paused");
    
    m_startContainerBtn = new QPushButton(tr("Start"));
    m_startContainerBtn->setEnabled(false);
    m_stopContainerBtn = new QPushButton(tr("Stop"));
    m_stopContainerBtn->setEnabled(false);
    m_restartContainerBtn = new QPushButton(tr("Restart"));
    m_restartContainerBtn->setEnabled(false);
    m_removeContainerBtn = new QPushButton(tr("Remove"));
    m_removeContainerBtn->setEnabled(false);
    
    toolbar->addWidget(new QLabel(tr("Filter:")));
    toolbar->addWidget(m_containerFilterCombo);
    toolbar->addStretch();
    toolbar->addWidget(m_startContainerBtn);
    toolbar->addWidget(m_stopContainerBtn);
    toolbar->addWidget(m_restartContainerBtn);
    toolbar->addWidget(m_removeContainerBtn);
    
    layout->addLayout(toolbar);
    
    // Splitter with tree and details
    QSplitter* splitter = new QSplitter(Qt::Vertical);
    
    // Containers tree
    m_containersTree = new QTreeWidget();
    m_containersTree->setHeaderLabels({
        tr("Name"), tr("Image"), tr("Status"), tr("Ports"), tr("Created")
    });
    m_containersTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_containersTree->setAlternatingRowColors(true);
    m_containersTree->setRootIsDecorated(false);
    m_containersTree->header()->setStretchLastSection(false);
    m_containersTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    
    splitter->addWidget(m_containersTree);
    
    // Details tabs
    QTabWidget* detailsTabs = new QTabWidget();
    m_logsWidget = new ContainerLogsWidget();
    m_statsWidget = new ContainerStatsWidget();
    
    detailsTabs->addTab(m_logsWidget, tr("Logs"));
    detailsTabs->addTab(m_statsWidget, tr("Stats"));
    
    splitter->addWidget(detailsTabs);
    splitter->setSizes({400, 200});
    
    layout->addWidget(splitter, 1);
    
    m_tabWidget->addTab(tab, tr("Containers"));
}

void DockerToolWidget::setupImagesTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_pullImageEdit = new QLineEdit();
    m_pullImageEdit->setPlaceholderText(tr("Image name (e.g., nginx:latest)"));
    
    m_pullImageBtn = new QPushButton(tr("Pull"));
    m_buildImageBtn = new QPushButton(tr("Build..."));
    m_removeImageBtn = new QPushButton(tr("Remove"));
    m_removeImageBtn->setEnabled(false);
    
    m_showDanglingCheck = new QCheckBox(tr("Show dangling"));
    
    toolbar->addWidget(m_pullImageEdit, 1);
    toolbar->addWidget(m_pullImageBtn);
    toolbar->addWidget(m_buildImageBtn);
    toolbar->addWidget(m_removeImageBtn);
    toolbar->addWidget(m_showDanglingCheck);
    
    layout->addLayout(toolbar);
    
    // Images tree
    m_imagesTree = new QTreeWidget();
    m_imagesTree->setHeaderLabels({
        tr("Repository"), tr("Tag"), tr("Image ID"), tr("Created"), tr("Size")
    });
    m_imagesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_imagesTree->setAlternatingRowColors(true);
    m_imagesTree->setRootIsDecorated(false);
    m_imagesTree->header()->setStretchLastSection(false);
    m_imagesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    
    layout->addWidget(m_imagesTree, 1);
    
    m_tabWidget->addTab(tab, tr("Images"));
}

void DockerToolWidget::setupVolumesTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_createVolumeBtn = new QPushButton(tr("Create Volume"));
    m_removeVolumeBtn = new QPushButton(tr("Remove"));
    m_removeVolumeBtn->setEnabled(false);
    m_pruneVolumesBtn = new QPushButton(tr("Prune Unused"));
    
    toolbar->addWidget(m_createVolumeBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_removeVolumeBtn);
    toolbar->addWidget(m_pruneVolumesBtn);
    
    layout->addLayout(toolbar);
    
    // Volumes tree
    m_volumesTree = new QTreeWidget();
    m_volumesTree->setHeaderLabels({
        tr("Name"), tr("Driver"), tr("Mountpoint"), tr("Scope")
    });
    m_volumesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_volumesTree->setAlternatingRowColors(true);
    m_volumesTree->setRootIsDecorated(false);
    m_volumesTree->header()->setStretchLastSection(false);
    m_volumesTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_volumesTree->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    
    layout->addWidget(m_volumesTree, 1);
    
    m_tabWidget->addTab(tab, tr("Volumes"));
}

void DockerToolWidget::setupNetworksTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Toolbar
    QHBoxLayout* toolbar = new QHBoxLayout();
    
    m_createNetworkBtn = new QPushButton(tr("Create Network"));
    m_removeNetworkBtn = new QPushButton(tr("Remove"));
    m_removeNetworkBtn->setEnabled(false);
    
    toolbar->addWidget(m_createNetworkBtn);
    toolbar->addStretch();
    toolbar->addWidget(m_removeNetworkBtn);
    
    layout->addLayout(toolbar);
    
    // Networks tree
    m_networksTree = new QTreeWidget();
    m_networksTree->setHeaderLabels({
        tr("Name"), tr("ID"), tr("Driver"), tr("Scope"), tr("Subnet"), tr("Gateway")
    });
    m_networksTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_networksTree->setAlternatingRowColors(true);
    m_networksTree->setRootIsDecorated(false);
    m_networksTree->header()->setStretchLastSection(false);
    m_networksTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    
    layout->addWidget(m_networksTree, 1);
    
    m_tabWidget->addTab(tab, tr("Networks"));
}

void DockerToolWidget::setupComposeTab() {
    QWidget* tab = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(tab);
    
    // Compose file selection
    QHBoxLayout* fileLayout = new QHBoxLayout();
    
    m_composePathEdit = new QLineEdit();
    m_composePathEdit->setPlaceholderText(tr("docker-compose.yml path"));
    
    m_browseComposeBtn = new QPushButton(tr("Browse..."));
    
    fileLayout->addWidget(new QLabel(tr("Compose file:")));
    fileLayout->addWidget(m_composePathEdit, 1);
    fileLayout->addWidget(m_browseComposeBtn);
    
    layout->addLayout(fileLayout);
    
    // Compose controls
    QHBoxLayout* controlsLayout = new QHBoxLayout();
    
    m_composeUpBtn = new QPushButton(tr("Up"));
    m_composeUpBtn->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    m_composeDownBtn = new QPushButton(tr("Down"));
    m_composeDownBtn->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    m_composeStartBtn = new QPushButton(tr("Start"));
    m_composeStopBtn = new QPushButton(tr("Stop"));
    
    controlsLayout->addWidget(m_composeUpBtn);
    controlsLayout->addWidget(m_composeDownBtn);
    controlsLayout->addWidget(m_composeStartBtn);
    controlsLayout->addWidget(m_composeStopBtn);
    controlsLayout->addStretch();
    
    layout->addLayout(controlsLayout);
    
    // Splitter with services tree and output
    QSplitter* splitter = new QSplitter(Qt::Vertical);
    
    // Services tree
    m_composeTree = new QTreeWidget();
    m_composeTree->setHeaderLabels({
        tr("Service"), tr("Image"), tr("Status"), tr("Ports")
    });
    m_composeTree->setAlternatingRowColors(true);
    m_composeTree->setRootIsDecorated(false);
    
    splitter->addWidget(m_composeTree);
    
    // Output
    m_composeOutput = new QTextEdit();
    m_composeOutput->setReadOnly(true);
    m_composeOutput->setFont(QFont("Consolas", 10));
    
    splitter->addWidget(m_composeOutput);
    splitter->setSizes({300, 200});
    
    layout->addWidget(splitter, 1);
    
    m_tabWidget->addTab(tab, tr("Compose"));
}

void DockerToolWidget::setupConnections() {
    // General
    connect(m_refreshBtn, &QPushButton::clicked, this, &DockerToolWidget::onRefreshClicked);
    
    // Containers
    connect(m_containersTree, &QTreeWidget::customContextMenuRequested,
            this, &DockerToolWidget::onContainerContextMenu);
    connect(m_containersTree, &QTreeWidget::itemClicked,
            this, &DockerToolWidget::onContainerSelected);
    connect(m_startContainerBtn, &QPushButton::clicked, [this]() {
        auto item = m_containersTree->currentItem();
        if (item) startContainer(item->data(0, Qt::UserRole).toString());
    });
    connect(m_stopContainerBtn, &QPushButton::clicked, [this]() {
        auto item = m_containersTree->currentItem();
        if (item) stopContainer(item->data(0, Qt::UserRole).toString());
    });
    connect(m_restartContainerBtn, &QPushButton::clicked, [this]() {
        auto item = m_containersTree->currentItem();
        if (item) restartContainer(item->data(0, Qt::UserRole).toString());
    });
    connect(m_removeContainerBtn, &QPushButton::clicked, [this]() {
        auto item = m_containersTree->currentItem();
        if (item) removeContainer(item->data(0, Qt::UserRole).toString());
    });
    
    // Images
    connect(m_imagesTree, &QTreeWidget::customContextMenuRequested,
            this, &DockerToolWidget::onImageContextMenu);
    connect(m_imagesTree, &QTreeWidget::itemClicked,
            this, &DockerToolWidget::onImageSelected);
    connect(m_pullImageBtn, &QPushButton::clicked, this, &DockerToolWidget::onPullImageClicked);
    connect(m_buildImageBtn, &QPushButton::clicked, this, &DockerToolWidget::onBuildImageClicked);
    connect(m_removeImageBtn, &QPushButton::clicked, [this]() {
        auto item = m_imagesTree->currentItem();
        if (item) removeImage(item->data(0, Qt::UserRole).toString());
    });
    
    // Volumes
    connect(m_volumesTree, &QTreeWidget::customContextMenuRequested,
            this, &DockerToolWidget::onVolumeContextMenu);
    connect(m_createVolumeBtn, &QPushButton::clicked, this, &DockerToolWidget::onCreateVolumeClicked);
    connect(m_removeVolumeBtn, &QPushButton::clicked, [this]() {
        auto item = m_volumesTree->currentItem();
        if (item) removeVolume(item->text(0));
    });
    connect(m_pruneVolumesBtn, &QPushButton::clicked, this, &DockerToolWidget::pruneVolumes);
    
    // Networks
    connect(m_networksTree, &QTreeWidget::customContextMenuRequested,
            this, &DockerToolWidget::onNetworkContextMenu);
    connect(m_createNetworkBtn, &QPushButton::clicked, this, &DockerToolWidget::onCreateNetworkClicked);
    connect(m_removeNetworkBtn, &QPushButton::clicked, [this]() {
        auto item = m_networksTree->currentItem();
        if (item) removeNetwork(item->text(0));
    });
    
    // Compose
    connect(m_browseComposeBtn, &QPushButton::clicked, this, &DockerToolWidget::onLoadComposeClicked);
    connect(m_composeUpBtn, &QPushButton::clicked, this, &DockerToolWidget::onComposeUpClicked);
    connect(m_composeDownBtn, &QPushButton::clicked, this, &DockerToolWidget::onComposeDownClicked);
}

void DockerToolWidget::checkDockerStatus() {
    executeDockerCommand({"version", "--format", "{{.Server.Version}}"}, 
        [this](const QString& output) {
            if (!output.isEmpty() && !output.contains("error", Qt::CaseInsensitive)) {
                m_dockerAvailable = true;
                m_dockerVersion = output.trimmed();
                m_statusLabel->setText(tr("Docker is running"));
                m_statusLabel->setStyleSheet("color: #4caf50;");
                m_dockerVersionLabel->setText(tr("Version: %1").arg(m_dockerVersion));
                
                refreshContainers();
                refreshImages();
                refreshVolumes();
                refreshNetworks();
                
                m_autoRefreshTimer->start();
                emit dockerStatusChanged(true);
            } else {
                m_dockerAvailable = false;
                m_statusLabel->setText(tr("Docker is not running"));
                m_statusLabel->setStyleSheet("color: #f44336;");
                emit dockerStatusChanged(false);
            }
        });
}

void DockerToolWidget::executeDockerCommand(const QStringList& args,
                                            std::function<void(const QString&)> callback) {
    QProcess* process = new QProcess(this);
    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [this, process, callback](int exitCode, QProcess::ExitStatus status) {
                QString output = process->readAllStandardOutput();
                QString error = process->readAllStandardError();
                
                if (callback) {
                    callback(exitCode == 0 ? output : error);
                }
                
                if (exitCode != 0 && !error.isEmpty()) {
                    emit errorOccurred(error);
                }
                
                process->deleteLater();
            });
    
    process->start("docker", args);
}

void DockerToolWidget::refreshContainers() {
    executeDockerCommand({"ps", "-a", "--format", "{{json .}}"}, 
        [this](const QString& output) {
            parseContainersOutput(output);
        });
}

void DockerToolWidget::parseContainersOutput(const QString& output) {
    m_containers.clear();
    m_containersTree->clear();
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            
            DockerContainer container;
            container.id = obj["ID"].toString();
            container.name = obj["Names"].toString();
            container.image = obj["Image"].toString();
            container.command = obj["Command"].toString();
            container.status = obj["Status"].toString();
            container.state = obj["State"].toString();
            container.ports = obj["Ports"].toString();
            
            m_containers[container.id] = container;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(m_containersTree);
            updateContainerItem(item, container);
        }
    }
}

void DockerToolWidget::updateContainerItem(QTreeWidgetItem* item, const DockerContainer& container) {
    item->setText(0, container.name);
    item->setText(1, container.image);
    item->setText(2, container.status);
    item->setText(3, container.ports);
    item->setText(4, container.created.toString());
    item->setData(0, Qt::UserRole, container.id);
    
    // Status icon
    if (container.isRunning()) {
        item->setIcon(0, style()->standardIcon(QStyle::SP_MediaPlay));
        item->setForeground(2, QColor("#4caf50"));
    } else if (container.isPaused()) {
        item->setIcon(0, style()->standardIcon(QStyle::SP_MediaPause));
        item->setForeground(2, QColor("#ff9800"));
    } else {
        item->setIcon(0, style()->standardIcon(QStyle::SP_MediaStop));
        item->setForeground(2, QColor("#9e9e9e"));
    }
}

void DockerToolWidget::refreshImages() {
    executeDockerCommand({"images", "--format", "{{json .}}"}, 
        [this](const QString& output) {
            parseImagesOutput(output);
        });
}

void DockerToolWidget::parseImagesOutput(const QString& output) {
    m_images.clear();
    m_imagesTree->clear();
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            
            DockerImage image;
            image.id = obj["ID"].toString();
            image.repository = obj["Repository"].toString();
            image.tag = obj["Tag"].toString();
            
            // Parse size
            QString sizeStr = obj["Size"].toString();
            // Handle formats like "100MB", "1.5GB"
            
            m_images[image.id] = image;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(m_imagesTree);
            updateImageItem(item, image);
        }
    }
}

void DockerToolWidget::updateImageItem(QTreeWidgetItem* item, const DockerImage& image) {
    item->setText(0, image.repository);
    item->setText(1, image.tag);
    item->setText(2, image.id.left(12));
    item->setText(3, image.created.toString());
    item->setText(4, formatBytes(image.size));
    item->setData(0, Qt::UserRole, image.id);
}

void DockerToolWidget::refreshVolumes() {
    executeDockerCommand({"volume", "ls", "--format", "{{json .}}"}, 
        [this](const QString& output) {
            parseVolumesOutput(output);
        });
}

void DockerToolWidget::parseVolumesOutput(const QString& output) {
    m_volumes.clear();
    m_volumesTree->clear();
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            
            DockerVolume volume;
            volume.name = obj["Name"].toString();
            volume.driver = obj["Driver"].toString();
            volume.mountpoint = obj["Mountpoint"].toString();
            volume.scope = obj["Scope"].toString();
            
            m_volumes[volume.name] = volume;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(m_volumesTree);
            updateVolumeItem(item, volume);
        }
    }
}

void DockerToolWidget::updateVolumeItem(QTreeWidgetItem* item, const DockerVolume& volume) {
    item->setText(0, volume.name);
    item->setText(1, volume.driver);
    item->setText(2, volume.mountpoint);
    item->setText(3, volume.scope);
}

void DockerToolWidget::refreshNetworks() {
    executeDockerCommand({"network", "ls", "--format", "{{json .}}"}, 
        [this](const QString& output) {
            parseNetworksOutput(output);
        });
}

void DockerToolWidget::parseNetworksOutput(const QString& output) {
    m_networks.clear();
    m_networksTree->clear();
    
    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QJsonDocument doc = QJsonDocument::fromJson(line.toUtf8());
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            
            DockerNetwork network;
            network.id = obj["ID"].toString();
            network.name = obj["Name"].toString();
            network.driver = obj["Driver"].toString();
            network.scope = obj["Scope"].toString();
            
            m_networks[network.id] = network;
            
            QTreeWidgetItem* item = new QTreeWidgetItem(m_networksTree);
            updateNetworkItem(item, network);
        }
    }
}

void DockerToolWidget::updateNetworkItem(QTreeWidgetItem* item, const DockerNetwork& network) {
    item->setText(0, network.name);
    item->setText(1, network.id.left(12));
    item->setText(2, network.driver);
    item->setText(3, network.scope);
    item->setText(4, network.subnet);
    item->setText(5, network.gateway);
}

// Container operations
void DockerToolWidget::startContainer(const QString& containerId) {
    executeDockerCommand({"start", containerId}, [this, containerId](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
        emit containerStarted(containerId);
        emit operationCompleted(tr("Container started"));
    });
}

void DockerToolWidget::stopContainer(const QString& containerId) {
    executeDockerCommand({"stop", containerId}, [this, containerId](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
        emit containerStopped(containerId);
        emit operationCompleted(tr("Container stopped"));
    });
}

void DockerToolWidget::restartContainer(const QString& containerId) {
    executeDockerCommand({"restart", containerId}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
        emit operationCompleted(tr("Container restarted"));
    });
}

void DockerToolWidget::pauseContainer(const QString& containerId) {
    executeDockerCommand({"pause", containerId}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
    });
}

void DockerToolWidget::unpauseContainer(const QString& containerId) {
    executeDockerCommand({"unpause", containerId}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
    });
}

void DockerToolWidget::removeContainer(const QString& containerId, bool force) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Remove Container"),
        tr("Are you sure you want to remove this container?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    QStringList args = {"rm"};
    if (force) args << "-f";
    args << containerId;
    
    executeDockerCommand(args, [this, containerId](const QString& output) {
        Q_UNUSED(output);
        refreshContainers();
        emit containerRemoved(containerId);
        emit operationCompleted(tr("Container removed"));
    });
}

void DockerToolWidget::execInContainer(const QString& containerId, const QString& command) {
    executeDockerCommand({"exec", "-it", containerId, "sh", "-c", command}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
        });
}

void DockerToolWidget::attachToContainer(const QString& containerId) {
    // This would typically open a terminal widget
    executeDockerCommand({"attach", containerId}, nullptr);
}

// Image operations
void DockerToolWidget::pullImage(const QString& imageName) {
    m_statusLabel->setText(tr("Pulling %1...").arg(imageName));
    
    executeDockerCommand({"pull", imageName}, [this, imageName](const QString& output) {
        Q_UNUSED(output);
        refreshImages();
        m_statusLabel->setText(tr("Docker is running"));
        emit imagePulled(imageName);
        emit operationCompleted(tr("Image pulled: %1").arg(imageName));
    });
}

void DockerToolWidget::pushImage(const QString& imageName) {
    executeDockerCommand({"push", imageName}, [this, imageName](const QString& output) {
        Q_UNUSED(output);
        emit imagePushed(imageName);
        emit operationCompleted(tr("Image pushed: %1").arg(imageName));
    });
}

void DockerToolWidget::removeImage(const QString& imageId, bool force) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Remove Image"),
        tr("Are you sure you want to remove this image?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    QStringList args = {"rmi"};
    if (force) args << "-f";
    args << imageId;
    
    executeDockerCommand(args, [this](const QString& output) {
        Q_UNUSED(output);
        refreshImages();
        emit operationCompleted(tr("Image removed"));
    });
}

void DockerToolWidget::tagImage(const QString& imageId, const QString& newTag) {
    executeDockerCommand({"tag", imageId, newTag}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshImages();
        emit operationCompleted(tr("Image tagged"));
    });
}

void DockerToolWidget::buildImage(const QString& dockerfilePath, const QString& tag) {
    QFileInfo fi(dockerfilePath);
    QString context = fi.absolutePath();
    
    m_statusLabel->setText(tr("Building image..."));
    
    QStringList args = {"build", "-t", tag, "-f", dockerfilePath, context};
    executeDockerCommand(args, [this, tag](const QString& output) {
        m_composeOutput->append(output);
        refreshImages();
        m_statusLabel->setText(tr("Docker is running"));
        emit imageBuilt(tag);
        emit operationCompleted(tr("Image built: %1").arg(tag));
    });
}

void DockerToolWidget::inspectImage(const QString& imageId) {
    executeDockerCommand({"inspect", imageId}, [this](const QString& output) {
        QMessageBox::information(this, tr("Image Details"), output);
    });
}

// Volume operations
void DockerToolWidget::createVolume(const QString& name, const QString& driver) {
    QStringList args = {"volume", "create"};
    if (!driver.isEmpty() && driver != "local") {
        args << "--driver" << driver;
    }
    args << name;
    
    executeDockerCommand(args, [this](const QString& output) {
        Q_UNUSED(output);
        refreshVolumes();
        emit operationCompleted(tr("Volume created"));
    });
}

void DockerToolWidget::removeVolume(const QString& name) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Remove Volume"),
        tr("Are you sure you want to remove volume '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    executeDockerCommand({"volume", "rm", name}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshVolumes();
        emit operationCompleted(tr("Volume removed"));
    });
}

void DockerToolWidget::pruneVolumes() {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Prune Volumes"),
        tr("This will remove all unused volumes. Continue?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    executeDockerCommand({"volume", "prune", "-f"}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshVolumes();
        emit operationCompleted(tr("Unused volumes pruned"));
    });
}

// Network operations
void DockerToolWidget::createNetwork(const QString& name, const QString& driver) {
    QStringList args = {"network", "create"};
    if (!driver.isEmpty() && driver != "bridge") {
        args << "--driver" << driver;
    }
    args << name;
    
    executeDockerCommand(args, [this](const QString& output) {
        Q_UNUSED(output);
        refreshNetworks();
        emit operationCompleted(tr("Network created"));
    });
}

void DockerToolWidget::removeNetwork(const QString& name) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, tr("Remove Network"),
        tr("Are you sure you want to remove network '%1'?").arg(name),
        QMessageBox::Yes | QMessageBox::No);
    
    if (reply != QMessageBox::Yes) return;
    
    executeDockerCommand({"network", "rm", name}, [this](const QString& output) {
        Q_UNUSED(output);
        refreshNetworks();
        emit operationCompleted(tr("Network removed"));
    });
}

void DockerToolWidget::inspectNetwork(const QString& name) {
    executeDockerCommand({"network", "inspect", name}, [this](const QString& output) {
        QMessageBox::information(this, tr("Network Details"), output);
    });
}

// Compose operations
void DockerToolWidget::loadComposeFile(const QString& filePath) {
    m_composePathEdit->setText(filePath);
    // Parse compose file and populate tree
    // This would require YAML parsing
}

void DockerToolWidget::composeUp(const QString& projectPath) {
    QFileInfo fi(projectPath);
    
    executeDockerCommand({"compose", "-f", projectPath, "up", "-d"}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
            refreshContainers();
            emit operationCompleted(tr("Compose up completed"));
        });
}

void DockerToolWidget::composeDown(const QString& projectPath) {
    executeDockerCommand({"compose", "-f", projectPath, "down"}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
            refreshContainers();
            emit operationCompleted(tr("Compose down completed"));
        });
}

void DockerToolWidget::composeStart(const QString& projectPath) {
    executeDockerCommand({"compose", "-f", projectPath, "start"}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
            refreshContainers();
        });
}

void DockerToolWidget::composeStop(const QString& projectPath) {
    executeDockerCommand({"compose", "-f", projectPath, "stop"}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
            refreshContainers();
        });
}

void DockerToolWidget::composeRestart(const QString& projectPath) {
    executeDockerCommand({"compose", "-f", projectPath, "restart"}, 
        [this](const QString& output) {
            m_composeOutput->append(output);
            refreshContainers();
        });
}

void DockerToolWidget::composeLogs(const QString& projectPath, const QString& service) {
    QStringList args = {"compose", "-f", projectPath, "logs"};
    if (!service.isEmpty()) {
        args << service;
    }
    
    executeDockerCommand(args, [this](const QString& output) {
        m_composeOutput->append(output);
    });
}

// Slot implementations
void DockerToolWidget::onRefreshClicked() {
    checkDockerStatus();
}

void DockerToolWidget::onContainerContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_containersTree->itemAt(pos);
    if (!item) return;
    
    QString containerId = item->data(0, Qt::UserRole).toString();
    DockerContainer* container = m_containers.contains(containerId) ? 
                                 &m_containers[containerId] : nullptr;
    
    QMenu menu;
    
    if (container && container->isRunning()) {
        menu.addAction(tr("Stop"), [this, containerId]() { stopContainer(containerId); });
        menu.addAction(tr("Restart"), [this, containerId]() { restartContainer(containerId); });
        menu.addAction(tr("Pause"), [this, containerId]() { pauseContainer(containerId); });
    } else if (container && container->isPaused()) {
        menu.addAction(tr("Unpause"), [this, containerId]() { unpauseContainer(containerId); });
    } else {
        menu.addAction(tr("Start"), [this, containerId]() { startContainer(containerId); });
    }
    
    menu.addSeparator();
    menu.addAction(tr("Remove"), [this, containerId]() { removeContainer(containerId); });
    menu.addAction(tr("Remove (Force)"), [this, containerId]() { removeContainer(containerId, true); });
    
    menu.exec(m_containersTree->mapToGlobal(pos));
}

void DockerToolWidget::onImageContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_imagesTree->itemAt(pos);
    if (!item) return;
    
    QString imageId = item->data(0, Qt::UserRole).toString();
    
    QMenu menu;
    menu.addAction(tr("Inspect"), [this, imageId]() { inspectImage(imageId); });
    menu.addAction(tr("Tag..."), [this, imageId]() {
        QString tag = QInputDialog::getText(this, tr("Tag Image"), tr("New tag:"));
        if (!tag.isEmpty()) {
            tagImage(imageId, tag);
        }
    });
    menu.addSeparator();
    menu.addAction(tr("Remove"), [this, imageId]() { removeImage(imageId); });
    menu.addAction(tr("Remove (Force)"), [this, imageId]() { removeImage(imageId, true); });
    
    menu.exec(m_imagesTree->mapToGlobal(pos));
}

void DockerToolWidget::onVolumeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_volumesTree->itemAt(pos);
    if (!item) return;
    
    QString volumeName = item->text(0);
    
    QMenu menu;
    menu.addAction(tr("Remove"), [this, volumeName]() { removeVolume(volumeName); });
    
    menu.exec(m_volumesTree->mapToGlobal(pos));
}

void DockerToolWidget::onNetworkContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = m_networksTree->itemAt(pos);
    if (!item) return;
    
    QString networkName = item->text(0);
    
    QMenu menu;
    menu.addAction(tr("Inspect"), [this, networkName]() { inspectNetwork(networkName); });
    menu.addSeparator();
    menu.addAction(tr("Remove"), [this, networkName]() { removeNetwork(networkName); });
    
    menu.exec(m_networksTree->mapToGlobal(pos));
}

void DockerToolWidget::onContainerSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    
    if (!item) {
        m_startContainerBtn->setEnabled(false);
        m_stopContainerBtn->setEnabled(false);
        m_restartContainerBtn->setEnabled(false);
        m_removeContainerBtn->setEnabled(false);
        return;
    }
    
    QString containerId = item->data(0, Qt::UserRole).toString();
    DockerContainer* container = m_containers.contains(containerId) ? 
                                 &m_containers[containerId] : nullptr;
    
    m_removeContainerBtn->setEnabled(true);
    
    if (container) {
        m_startContainerBtn->setEnabled(!container->isRunning());
        m_stopContainerBtn->setEnabled(container->isRunning());
        m_restartContainerBtn->setEnabled(container->isRunning());
        
        m_logsWidget->setContainer(containerId);
        m_statsWidget->setContainer(containerId);
    }
}

void DockerToolWidget::onImageSelected(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column);
    m_removeImageBtn->setEnabled(item != nullptr);
}

void DockerToolWidget::onPullImageClicked() {
    QString imageName = m_pullImageEdit->text().trimmed();
    if (imageName.isEmpty()) {
        QMessageBox::warning(this, tr("Pull Image"), tr("Please enter an image name"));
        return;
    }
    pullImage(imageName);
}

void DockerToolWidget::onBuildImageClicked() {
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Dockerfile"), QString(),
        tr("Dockerfile (Dockerfile*);;All Files (*.*)"));
    
    if (filePath.isEmpty()) return;
    
    QString tag = QInputDialog::getText(this, tr("Build Image"), 
                                        tr("Image tag (e.g., myapp:latest):"));
    if (tag.isEmpty()) return;
    
    buildImage(filePath, tag);
}

void DockerToolWidget::onCreateVolumeClicked() {
    QString name = QInputDialog::getText(this, tr("Create Volume"), tr("Volume name:"));
    if (name.isEmpty()) return;
    
    createVolume(name);
}

void DockerToolWidget::onCreateNetworkClicked() {
    QString name = QInputDialog::getText(this, tr("Create Network"), tr("Network name:"));
    if (name.isEmpty()) return;
    
    createNetwork(name);
}

void DockerToolWidget::onLoadComposeClicked() {
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select Compose File"), QString(),
        tr("Docker Compose (docker-compose.yml *.yml *.yaml);;All Files (*.*)"));
    
    if (!filePath.isEmpty()) {
        loadComposeFile(filePath);
    }
}

void DockerToolWidget::onComposeUpClicked() {
    QString path = m_composePathEdit->text();
    if (path.isEmpty()) {
        QMessageBox::warning(this, tr("Compose"), tr("Please select a compose file first"));
        return;
    }
    composeUp(path);
}

void DockerToolWidget::onComposeDownClicked() {
    QString path = m_composePathEdit->text();
    if (path.isEmpty()) return;
    composeDown(path);
}

void DockerToolWidget::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(exitCode);
    Q_UNUSED(status);
}

void DockerToolWidget::onProcessError(QProcess::ProcessError error) {
    Q_UNUSED(error);
}

void DockerToolWidget::onAutoRefresh() {
    if (m_dockerAvailable) {
        refreshContainers();
    }
}

QString DockerToolWidget::formatBytes(qint64 bytes) const {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int unit = 0;
    double size = bytes;
    
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        unit++;
    }
    
    return QString("%1 %2").arg(size, 0, 'f', unit > 0 ? 1 : 0).arg(units[unit]);
}

QString DockerToolWidget::formatDuration(qint64 seconds) const {
    if (seconds < 60) return tr("%1 seconds").arg(seconds);
    if (seconds < 3600) return tr("%1 minutes").arg(seconds / 60);
    if (seconds < 86400) return tr("%1 hours").arg(seconds / 3600);
    return tr("%1 days").arg(seconds / 86400);
}
