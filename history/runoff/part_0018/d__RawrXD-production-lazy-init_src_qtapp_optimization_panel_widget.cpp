#include "optimization_panel_widget.hpp"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QTextEdit>
#include <QSplitter>
#include <cmath>

OptimizationPanelWidget::OptimizationPanelWidget(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    updateMetrics();
}

OptimizationPanelWidget::~OptimizationPanelWidget() = default;

void OptimizationPanelWidget::setupUI() {
    auto mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);

    // Title and metrics row
    auto titleLayout = new QHBoxLayout();
    auto titleLabel = new QTextEdit();
    titleLabel->setPlainText("Performance Optimizations");
    titleLabel->setStyleSheet("font-weight: bold; font-size: 12pt; border: none; background: transparent;");
    titleLabel->setReadOnly(true);
    titleLabel->setMaximumHeight(24);
    titleLayout->addWidget(titleLabel);
    
    m_statsLabel = new QTextEdit();
    m_statsLabel->setPlainText("0 optimizations");
    m_statsLabel->setStyleSheet("color: #666; border: none; background: transparent;");
    m_statsLabel->setReadOnly(true);
    m_statsLabel->setMaximumHeight(24);
    titleLayout->addStretch();
    titleLayout->addWidget(m_statsLabel);
    
    mainLayout->addLayout(titleLayout);

    // Speedup visualization
    auto speedupLayout = new QHBoxLayout();
    m_speedupLabel = new QTextEdit();
    m_speedupLabel->setPlainText("Potential: 1.0x");
    m_speedupLabel->setStyleSheet("border: none; background: transparent;");
    m_speedupLabel->setReadOnly(true);
    m_speedupLabel->setMaximumHeight(24);
    speedupLayout->addWidget(m_speedupLabel);
    m_speedupBar = new QProgressBar();
    m_speedupBar->setMaximum(1000); // 0-10x scale
    m_speedupBar->setStyleSheet(
        "QProgressBar { border: 1px solid #4CAF50; background: #eee; }"
        "QProgressBar::chunk { background: #4CAF50; }"
    );
    speedupLayout->addWidget(m_speedupBar);
    mainLayout->addLayout(speedupLayout);

    // Memory impact
    auto memLayout = new QHBoxLayout();
    m_memoryImpactLabel = new QTextEdit();
    m_memoryImpactLabel->setPlainText("Memory: ±0 KB");
    m_memoryImpactLabel->setStyleSheet("border: none; background: transparent;");
    m_memoryImpactLabel->setReadOnly(true);
    m_memoryImpactLabel->setMaximumHeight(24);
    memLayout->addWidget(m_memoryImpactLabel);
    m_memoryBar = new QProgressBar();
    m_memoryBar->setMaximum(100);
    m_memoryBar->setStyleSheet(
        "QProgressBar { border: 1px solid #FF9800; background: #eee; }"
        "QProgressBar::chunk { background: #FF9800; }"
    );
    memLayout->addWidget(m_memoryBar);
    mainLayout->addLayout(memLayout);

    // Splitter: list on left, details on right
    auto splitter = new QSplitter(Qt::Horizontal);
    
    // Optimization list
    m_optimizationList = new QListWidget();
    m_optimizationList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_optimizationList, &QListWidget::itemClicked, this, &OptimizationPanelWidget::onItemClicked);
    splitter->addWidget(m_optimizationList);
    splitter->setStretchFactor(0, 1);

    // Details panel
    m_detailsText = new QTextEdit();
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumWidth(300);
    splitter->addWidget(m_detailsText);
    splitter->setStretchFactor(1, 0);
    
    mainLayout->addWidget(splitter);

    // Button row
    auto buttonLayout = new QHBoxLayout();
    
    m_applyButton = new QPushButton("✓ Apply");
    m_applyButton->setEnabled(false);
    connect(m_applyButton, &QPushButton::clicked, this, &OptimizationPanelWidget::onApplyClicked);
    buttonLayout->addWidget(m_applyButton);

    m_dismissButton = new QPushButton("↻ Dismiss");
    m_dismissButton->setEnabled(false);
    connect(m_dismissButton, &QPushButton::clicked, this, &OptimizationPanelWidget::onDismissClicked);
    buttonLayout->addWidget(m_dismissButton);

    m_detailsButton = new QPushButton("ℹ Details");
    m_detailsButton->setEnabled(false);
    connect(m_detailsButton, &QPushButton::clicked, this, &OptimizationPanelWidget::onDetailsClicked);
    buttonLayout->addWidget(m_detailsButton);

    m_clearAllButton = new QPushButton("✕ Clear All");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_clearAllButton);
    connect(m_clearAllButton, &QPushButton::clicked, this, [this]() {
        clearAllOptimizations();
    });

    mainLayout->addLayout(buttonLayout);
    setLayout(mainLayout);
}

