/**
 * @file discovery_dashboard.h
 * @brief Enterprise Discovery Dashboard - Visual capability display system
 * 
 * Provides comprehensive visual capability display including:
 * - System capability detection and visualization
 * - Feature availability mapping
 * - Hardware capability assessment
 * - Service discovery and health monitoring
 * - Real-time capability updates
 * 
 * Production-ready with full observability, error handling, and adaptive behavior.
 * 
 * @author RawrXD Agentic IDE
 * @version 1.0.0
 */

#pragma once

#include <QWidget>
#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <QReadWriteLock>
#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QTableWidget;
class QProgressBar;
class QLabel;
class QGroupBox;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QChartView;
class QChart;
class QPieSeries;
class QBarSeries;

namespace QtCharts {
    class QChart;
    class QChartView;
    class QPieSeries;
    class QBarSeries;
}

/**
 * @enum CapabilityCategory
 * @brief Categories of system capabilities
 */
enum class CapabilityCategory {
    Hardware,       ///< CPU, GPU, Memory, Storage
    Software,       ///< Compilers, Runtime, Libraries
    AI_Models,      ///< Local models, cloud providers
    Network,        ///< Connectivity, endpoints
    Storage,        ///< File systems, databases
    Security,       ///< Authentication, encryption
    Integration,    ///< APIs, plugins, extensions
    Performance     ///< Benchmarks, metrics
};

/**
 * @enum CapabilityStatus
 * @brief Status of a detected capability
 */
enum class CapabilityStatus {
    Available,      ///< Fully operational
    Limited,        ///< Operational with restrictions
    Degraded,       ///< Operational but impaired
    Unavailable,    ///< Not available
    Unknown,        ///< Status not determined
    Checking        ///< Currently being evaluated
};

/**
 * @struct CapabilityInfo
 * @brief Information about a detected capability
 */
struct CapabilityInfo {
    QString id;                         ///< Unique identifier
    QString name;                       ///< Human-readable name
    QString description;                ///< Detailed description
    CapabilityCategory category;        ///< Category classification
    CapabilityStatus status;            ///< Current status
    QString version;                    ///< Version if applicable
    QJsonObject details;                ///< Additional details
    QDateTime lastChecked;              ///< Last verification time
    double healthScore;                 ///< 0.0 to 1.0 health metric
    QStringList dependencies;           ///< Required capabilities
    QStringList tags;                   ///< Searchable tags
};

/**
 * @struct ServiceEndpoint
 * @brief Information about a discovered service endpoint
 */
struct ServiceEndpoint {
    QString name;
    QString url;
    QString protocol;
    CapabilityStatus status;
    double latencyMs;
    QDateTime lastChecked;
    QJsonObject metadata;
};

/**
 * @struct HardwareInfo
 * @brief Detailed hardware capability information
 */
struct HardwareInfo {
    // CPU Information
    QString cpuModel;
    int cpuCores;
    int cpuThreads;
    double cpuFrequencyGHz;
    bool hasAVX;
    bool hasAVX2;
    bool hasAVX512;
    bool hasSSE42;
    bool hasNEON;
    
    // GPU Information
    QString gpuModel;
    QString gpuVendor;
    size_t gpuMemoryMB;
    bool hasCUDA;
    bool hasOpenCL;
    bool hasVulkan;
    bool hasMetal;
    int cudaComputeCapability;
    
    // Memory Information
    size_t totalMemoryMB;
    size_t availableMemoryMB;
    size_t totalSwapMB;
    
    // Storage Information
    size_t totalStorageGB;
    size_t availableStorageGB;
    bool hasSSD;
    bool hasNVMe;
};

/**
 * @class CapabilityScanner
 * @brief Background scanner for capability detection
 */
class CapabilityScanner : public QObject {
    Q_OBJECT
    
public:
    explicit CapabilityScanner(QObject* parent = nullptr);
    ~CapabilityScanner() override;
    
    void startScan();
    void stopScan();
    bool isScanning() const;
    
signals:
    void capabilityDetected(const CapabilityInfo& capability);
    void scanProgress(int current, int total, const QString& currentItem);
    void scanCompleted(int totalCapabilities);
    void scanError(const QString& error);
    void hardwareInfoUpdated(const HardwareInfo& info);
    
private:
    void scanHardware();
    void scanSoftware();
    void scanAIModels();
    void scanNetwork();
    void scanStorage();
    void scanSecurity();
    void scanIntegrations();
    
