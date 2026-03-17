/**
 * @file docker_tool_widget.h
 * @brief Docker container management, image browser, compose file support
 * 
 * Production-ready Docker management widget providing:
 * - Container lifecycle management (start, stop, restart, remove)
 * - Image browser with pull, push, remove, tag operations
 * - Docker Compose file support with service management
 * - Container logs viewer with filtering
 * - Container stats monitoring (CPU, memory, network)
 * - Volume and network management
 * - Container exec/attach functionality
 * - Dockerfile building support
 */

#ifndef DOCKER_TOOL_WIDGET_H
#define DOCKER_TOOL_WIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QLabel>
#include <QSplitter>
#include <QGroupBox>
#include <QProcess>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QMenu>
#include <QProgressBar>

/**
 * @struct DockerContainer
 * @brief Represents a Docker container
 */
struct DockerContainer {
    QString id;
    QString name;
    QString image;
    QString command;
    QString status;
    QString state;  // running, exited, paused, etc.
    QString ports;
    QDateTime created;
    QStringList networks;
    QStringList volumes;
    QMap<QString, QString> labels;
    
    // Runtime stats
    double cpuPercent = 0.0;
    qint64 memoryUsage = 0;
    qint64 memoryLimit = 0;
    qint64 networkRx = 0;
    qint64 networkTx = 0;
    
    bool isRunning() const { return state == "running"; }
    bool isPaused() const { return state == "paused"; }
};

/**
 * @struct DockerImage
 * @brief Represents a Docker image
 */
struct DockerImage {
    QString id;
    QString repository;
    QString tag;
    QDateTime created;
    qint64 size;
    QString digest;
    QStringList repoTags;
    bool isDangling = false;
};

/**
 * @struct DockerVolume
 * @brief Represents a Docker volume
 */
struct DockerVolume {
    QString name;
    QString driver;
    QString mountpoint;
    QDateTime created;
    QMap<QString, QString> labels;
    QString scope;
};

/**
 * @struct DockerNetwork
 * @brief Represents a Docker network
 */
struct DockerNetwork {
    QString id;
    QString name;
    QString driver;
    QString scope;
    bool internal = false;
    QString subnet;
    QString gateway;
};

/**
 * @struct ComposeService
 * @brief Represents a Docker Compose service
 */
struct ComposeService {
    QString name;
    QString image;
    QString containerName;
    QString status;
    QStringList ports;
    QStringList volumes;
    QStringList environment;
    QStringList depends;
    int replicas = 1;
};

/**
 * @struct ComposeProject
 * @brief Represents a Docker Compose project
 */
struct ComposeProject {
    QString name;
    QString configPath;
    QString workingDir;
    QList<ComposeService> services;
    QString status;  // running, stopped, partial
};

/**
 * @class ContainerStatsWidget
 * @brief Widget for displaying real-time container statistics
 */
class ContainerStatsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ContainerStatsWidget(QWidget* parent = nullptr);
    
    void setContainer(const QString& containerId);
    void startMonitoring();
    void stopMonitoring();

signals:
    void statsUpdated(const DockerContainer& container);

private slots:
    void onStatsReceived();

private:
    void updateDisplay(const DockerContainer& container);
    
    QString m_containerId;
    QProcess* m_statsProcess;
    QTimer* m_updateTimer;
    
    QLabel* m_containerNameLabel;
    QProgressBar* m_cpuBar;
    QProgressBar* m_memoryBar;
    QLabel* m_cpuLabel;
    QLabel* m_memoryLabel;
    QLabel* m_networkLabel;
    QLabel* m_blockIOLabel;
};

/**
 * @class ContainerLogsWidget
 * @brief Widget for viewing container logs
 */
class ContainerLogsWidget : public QWidget {
    Q_OBJECT

public:
    explicit ContainerLogsWidget(QWidget* parent = nullptr);
    
    void setContainer(const QString& containerId);
    void setFollow(bool follow);
    void setTailLines(int lines);
    void clearLogs();
    void refresh();

signals:
    void logReceived(const QString& log);

private slots:
    void onLogsReceived();
    void onFilterChanged(const QString& filter);
    void onFollowToggled(bool checked);
    void onTimestampsToggled(bool checked);

private:
    QString m_containerId;
    QProcess* m_logsProcess;
    bool m_follow = false;
    int m_tailLines = 100;
    bool m_showTimestamps = true;
    
    QTextEdit* m_logsView;
    QLineEdit* m_filterEdit;
    QPushButton* m_followBtn;
    QPushButton* m_timestampsBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_refreshBtn;
    QComboBox* m_tailCombo;
};

/**
 * @class DockerToolWidget
 * @brief Main Docker management widget
 */
class DockerToolWidget : public QWidget {
    Q_OBJECT

public:
    explicit DockerToolWidget(QWidget* parent = nullptr);
    ~DockerToolWidget();
    
    // Docker daemon
    bool isDockerAvailable() const { return m_dockerAvailable; }
    QString dockerVersion() const { return m_dockerVersion; }
    void checkDockerStatus();
    
    // Container operations
    void refreshContainers();
    void startContainer(const QString& containerId);
    void stopContainer(const QString& containerId);
    void restartContainer(const QString& containerId);
    void pauseContainer(const QString& containerId);
    void unpauseContainer(const QString& containerId);
    void removeContainer(const QString& containerId, bool force = false);
    void execInContainer(const QString& containerId, const QString& command);
    void attachToContainer(const QString& containerId);
    