void OptimizationPanelWidget::addOptimization(const PerformanceOptimization& opt) {
    m_optimizations.append(opt);
    refreshOptimizationList();
    updateMetrics();
}

void OptimizationPanelWidget::removeOptimization(const QString& optId) {
    m_optimizations.erase(std::remove_if(m_optimizations.begin(), m_optimizations.end(),
        [optId](const PerformanceOptimization& o) { return o.id == optId; }), m_optimizations.end());
    refreshOptimizationList();
    updateMetrics();
}

void OptimizationPanelWidget::clearAllOptimizations() {
    m_optimizations.clear();
    m_optimizationList->clear();
    m_detailsText->clear();
    m_selectedOptId.clear();
    updateMetrics();
    emit optimizationsCleared();
}

void OptimizationPanelWidget::markAsApplied(const QString& optId) {
    for (auto& opt : m_optimizations) {
        if (opt.id == optId) {
            opt.isApplied = true;
            emit optimizationApplied(optId);
            break;
        }
    }
    refreshOptimizationList();
    updateMetrics();
}

int OptimizationPanelWidget::optimizationCount() const {
    return m_optimizations.size();
}

double OptimizationPanelWidget::totalPotentialSpeedup() const {
    double total = 1.0;
    for (const auto& opt : m_optimizations) {
        if (!opt.isApplied) {
            total *= opt.expectedSpeedup;
        }
    }
    return total;
}

QList<OptimizationPanelWidget::PerformanceOptimization> OptimizationPanelWidget::getAllOptimizations() const {
    return m_optimizations;
}

QColor OptimizationPanelWidget::getImpactColor(ImpactLevel impact) {
    switch (impact) {
        case ImpactMinor:       return QColor(255, 193, 7);    // Yellow
        case ImpactModerate:    return QColor(255, 152, 0);    // Orange
        case ImpactSignificant: return QColor(76, 175, 80);    // Green
        case ImpactMajor:       return QColor(33, 150, 243);   // Blue
        default:                return QColor(128, 128, 128);  // Gray
    }
}

QString OptimizationPanelWidget::getImpactString(ImpactLevel impact) {
    switch (impact) {
        case ImpactMinor:       return "Minor";
        case ImpactModerate:    return "Moderate";
        case ImpactSignificant: return "Significant";
        case ImpactMajor:       return "Major";
        default:                return "Unknown";
    }
}

QString OptimizationPanelWidget::getTypeString(OptimizationType type) {
    switch (type) {
        case TypeAlgorithmChoice:  return "Algorithm";
        case TypeMemoryLayout:     return "Memory Layout";
        case TypeCompilation:      return "Compilation";
        case TypeConcurrency:      return "Concurrency";
        case TypeDiskIO:           return "Disk I/O";
        case TypeNetworkIO:        return "Network I/O";
        case TypeGPUAcceleration:  return "GPU Acceleration";
        case TypeCaching:          return "Caching";
        default:                   return "Other";
    }
}

void OptimizationPanelWidget::refreshOptimizationList() {
    m_optimizationList->clear();
    
    // Sort by potential speedup (highest first)
    auto sortedOpts = m_optimizations;
    std::sort(sortedOpts.begin(), sortedOpts.end(),
        [](const PerformanceOptimization& a, const PerformanceOptimization& b) {
            return a.expectedSpeedup > b.expectedSpeedup;
        });

    for (const auto& opt : sortedOpts) {
        auto item = new QListWidgetItem();
        item->setText(formatOptDisplay(opt));
        item->setData(Qt::UserRole, opt.id);
        item->setForeground(getImpactColor(opt.impact));
        
        if (opt.isApplied) {
            item->setBackground(QColor(200, 255, 200)); // Light green
            item->setText(item->text() + " [APPLIED]");
        }
        
        m_optimizationList->addItem(item);
    }
}