    std::atomic<bool> m_scanning{false};
    QMutex m_scanMutex;
};

/**
 * @class ServiceDiscovery
 * @brief Service discovery and health monitoring
 */
class ServiceDiscovery : public QObject {
    Q_OBJECT
    
public:
    explicit ServiceDiscovery(QObject* parent = nullptr);
    ~ServiceDiscovery() override;
    
    void startDiscovery();
    void stopDiscovery();
    void addEndpoint(const QString& name, const QString& url);
    void removeEndpoint(const QString& name);
    QVector<ServiceEndpoint> getEndpoints() const;
    
    void setHealthCheckInterval(int intervalMs);
    int getHealthCheckInterval() const;
    
signals:
    void endpointDiscovered(const ServiceEndpoint& endpoint);
    void endpointStatusChanged(const QString& name, CapabilityStatus newStatus);
    void healthCheckCompleted(const QVector<ServiceEndpoint>& endpoints);
    void discoveryError(const QString& error);
    
private slots:
    void performHealthChecks();
    
private:
    QVector<ServiceEndpoint> m_endpoints;
    QTimer* m_healthCheckTimer;
    int m_healthCheckInterval = 30000; // 30 seconds
    mutable QReadWriteLock m_endpointLock;
};

/**
 * @class DiscoveryDashboard
 * @brief Enterprise-grade visual capability display dashboard
 * 
 * Features:
 * - Real-time capability visualization
 * - Interactive capability tree view
 * - Health status monitoring
 * - Service endpoint discovery
 * - Hardware utilization charts
 * - Feature availability matrix
 * - Dependency visualization
 * - Export capabilities report
 */
class DiscoveryDashboard : public QWidget {
    Q_OBJECT
    
public:
    /**
     * @brief Constructor
     * @param parent Parent widget
     */
    explicit DiscoveryDashboard(QWidget* parent = nullptr);
    
    /**
     * @brief Destructor
     */
    ~DiscoveryDashboard() override;
    
    /**
     * @brief Initialize dashboard (two-phase init)
     */
    void initialize();
    
    /**
     * @brief Check if initialized
     */
    bool isInitialized() const { return m_initialized; }
    
    /**
     * @brief Start capability scanning
     */
    void startScan();
    
    /**
     * @brief Stop capability scanning
     */
    void stopScan();
    
    /**
     * @brief Get all detected capabilities
     */
    QVector<CapabilityInfo> getCapabilities() const;
    
    /**
     * @brief Get capabilities by category
     */
    QVector<CapabilityInfo> getCapabilitiesByCategory(CapabilityCategory category) const;
    
    /**
     * @brief Get hardware information
     */
    HardwareInfo getHardwareInfo() const;
    
    /**
     * @brief Get capability by ID
     */
    CapabilityInfo getCapability(const QString& id) const;
    
    /**
     * @brief Check if capability is available
     */
    bool isCapabilityAvailable(const QString& id) const;
    
    /**
     * @brief Get system health score (0.0 - 1.0)
     */
    double getSystemHealthScore() const;
    
    /**
     * @brief Export capabilities report
     */
    bool exportReport(const QString& filePath, const QString& format = "json");
    
    /**
     * @brief Set auto-refresh interval
     */
    void setRefreshInterval(int intervalMs);
    
    /**
     * @brief Enable/disable auto-refresh
     */
    void setAutoRefresh(bool enabled);
    
public slots:
    /**
     * @brief Refresh all capability data
     */
    void refresh();
    
    /**
     * @brief Clear all cached capability data
     */
    void clearCache();
    
    /**
     * @brief Filter capabilities by search query
     */
    void filterCapabilities(const QString& query);
    
signals:
    /**
     * @brief Emitted when capability scan completes
     */
    void scanCompleted(int totalCapabilities);
    
    /**
     * @brief Emitted when a capability status changes
     */
    void capabilityStatusChanged(const QString& capabilityId, CapabilityStatus newStatus);
    
    /**
     * @brief Emitted when system health score changes significantly
     */
    void systemHealthChanged(double newScore);
    
    /**
     * @brief Emitted when new capability is discovered
     */
    void capabilityDiscovered(const CapabilityInfo& capability);
    