    // Image operations
    void refreshImages();
    void pullImage(const QString& imageName);
    void pushImage(const QString& imageName);
    void removeImage(const QString& imageId, bool force = false);
    void tagImage(const QString& imageId, const QString& newTag);
    void buildImage(const QString& dockerfilePath, const QString& tag);
    void inspectImage(const QString& imageId);
    
    // Volume operations
    void refreshVolumes();
    void createVolume(const QString& name, const QString& driver = "local");
    void removeVolume(const QString& name);
    void pruneVolumes();
    
    // Network operations
    void refreshNetworks();
    void createNetwork(const QString& name, const QString& driver = "bridge");
    void removeNetwork(const QString& name);
    void inspectNetwork(const QString& name);
    
    // Compose operations
    void loadComposeFile(const QString& filePath);
    void composeUp(const QString& projectPath);
    void composeDown(const QString& projectPath);
    void composeStart(const QString& projectPath);
    void composeStop(const QString& projectPath);
    void composeRestart(const QString& projectPath);
    void composeLogs(const QString& projectPath, const QString& service = QString());

signals:
    void dockerStatusChanged(bool available);
    void containerStarted(const QString& containerId);
    void containerStopped(const QString& containerId);
    void containerRemoved(const QString& containerId);
    void imagePulled(const QString& imageName);
    void imagePushed(const QString& imageName);
    void imageBuilt(const QString& imageName);
    void errorOccurred(const QString& error);
    void operationCompleted(const QString& operation);

private slots:
    void onRefreshClicked();
    void onContainerContextMenu(const QPoint& pos);
    void onImageContextMenu(const QPoint& pos);
    void onVolumeContextMenu(const QPoint& pos);
    void onNetworkContextMenu(const QPoint& pos);
    void onContainerSelected(QTreeWidgetItem* item, int column);
    void onImageSelected(QTreeWidgetItem* item, int column);
    void onPullImageClicked();
    void onBuildImageClicked();
    void onCreateVolumeClicked();
    void onCreateNetworkClicked();
    void onLoadComposeClicked();
    void onComposeUpClicked();
    void onComposeDownClicked();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessError(QProcess::ProcessError error);
    void onAutoRefresh();

private:
    void setupUI();
    void setupContainersTab();
    void setupImagesTab();
    void setupVolumesTab();
    void setupNetworksTab();
    void setupComposeTab();
    void setupConnections();
    
    void executeDockerCommand(const QStringList& args, 
                              std::function<void(const QString&)> callback = nullptr);
    void parseContainersOutput(const QString& output);
    void parseImagesOutput(const QString& output);
    void parseVolumesOutput(const QString& output);
    void parseNetworksOutput(const QString& output);
    
    void updateContainerItem(QTreeWidgetItem* item, const DockerContainer& container);
    void updateImageItem(QTreeWidgetItem* item, const DockerImage& image);
    void updateVolumeItem(QTreeWidgetItem* item, const DockerVolume& volume);
    void updateNetworkItem(QTreeWidgetItem* item, const DockerNetwork& network);
    
    QString formatBytes(qint64 bytes) const;
    QString formatDuration(qint64 seconds) const;
    
    // UI Components
    QTabWidget* m_tabWidget;
    
    // Containers tab
    QTreeWidget* m_containersTree;
    QPushButton* m_startContainerBtn;
    QPushButton* m_stopContainerBtn;
    QPushButton* m_restartContainerBtn;
    QPushButton* m_removeContainerBtn;
    QComboBox* m_containerFilterCombo;
    ContainerLogsWidget* m_logsWidget;
    ContainerStatsWidget* m_statsWidget;
    
    // Images tab
    QTreeWidget* m_imagesTree;
    QLineEdit* m_pullImageEdit;
    QPushButton* m_pullImageBtn;
    QPushButton* m_buildImageBtn;
    QPushButton* m_removeImageBtn;
    QCheckBox* m_showDanglingCheck;
    
    // Volumes tab
    QTreeWidget* m_volumesTree;
    QPushButton* m_createVolumeBtn;
    QPushButton* m_removeVolumeBtn;
    QPushButton* m_pruneVolumesBtn;
    
    // Networks tab
    QTreeWidget* m_networksTree;
    QPushButton* m_createNetworkBtn;
    QPushButton* m_removeNetworkBtn;
    
    // Compose tab
    QTreeWidget* m_composeTree;
    QLineEdit* m_composePathEdit;
    QPushButton* m_browseComposeBtn;
    QPushButton* m_composeUpBtn;
    QPushButton* m_composeDownBtn;
    QPushButton* m_composeStartBtn;
    QPushButton* m_composeStopBtn;
    QTextEdit* m_composeOutput;
    
    // Status bar
    QLabel* m_statusLabel;
    QLabel* m_dockerVersionLabel;
    QPushButton* m_refreshBtn;
    
    // Data
    QMap<QString, DockerContainer> m_containers;
    QMap<QString, DockerImage> m_images;
    QMap<QString, DockerVolume> m_volumes;
    QMap<QString, DockerNetwork> m_networks;
    QList<ComposeProject> m_composeProjects;
    
    // State
    bool m_dockerAvailable = false;
    QString m_dockerVersion;
    QProcess* m_currentProcess = nullptr;
    QTimer* m_autoRefreshTimer;
    std::function<void(const QString&)> m_processCallback;
};

#endif // DOCKER_TOOL_WIDGET_H
