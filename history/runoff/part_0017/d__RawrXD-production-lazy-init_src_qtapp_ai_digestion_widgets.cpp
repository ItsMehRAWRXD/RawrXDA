// ai_digestion_widgets.cpp - Supporting widget implementations
#include "ai_digestion_panel.hpp"
#include <QtGui/QPainter>
#include <QtGui/QPen>
#include <QtGui/QDragEnterEvent>
#include <QtGui/QDropEvent>
#include <QtCore/QMimeData>
#include <QtCore/QUrl>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QHBoxLayout>

// FileDropWidget Implementation
FileDropWidget::FileDropWidget(QWidget* parent)
    : QFrame(parent)
    , m_dragActive(false)
{
    setAcceptDrops(true);
    setFrameStyle(QFrame::Box | QFrame::Raised);
    setLineWidth(2);
    setStyleSheet(
        "FileDropWidget {"
        "    border: 2px dashed #aaa;"
        "    border-radius: 8px;"
        "    background-color: #f8f8f8;"
        "}"
        "FileDropWidget:hover {"
        "    border-color: #0078d4;"
        "    background-color: #f0f8ff;"
        "}"
    );
}

void FileDropWidget::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        m_dragActive = true;
        update();
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileDropWidget::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

void FileDropWidget::dropEvent(QDropEvent* event) {
    m_dragActive = false;
    update();
    
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QStringList files;
        for (const QUrl& url : mimeData->urls()) {
            if (url.isLocalFile()) {
                files.append(url.toLocalFile());
            }
        }
        
        if (!files.isEmpty()) {
            emit filesDropped(files);
            event->acceptProposedAction();
        }
    }
}

void FileDropWidget::paintEvent(QPaintEvent* event) {
    QFrame::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect textRect = rect().adjusted(20, 20, -20, -20);
    
    if (m_dragActive) {
        painter.setPen(QPen(QColor("#0078d4"), 2));
        painter.setFont(QFont(font().family(), font().pointSize() + 2, QFont::Bold));
    } else {
        painter.setPen(QPen(QColor("#666"), 2));
        painter.setFont(font());
    }
    
    QString text = m_dragActive ? 
        "Drop files here to add them to the digestion queue" :
        "Drag and drop files or directories here\n\nSupported formats:\n• Source code (C++, Python, JavaScript, etc.)\n• Assembly files (*.asm, *.s)\n• Documentation (*.md, *.txt, *.rst)\n• Data files (*.json, *.xml)";
    
    painter.drawText(textRect, Qt::AlignCenter | Qt::TextWordWrap, text);
}

// ParameterWidget Implementation
ParameterWidget::ParameterWidget(const QString& title, QWidget* parent)
    : QWidget(parent)
    , m_title(title)
    , m_collapsed(false)
    , m_parameterCount(0)
{
    setupUI();
}

void ParameterWidget::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    
    // Header widget
    m_headerWidget = new QWidget();
    m_headerWidget->setStyleSheet(
        "QWidget {"
        "    background-color: #e0e0e0;"
        "    border: 1px solid #ccc;"
        "    border-bottom: none;"
        "}"
        "QWidget:hover {"
        "    background-color: #d0d0d0;"
        "}"
    );
    m_headerWidget->setCursor(Qt::PointingHandCursor);
    
    m_headerLayout = new QHBoxLayout(m_headerWidget);
    m_headerLayout->setContentsMargins(8, 4, 8, 4);
    
    m_toggleButton = new QPushButton();
    m_toggleButton->setFlat(true);
    m_toggleButton->setFixedSize(16, 16);
    updateToggleButton();
    
    m_titleLabel = new QLabel(m_title);
    QFont titleFont = m_titleLabel->font();
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    
    m_headerLayout->addWidget(m_toggleButton);
    m_headerLayout->addWidget(m_titleLabel);
    m_headerLayout->addStretch();
    
    connect(m_toggleButton, &QPushButton::clicked, this, &ParameterWidget::onHeaderClicked);
    
    // Content widget
    m_contentWidget = new QWidget();
    m_contentWidget->setStyleSheet(
        "QWidget {"
        "    background-color: white;"
        "    border: 1px solid #ccc;"
        "    border-top: none;"
        "}"
    );
    
    m_contentLayout = new QGridLayout(m_contentWidget);
    m_contentLayout->setContentsMargins(12, 8, 12, 8);
    m_contentLayout->setSpacing(8);
    
    m_layout->addWidget(m_headerWidget);
    m_layout->addWidget(m_contentWidget);
}

void ParameterWidget::addParameter(const QString& name, QWidget* widget, const QString& description) {
    QLabel* nameLabel = new QLabel(name + ":");
    nameLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    
    m_contentLayout->addWidget(nameLabel, m_parameterCount, 0);
    m_contentLayout->addWidget(widget, m_parameterCount, 1);
    
    if (!description.isEmpty()) {
        QLabel* descLabel = new QLabel(description);
        descLabel->setStyleSheet("QLabel { color: gray; font-style: italic; font-size: 9pt; }");
        descLabel->setWordWrap(true);
        m_contentLayout->addWidget(descLabel, m_parameterCount, 2);
    }
    
    m_parameterCount++;
}