    /**
     * @brief Emitted when hardware info updates
     */
    void hardwareInfoUpdated(const HardwareInfo& info);
    
    /**
     * @brief Emitted on error
     */
    void errorOccurred(const QString& error);
    
private slots:
    void onCapabilityDetected(const CapabilityInfo& capability);
    void onScanProgress(int current, int total, const QString& currentItem);
    void onScanCompleted(int totalCapabilities);
    void onHardwareInfoUpdated(const HardwareInfo& info);
    void onEndpointStatusChanged(const QString& name, CapabilityStatus newStatus);
    void onAutoRefreshTriggered();
    
private:
    void setupUI();
    void setupCapabilityTree();
    void setupHardwarePanel();
    void setupServicesPanel();
    void setupHealthPanel();
    void setupFeatureMatrix();
    void setupConnections();
    
    void updateCapabilityTree();
    void updateHardwarePanel();
    void updateServicesTable();
    void updateHealthCharts();
    void updateFeatureMatrix();
    void updateSummaryLabels();
    
    QTreeWidgetItem* getCategoryItem(CapabilityCategory category);
    QString categoryToString(CapabilityCategory category) const;
    QString statusToString(CapabilityStatus status) const;
    QColor statusToColor(CapabilityStatus status) const;
    QString statusToIcon(CapabilityStatus status) const;
    
    void logInfo(const QString& message);
    void logWarning(const QString& message);
    void logError(const QString& error);
    
    // UI Components
    QTabWidget* m_tabWidget = nullptr;
    
    // Overview Tab
    QLabel* m_totalCapabilitiesLabel = nullptr;
    QLabel* m_availableCapabilitiesLabel = nullptr;
    QLabel* m_healthScoreLabel = nullptr;
    QLabel* m_lastScanLabel = nullptr;
    QProgressBar* m_healthProgressBar = nullptr;
    QProgressBar* m_scanProgressBar = nullptr;
    
    // Capability Tree Tab
    QTreeWidget* m_capabilityTree = nullptr;
    QMap<CapabilityCategory, QTreeWidgetItem*> m_categoryItems;
    
    // Hardware Tab
    QLabel* m_cpuLabel = nullptr;
    QLabel* m_gpuLabel = nullptr;
    QLabel* m_memoryLabel = nullptr;
    QLabel* m_storageLabel = nullptr;
    QProgressBar* m_cpuProgressBar = nullptr;
    QProgressBar* m_gpuProgressBar = nullptr;
    QProgressBar* m_memoryProgressBar = nullptr;
    QProgressBar* m_storageProgressBar = nullptr;
    QTableWidget* m_cpuFeaturesTable = nullptr;
    QTableWidget* m_gpuFeaturesTable = nullptr;
    
    // Services Tab
    QTableWidget* m_servicesTable = nullptr;
    QPushButton* m_addServiceButton = nullptr;
    QPushButton* m_checkHealthButton = nullptr;
    
    // Health Tab
    QtCharts::QChartView* m_healthChartView = nullptr;
    QtCharts::QChart* m_healthChart = nullptr;
    QtCharts::QPieSeries* m_healthPieSeries = nullptr;
    QTableWidget* m_healthDetailsTable = nullptr;
    
    // Feature Matrix Tab
    QTableWidget* m_featureMatrixTable = nullptr;
    
    // Action Buttons
    QPushButton* m_scanButton = nullptr;
    QPushButton* m_refreshButton = nullptr;
    QPushButton* m_exportButton = nullptr;
    
    // Backend Components
    std::unique_ptr<CapabilityScanner> m_scanner;
    std::unique_ptr<ServiceDiscovery> m_serviceDiscovery;
    
    // Data Storage
    QMap<QString, CapabilityInfo> m_capabilities;
    HardwareInfo m_hardwareInfo;
    mutable QReadWriteLock m_dataLock;
    
    // Configuration
    QTimer* m_autoRefreshTimer = nullptr;
    int m_refreshInterval = 60000; // 1 minute
    bool m_autoRefreshEnabled = true;
    bool m_initialized = false;
    
    // Metrics
    double m_systemHealthScore = 1.0;
    QDateTime m_lastScanTime;
    int m_totalScans = 0;
    QVector<double> m_healthHistory;
};