QString OptimizationPanelWidget::formatOptDisplay(const PerformanceOptimization& opt) const {
    return QString("%1 (%2x) - %3")
        .arg(opt.title)
        .arg(opt.expectedSpeedup, 0, 'f', 1)
        .arg(getTypeString(opt.type));
}

void OptimizationPanelWidget::updateMetrics() {
    int total = m_optimizations.size();
    double totalSpeedup = totalPotentialSpeedup();
    
    m_statsLabel->setText(QString::number(total) + " optimizations");
    
    // Speedup bar (0-10x scale)
    int barValue = static_cast<int>(std::min(totalSpeedup, 10.0) * 100);
    m_speedupBar->setMaximum(1000); // 10x = 1000
    m_speedupBar->setValue(barValue);
    m_speedupLabel->setText(QString("Potential: %1x").arg(totalSpeedup, 0, 'f', 1));
    
    // Memory impact
    int totalMemoryImpact = 0;
    for (const auto& opt : m_optimizations) {
        if (!opt.isApplied) {
            totalMemoryImpact += opt.estimatedMemoryImpact;
        }
    }
    
    QString memStr = (totalMemoryImpact >= 0) ? "+" : "";
    memStr += QString::number(totalMemoryImpact / 1024.0, 'f', 1) + " MB";
    m_memoryImpactLabel->setText("Memory: " + memStr);
    
    // Update memory bar color
    if (totalMemoryImpact > 0) {
        m_memoryBar->setStyleSheet(
            "QProgressBar { border: 1px solid #FF9800; background: #eee; }"
            "QProgressBar::chunk { background: #FF9800; }" // Orange for increase
        );
    } else {
        m_memoryBar->setStyleSheet(
            "QProgressBar { border: 1px solid #4CAF50; background: #eee; }"
            "QProgressBar::chunk { background: #4CAF50; }" // Green for decrease
        );
    }
    
    m_memoryBar->setMaximum(100);
    m_memoryBar->setValue(std::abs(totalMemoryImpact) / 100); // Rough scale
}

void OptimizationPanelWidget::onItemClicked(QListWidgetItem* item) {
    m_selectedOptId = item->data(Qt::UserRole).toString();
    
    // Find and display optimization details
    for (const auto& opt : m_optimizations) {
        if (opt.id == m_selectedOptId) {
            QString details = QString(
                "<b>%1</b><br/>"
                "<b>Type:</b> %2<br/>"
                "<b>Expected Speedup:</b> %3x<br/>"
                "<b>Impact:</b> %4 | Risk: %5<br/>"
                "<b>Memory Change:</b> %6 KB<br/><br/>"
                "<b>Description:</b><br/>%7<br/><br/>"
                "<b>Implementation Hint:</b><br/>%8"
            ).arg(opt.title)
             .arg(getTypeString(opt.type))
             .arg(opt.expectedSpeedup, 0, 'f', 1)
             .arg(getImpactString(opt.impact))
             .arg(opt.riskLevel)
             .arg(opt.estimatedMemoryImpact)
             .arg(opt.description)
             .arg(opt.implementationHint);
            
            m_detailsText->setText(details);
            emit optimizationSelected(m_selectedOptId);
            break;
        }
    }
    
    m_applyButton->setEnabled(!m_optimizations[0].isApplied);
    m_dismissButton->setEnabled(true);
    m_detailsButton->setEnabled(true);
}

void OptimizationPanelWidget::onApplyClicked() {
    if (!m_selectedOptId.isEmpty()) {
        emit applyRequested(m_selectedOptId);
        markAsApplied(m_selectedOptId);
        QMessageBox::information(this, "Optimization Applied",
            "The optimization has been queued for implementation.\n"
            "Review the changes before committing.");
    }
}

void OptimizationPanelWidget::onDismissClicked() {
    if (!m_selectedOptId.isEmpty()) {
        emit dismissRequested(m_selectedOptId);
        removeOptimization(m_selectedOptId);
    }
}

void OptimizationPanelWidget::onDetailsClicked() {
    if (!m_selectedOptId.isEmpty()) {
        QMessageBox::information(this, "Optimization Details", m_detailsText->toPlainText(), QMessageBox::Ok);
    }
}