void ParameterWidget::setCollapsed(bool collapsed) {
    m_collapsed = collapsed;
    m_contentWidget->setVisible(!collapsed);
    updateToggleButton();
    emit toggled(!collapsed);
}

bool ParameterWidget::isCollapsed() const {
    return m_collapsed;
}

void ParameterWidget::onHeaderClicked() {
    setCollapsed(!m_collapsed);
}

void ParameterWidget::updateToggleButton() {
    if (m_collapsed) {
        m_toggleButton->setText("▶");
    } else {
        m_toggleButton->setText("▼");
    }
}

// ModelPresetWidget Implementation
ModelPresetWidget::ModelPresetWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void ModelPresetWidget::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);
}

void ModelPresetWidget::addPreset(const QString& name, const QString& description, const QJsonObject& config) {
    m_presets[name] = config;
    createPresetButton(name, description);
}

void ModelPresetWidget::createPresetButton(const QString& name, const QString& description) {
    QPushButton* button = new QPushButton();
    button->setText(QString("%1\n%2").arg(name).arg(description));
    button->setMinimumHeight(50);
    button->setStyleSheet(
        "QPushButton {"
        "    text-align: left;"
        "    padding: 8px;"
        "    border: 1px solid #ccc;"
        "    border-radius: 4px;"
        "    background-color: #f8f8f8;"
        "}"
        "QPushButton:hover {"
        "    background-color: #e8e8e8;"
        "    border-color: #0078d4;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #d0d0d0;"
        "}"
    );
    
    connect(button, &QPushButton::clicked, this, &ModelPresetWidget::onPresetButtonClicked);
    
    m_presetButtons[name] = button;
    m_layout->addWidget(button);
}

QJsonObject ModelPresetWidget::getPresetConfig(const QString& name) const {
    return m_presets.value(name);
}

QStringList ModelPresetWidget::getPresetNames() const {
    return m_presets.keys();
}

void ModelPresetWidget::onPresetButtonClicked() {
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (button) {
        for (auto it = m_presetButtons.begin(); it != m_presetButtons.end(); ++it) {
            if (it.value() == button) {
                QString name = it.key();
                emit presetSelected(name, m_presets[name]);
                break;
            }
        }
    }
}

// TrainingMetricsWidget Implementation
TrainingMetricsWidget::TrainingMetricsWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
}

void TrainingMetricsWidget::setupUI() {
    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(12, 12, 12, 12);
    m_layout->setSpacing(8);
    
    setStyleSheet(
        "TrainingMetricsWidget {"
        "    background-color: #f8f8f8;"
        "    border: 1px solid #ddd;"
        "    border-radius: 6px;"
        "}"
        "QLabel {"
        "    padding: 4px;"
        "}"
    );
    
    createMetricDisplay("Loss", "%.6f");
    createMetricDisplay("Learning Rate", "%.2e");
    createMetricDisplay("Epoch", "%d");
    createMetricDisplay("Step", "%d");
    createMetricDisplay("Elapsed Time", "%s");
    createMetricDisplay("ETA", "%s");
    createMetricDisplay("Samples/sec", "%.1f");
    createMetricDisplay("GPU Memory", "%s");
}

void TrainingMetricsWidget::createMetricDisplay(const QString& name, const QString& format) {
    int row = m_metricLabels.size() / 2;
    int col = (m_metricLabels.size() % 2) * 2;
    
    QLabel* nameLabel = new QLabel(name + ":");
    nameLabel->setStyleSheet("QLabel { font-weight: bold; }");
    
    QLabel* valueLabel = new QLabel("--");
    valueLabel->setStyleSheet("QLabel { color: #333; font-family: 'Courier', monospace; }");
    
    m_layout->addWidget(nameLabel, row, col);
    m_layout->addWidget(valueLabel, row, col + 1);
    
    m_metricLabels[name] = valueLabel;
    m_metricFormats[name] = format;
}

void TrainingMetricsWidget::updateMetrics(const QJsonObject& metrics) {
    for (auto it = metrics.begin(); it != metrics.end(); ++it) {
        QString key = it.key();
        QVariant value = it.value().toVariant();
        
        // Convert metric keys to display names
        QString displayName;
        if (key == "current_loss") displayName = "Loss";
        else if (key == "learning_rate") displayName = "Learning Rate";
        else if (key == "current_epoch") displayName = "Epoch";
        else if (key == "current_step") displayName = "Step";
        else if (key == "elapsed_time") displayName = "Elapsed Time";
        else continue;
        
        if (m_metricLabels.contains(displayName)) {
            QLabel* label = m_metricLabels[displayName];
            QString format = m_metricFormats[displayName];
            
            QString displayValue;
            if (displayName == "Elapsed Time" || displayName == "ETA") {
                int seconds = value.toInt();
                int hours = seconds / 3600;
                int minutes = (seconds % 3600) / 60;
                int secs = seconds % 60;
                displayValue = QString("%1:%2:%3")
                    .arg(hours, 2, 10, QChar('0'))
                    .arg(minutes, 2, 10, QChar('0'))
                    .arg(secs, 2, 10, QChar('0'));
            } else if (format.contains('d')) {
                displayValue = QString::asprintf(format.toLatin1().data(), value.toInt());
            } else if (format.contains('f') || format.contains('e')) {
                displayValue = QString::asprintf(format.toLatin1().data(), value.toDouble());
            } else {
                displayValue = value.toString();
            }
            
            label->setText(displayValue);
        }
    }
}

