#pragma once

#include <QWidget>
#include <QListWidget>
#include <QPushButton>
#include <QTextEdit>
#include <QProgressBar>
#include <QSlider>
#include <QString>

/**
 * @brief OptimizationPanelWidget - Qt/C++ reference implementation
 * 
 * Displays performance optimizations suggested by the autonomous system.
 * Features:
 * - ListView with columns: Optimization, Expected Speedup, Memory Impact
 * - ProgressBar showing expected speedup (0-10x scale)
 * - Apply/Dismiss buttons with confirmation
 * - Memory/latency impact visualization
 * - Integration with PerformanceOptimization MASM structures
 * 
 * This is the Qt reference version—will be ported to MASM Win32 later.
 */
class OptimizationPanelWidget : public QWidget {
    Q_OBJECT

public:
    // Optimization impact levels
    enum ImpactLevel {
        ImpactMinor = 0,      // < 5% improvement
        ImpactModerate = 1,   // 5-15% improvement
        ImpactSignificant = 2, // 15-50% improvement
        ImpactMajor = 3       // > 50% improvement
    };

    // Optimization categories
    enum OptimizationType {
        TypeAlgorithmChoice = 0,      // Use faster algorithm
        TypeMemoryLayout = 1,         // Cache-friendly data layout
        TypeCompilation = 2,          // Compiler flags, optimizations
        TypeConcurrency = 3,          // Parallelization
        TypeDiskIO = 4,               // I/O optimization
        TypeNetworkIO = 5,            // Network caching, batching
        TypeGPUAcceleration = 6,      // GPU compute offload
        TypeCaching = 7               // Memoization, caching layer
    };

    struct PerformanceOptimization {
        QString id;                    // Unique ID
        QString title;                 // e.g., "Use vectorized ops in loop_X"
        QString description;           // Detailed explanation
        OptimizationType type;        // Category
        double expectedSpeedup;       // 1.2 = 20% faster, 2.5 = 150% faster
        int estimatedMemoryImpact;   // KB change (+ = more memory, - = less)
        ImpactLevel impact;           // Minor/Moderate/Significant/Major
        QString riskLevel;            // "Low", "Medium", "High"
        QString location;              // File:Line where optimization found
        QString implementationHint;   // How to implement
        bool isApplied = false;
    };

    explicit OptimizationPanelWidget(QWidget* parent = nullptr);
    ~OptimizationPanelWidget() override;

    // Optimization management
    void addOptimization(const PerformanceOptimization& opt);
    void removeOptimization(const QString& optId);
    void clearAllOptimizations();
    void markAsApplied(const QString& optId);

    // Query
    int optimizationCount() const;
    double totalPotentialSpeedup() const; // Product of all speedups
    QList<PerformanceOptimization> getAllOptimizations() const;

    // Impact color mapping
    static QColor getImpactColor(ImpactLevel impact);
    static QString getImpactString(ImpactLevel impact);
    static QString getTypeString(OptimizationType type);

signals:
    void optimizationSelected(const QString& optId);
    void applyRequested(const QString& optId);
    void dismissRequested(const QString& optId);
    void optimizationApplied(const QString& optId);
    void optimizationsCleared();

private slots:
    void onItemClicked(QListWidgetItem* item);
    void onApplyClicked();
    void onDismissClicked();
    void onDetailsClicked();
    void updateMetrics();

private:
    void setupUI();
    void refreshOptimizationList();
    QString formatOptDisplay(const PerformanceOptimization& opt) const;

    // UI Components
    QListWidget* m_optimizationList;
    QTextEdit* m_statsLabel;             // "3 optimizations: 2.4x total speedup"
    QProgressBar* m_speedupBar;       // Visual speedup indicator (0-10x)
    QProgressBar* m_memoryBar;        // Memory impact
    QTextEdit* m_speedupLabel;           // "Potential: 2.4x faster"
    QTextEdit* m_memoryImpactLabel;      // "Memory: +2 MB"
    QPushButton* m_applyButton;
    QPushButton* m_dismissButton;
    QPushButton* m_detailsButton;
    QPushButton* m_clearAllButton;
    QTextEdit* m_detailsText;          // Details panel

    // Data
    QList<PerformanceOptimization> m_optimizations;
    QString m_selectedOptId;
};