void TrainingMetricsWidget::reset() {
    for (QLabel* label : m_metricLabels) {
        label->setText("--");
    }
}

// ModelManagerWidget Implementation
ModelManagerWidget::ModelManagerWidget(QWidget* parent) : QWidget(parent) {
    setupUI();
    
    // Set default models directory
    m_modelsDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/AI Models";
}

void ModelManagerWidget::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(8);
    
    // Models table
    m_modelsTable = new QTableWidget(0, 5);
    m_modelsTable->setHorizontalHeaderLabels({"Name", "Size", "Type", "Created", "Path"});
    m_modelsTable->horizontalHeader()->setStretchLastSection(true);
    m_modelsTable->setAlternatingRowColors(true);
    m_modelsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_modelsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    
    // Resize columns
    m_modelsTable->setColumnWidth(0, 150);  // Name
    m_modelsTable->setColumnWidth(1, 80);   // Size
    m_modelsTable->setColumnWidth(2, 60);   // Type
    m_modelsTable->setColumnWidth(3, 120);  // Created
    
    connect(m_modelsTable, &QTableWidget::itemSelectionChanged, 
            this, &ModelManagerWidget::onModelSelectionChanged);
    connect(m_modelsTable, &QTableWidget::itemDoubleClicked, 
            this, &ModelManagerWidget::onModelDoubleClicked);
    
    m_layout->addWidget(m_modelsTable);
}

void ModelManagerWidget::refreshModels() {
    loadModelsFromDirectory(m_modelsDirectory);
}

void ModelManagerWidget::loadModelsFromDirectory(const QString& directory) {
    m_modelsTable->setRowCount(0);
    
    QDir dir(directory);
    if (!dir.exists()) return;
    
    QStringList filters;
    filters << "*.gguf" << "*.bin" << "*.safetensors";
    
    QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Time);
    
    for (const QFileInfo& fileInfo : files) {
        addModel(fileInfo.baseName(), fileInfo.filePath(), fileInfo.size(), "GGUF");
    }
}

void ModelManagerWidget::addModel(const QString& name, const QString& path, qint64 size, const QString& type) {
    int row = m_modelsTable->rowCount();
    m_modelsTable->insertRow(row);
    
    QFileInfo info(path);
    
    m_modelsTable->setItem(row, 0, new QTableWidgetItem(name));
    m_modelsTable->setItem(row, 1, new QTableWidgetItem(formatFileSize(size)));
    m_modelsTable->setItem(row, 2, new QTableWidgetItem(type));
    m_modelsTable->setItem(row, 3, new QTableWidgetItem(info.lastModified().toString("yyyy-MM-dd hh:mm")));
    m_modelsTable->setItem(row, 4, new QTableWidgetItem(path));
}

void ModelManagerWidget::removeModel(const QString& name) {
    for (int row = 0; row < m_modelsTable->rowCount(); ++row) {
        if (m_modelsTable->item(row, 0)->text() == name) {
            m_modelsTable->removeRow(row);
            break;
        }
    }
}

QString ModelManagerWidget::getSelectedModelPath() const {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        return m_modelsTable->item(currentRow, 4)->text();
    }
    return QString();
}

void ModelManagerWidget::onModelSelectionChanged() {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        QString name = m_modelsTable->item(currentRow, 0)->text();
        QString path = m_modelsTable->item(currentRow, 4)->text();
        emit modelSelected(name, path);
    }
}

void ModelManagerWidget::onModelDoubleClicked() {
    int currentRow = m_modelsTable->currentRow();
    if (currentRow >= 0) {
        QString name = m_modelsTable->item(currentRow, 0)->text();
        QString path = m_modelsTable->item(currentRow, 4)->text();
        emit modelDoubleClicked(name, path);
    }
}

QString ModelManagerWidget::formatFileSize(qint64 bytes) const {
    const qint64 KB = 1024;
    const qint64 MB = KB * 1024;
    const qint64 GB = MB * 1024;
    
    if (bytes >= GB) {
        return QString::number(bytes / (double)GB, 'f', 1) + " GB";
    } else if (bytes >= MB) {
        return QString::number(bytes / (double)MB, 'f', 1) + " MB";
    } else if (bytes >= KB) {
        return QString::number(bytes / (double)KB, 'f', 1) + " KB";
    } else {
        return QString::number(bytes) + " bytes";
    }
}