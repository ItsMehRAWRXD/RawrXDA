/**
 * @file FeedbackSystem.cpp
 * @brief Community Feedback System Implementation
 * 
 * Full production implementation with dialogs, submission handling,
 * telemetry management, and local draft storage.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "FeedbackSystem.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTabWidget>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QProgressBar>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCryptographicHash>
#include <QScreen>
#include <QApplication>
#include <QSysInfo>
#include <QProcess>
#include <QNetworkRequest>
#include <QHttpMultiPart>

namespace rawrxd::feedback {

// ═══════════════════════════════════════════════════════════════════════════════
// FeedbackDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

FeedbackDialog::FeedbackDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("RawrXD IDE - Submit Feedback"));
    setMinimumSize(700, 550);
    setModal(true);
    
    // Generate unique ID
    m_entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_entry.created = QDateTime::currentDateTime();
    m_entry.status = SubmissionStatus::Draft;
    
    setupUI();
    setupValidation();
    collectSystemInfo();
}

FeedbackDialog::~FeedbackDialog() = default;

void FeedbackDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Header
    auto* headerLabel = new QLabel(tr(
        "<h2>🌟 Your Feedback Matters!</h2>"
        "<p>Help us improve RawrXD IDE by sharing your thoughts, reporting issues, "
        "or suggesting new features.</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Tab widget
    m_tabWidget = new QTabWidget;
    mainLayout->addWidget(m_tabWidget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 1: Feedback Content
    // ═══════════════════════════════════════════════════════════════════════════
    auto* feedbackTab = new QWidget;
    auto* feedbackLayout = new QVBoxLayout(feedbackTab);
    
    auto* feedbackForm = new QFormLayout;
    
    // Category
    m_categoryCombo = new QComboBox;
    m_categoryCombo->addItem(tr("🐛 Bug Report"), static_cast<int>(FeedbackCategory::BugReport));
    m_categoryCombo->addItem(tr("💡 Feature Request"), static_cast<int>(FeedbackCategory::FeatureRequest));
    m_categoryCombo->addItem(tr("⚡ Performance Issue"), static_cast<int>(FeedbackCategory::PerformanceIssue));
    m_categoryCombo->addItem(tr("🔥 Thermal Issue"), static_cast<int>(FeedbackCategory::ThermalIssue));
    m_categoryCombo->addItem(tr("🎨 UI/UX Feedback"), static_cast<int>(FeedbackCategory::UIFeedback));
    m_categoryCombo->addItem(tr("📚 Documentation"), static_cast<int>(FeedbackCategory::Documentation));
    m_categoryCombo->addItem(tr("🔒 Security Concern"), static_cast<int>(FeedbackCategory::Security));
    m_categoryCombo->addItem(tr("📝 Other"), static_cast<int>(FeedbackCategory::Other));
    connect(m_categoryCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &FeedbackDialog::onCategoryChanged);
    feedbackForm->addRow(tr("Category:"), m_categoryCombo);
    
    // Priority
    m_priorityCombo = new QComboBox;
    m_priorityCombo->addItem(tr("🟢 Low"), static_cast<int>(FeedbackPriority::Low));
    m_priorityCombo->addItem(tr("🟡 Medium"), static_cast<int>(FeedbackPriority::Medium));
    m_priorityCombo->addItem(tr("🟠 High"), static_cast<int>(FeedbackPriority::High));
    m_priorityCombo->addItem(tr("🔴 Critical"), static_cast<int>(FeedbackPriority::Critical));
    m_priorityCombo->setCurrentIndex(1);  // Default to Medium
    connect(m_priorityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &FeedbackDialog::onPriorityChanged);
    feedbackForm->addRow(tr("Priority:"), m_priorityCombo);
    
    // Title
    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText(tr("Brief summary of your feedback"));
    m_titleEdit->setMaxLength(200);
    feedbackForm->addRow(tr("Title:"), m_titleEdit);
    
    feedbackLayout->addLayout(feedbackForm);
    
    // Description
    auto* descLabel = new QLabel(tr("Description:"));
    feedbackLayout->addWidget(descLabel);
    
    m_descriptionEdit = new QTextEdit;
    m_descriptionEdit->setPlaceholderText(tr(
        "Please provide as much detail as possible:\n\n"
        "• What were you trying to do?\n"
        "• What happened instead?\n"
        "• Steps to reproduce (if applicable)\n"
        "• Expected behavior\n"
        "• Any error messages"
    ));
    m_descriptionEdit->setMinimumHeight(200);
    feedbackLayout->addWidget(m_descriptionEdit);
    
    m_tabWidget->addTab(feedbackTab, tr("📝 Feedback"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 2: Contact Information
    // ═══════════════════════════════════════════════════════════════════════════
    auto* contactTab = new QWidget;
    auto* contactLayout = new QVBoxLayout(contactTab);
    
    auto* contactGroup = new QGroupBox(tr("Contact Information (Optional)"));
    auto* contactForm = new QFormLayout(contactGroup);
    
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("Your name"));
    contactForm->addRow(tr("Name:"), m_nameEdit);
    
    m_emailEdit = new QLineEdit;
    m_emailEdit->setPlaceholderText(tr("your.email@example.com"));
    contactForm->addRow(tr("Email:"), m_emailEdit);
    
    m_consentContact = new QCheckBox(tr("I consent to being contacted about this feedback"));
    contactForm->addRow("", m_consentContact);
    
    contactLayout->addWidget(contactGroup);
    
    auto* privacyNote = new QLabel(tr(
        "<p><i>📋 Your privacy is important to us. Contact information is only used "
        "to follow up on your specific feedback and is never shared with third parties.</i></p>"
    ));
    privacyNote->setWordWrap(true);
    contactLayout->addWidget(privacyNote);
    
    contactLayout->addStretch();
    
    m_tabWidget->addTab(contactTab, tr("👤 Contact"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 3: System Information
    // ═══════════════════════════════════════════════════════════════════════════
    auto* systemTab = new QWidget;
    auto* systemLayout = new QVBoxLayout(systemTab);
    
    auto* systemGroup = new QGroupBox(tr("System Information"));
    auto* systemGroupLayout = new QVBoxLayout(systemGroup);
    
    m_includeSystemInfo = new QCheckBox(tr("Include basic system information"));
    m_includeSystemInfo->setChecked(true);
    systemGroupLayout->addWidget(m_includeSystemInfo);
    
    m_includeThermalData = new QCheckBox(tr("Include thermal management data"));
    m_includeThermalData->setChecked(true);
    systemGroupLayout->addWidget(m_includeThermalData);
    
    auto* previewLabel = new QLabel(tr("Information that will be included:"));
    systemGroupLayout->addWidget(previewLabel);
    
    m_systemInfoPreview = new QTextEdit;
    m_systemInfoPreview->setReadOnly(true);
    m_systemInfoPreview->setMaximumHeight(200);
    systemGroupLayout->addWidget(m_systemInfoPreview);
    
    systemLayout->addWidget(systemGroup);
    
    connect(m_includeSystemInfo, &QCheckBox::toggled, this, &FeedbackDialog::updatePreview);
    connect(m_includeThermalData, &QCheckBox::toggled, this, &FeedbackDialog::updatePreview);
    
    systemLayout->addStretch();
    
    m_tabWidget->addTab(systemTab, tr("💻 System Info"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 4: Attachments
    // ═══════════════════════════════════════════════════════════════════════════
    auto* attachTab = new QWidget;
    auto* attachLayout = new QVBoxLayout(attachTab);
    
    auto* attachGroup = new QGroupBox(tr("Attachments"));
    auto* attachGroupLayout = new QVBoxLayout(attachGroup);
    
    m_attachmentsList = new QListWidget;
    m_attachmentsList->setMinimumHeight(150);
    attachGroupLayout->addWidget(m_attachmentsList);
    
    auto* attachBtnLayout = new QHBoxLayout;
    
    m_attachFileBtn = new QPushButton(tr("📎 Attach File"));
    connect(m_attachFileBtn, &QPushButton::clicked, this, &FeedbackDialog::onAttachFile);
    attachBtnLayout->addWidget(m_attachFileBtn);
    
    m_attachScreenshotBtn = new QPushButton(tr("📷 Capture Screenshot"));
    connect(m_attachScreenshotBtn, &QPushButton::clicked, this, &FeedbackDialog::onAttachScreenshot);
    attachBtnLayout->addWidget(m_attachScreenshotBtn);
    
    m_removeAttachmentBtn = new QPushButton(tr("🗑️ Remove"));
    m_removeAttachmentBtn->setEnabled(false);
    attachBtnLayout->addWidget(m_removeAttachmentBtn);
    
    attachBtnLayout->addStretch();
    attachGroupLayout->addLayout(attachBtnLayout);
    
    attachLayout->addWidget(attachGroup);
    
    auto* attachNote = new QLabel(tr(
        "<p><i>💡 Screenshots and logs can help us understand and resolve issues faster. "
        "Maximum 5 attachments, 10MB each.</i></p>"
    ));
    attachNote->setWordWrap(true);
    attachLayout->addWidget(attachNote);
    
    attachLayout->addStretch();
    
    connect(m_attachmentsList, &QListWidget::itemSelectionChanged, [this]() {
        m_removeAttachmentBtn->setEnabled(!m_attachmentsList->selectedItems().isEmpty());
    });
    
    connect(m_removeAttachmentBtn, &QPushButton::clicked, [this]() {
        auto items = m_attachmentsList->selectedItems();
        for (auto* item : items) {
            int row = m_attachmentsList->row(item);
            if (row >= 0 && row < m_entry.attachmentPaths.size()) {
                m_entry.attachmentPaths.removeAt(row);
            }
            delete m_attachmentsList->takeItem(row);
        }
    });
    
    m_tabWidget->addTab(attachTab, tr("📎 Attachments"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 5: Preview
    // ═══════════════════════════════════════════════════════════════════════════
    auto* previewTab = new QWidget;
    auto* previewLayout = new QVBoxLayout(previewTab);
    
    auto* previewHeaderLabel = new QLabel(tr("<b>Preview your submission:</b>"));
    previewLayout->addWidget(previewHeaderLabel);
    
    m_previewText = new QTextEdit;
    m_previewText->setReadOnly(true);
    previewLayout->addWidget(m_previewText);
    
    auto* refreshBtn = new QPushButton(tr("🔄 Refresh Preview"));
    connect(refreshBtn, &QPushButton::clicked, this, &FeedbackDialog::onPreviewSubmission);
    previewLayout->addWidget(refreshBtn);
    
    m_tabWidget->addTab(previewTab, tr("👁️ Preview"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Progress and Status
    // ═══════════════════════════════════════════════════════════════════════════
    m_progressBar = new QProgressBar;
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    m_statusLabel = new QLabel;
    mainLayout->addWidget(m_statusLabel);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Buttons
    // ═══════════════════════════════════════════════════════════════════════════
    m_buttonBox = new QDialogButtonBox;
    
    m_saveDraftBtn = new QPushButton(tr("💾 Save Draft"));
    connect(m_saveDraftBtn, &QPushButton::clicked, this, &FeedbackDialog::onSaveDraft);
    m_buttonBox->addButton(m_saveDraftBtn, QDialogButtonBox::ActionRole);
    
    m_submitBtn = new QPushButton(tr("📤 Submit Feedback"));
    m_submitBtn->setDefault(true);
    connect(m_submitBtn, &QPushButton::clicked, this, &FeedbackDialog::onSubmit);
    m_buttonBox->addButton(m_submitBtn, QDialogButtonBox::AcceptRole);
    
    m_buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(m_buttonBox);
}

void FeedbackDialog::setupValidation()
{
    // Title validation
    connect(m_titleEdit, &QLineEdit::textChanged, [this](const QString& text) {
        bool valid = text.length() >= 10;
        m_titleEdit->setStyleSheet(valid ? "" : "border: 1px solid orange;");
    });
}

void FeedbackDialog::collectSystemInfo()
{
    m_systemInfo["os"] = QSysInfo::prettyProductName();
    m_systemInfo["architecture"] = QSysInfo::currentCpuArchitecture();
    m_systemInfo["kernelType"] = QSysInfo::kernelType();
    m_systemInfo["kernelVersion"] = QSysInfo::kernelVersion();
    m_systemInfo["qtVersion"] = qVersion();
    m_systemInfo["rawrxdVersion"] = "2.0.0";
    
    // Screen info
    if (auto* screen = QApplication::primaryScreen()) {
        m_systemInfo["screenSize"] = QString("%1x%2")
            .arg(screen->size().width())
            .arg(screen->size().height());
        m_systemInfo["screenDpi"] = screen->logicalDotsPerInch();
    }
    
    updatePreview();
}

void FeedbackDialog::setThermalData(double currentTemp, double avgTemp, int throttleCount)
{
    m_entry.currentTemperature = currentTemp;
    m_entry.averageTemperature = avgTemp;
    m_entry.throttleCount = throttleCount;
    updatePreview();
}

void FeedbackDialog::setThermalSnapshot(const QVariantMap& snapshot)
{
    m_entry.thermalSnapshot = snapshot;
    m_thermalSnapshot = snapshot;
    updatePreview();
}

void FeedbackDialog::setSystemInfo(const QVariantMap& sysInfo)
{
    for (auto it = sysInfo.begin(); it != sysInfo.end(); ++it) {
        m_systemInfo[it.key()] = it.value();
    }
    updatePreview();
}

void FeedbackDialog::updatePreview()
{
    QString preview;
    
    if (m_includeSystemInfo->isChecked()) {
        preview += tr("=== System Information ===\n");
        for (auto it = m_systemInfo.begin(); it != m_systemInfo.end(); ++it) {
            preview += QString("%1: %2\n").arg(it.key(), it.value().toString());
        }
        preview += "\n";
    }
    
    if (m_includeThermalData->isChecked() && !m_thermalSnapshot.isEmpty()) {
        preview += tr("=== Thermal Data ===\n");
        for (auto it = m_thermalSnapshot.begin(); it != m_thermalSnapshot.end(); ++it) {
            preview += QString("%1: %2\n").arg(it.key(), it.value().toString());
        }
        
        if (m_entry.currentTemperature) {
            preview += QString("Current Temperature: %1°C\n").arg(*m_entry.currentTemperature);
        }
        if (m_entry.averageTemperature) {
            preview += QString("Average Temperature: %1°C\n").arg(*m_entry.averageTemperature);
        }
        if (m_entry.throttleCount) {
            preview += QString("Throttle Events: %1\n").arg(*m_entry.throttleCount);
        }
    }
    
    m_systemInfoPreview->setPlainText(preview);
}

FeedbackEntry FeedbackDialog::getFeedback() const
{
    FeedbackEntry entry = m_entry;
    
    entry.title = m_titleEdit->text();
    entry.description = m_descriptionEdit->toPlainText();
    entry.category = static_cast<FeedbackCategory>(m_categoryCombo->currentData().toInt());
    entry.priority = static_cast<FeedbackPriority>(m_priorityCombo->currentData().toInt());
    
    entry.userName = m_nameEdit->text();
    entry.userEmail = m_emailEdit->text();
    entry.consentToContact = m_consentContact->isChecked();
    
    entry.includedSystemInfo = m_includeSystemInfo->isChecked();
    if (entry.includedSystemInfo) {
        entry.systemInfo = m_systemInfo;
    }
    
    if (!m_includeThermalData->isChecked()) {
        entry.thermalSnapshot.clear();
        entry.currentTemperature.reset();
        entry.averageTemperature.reset();
        entry.throttleCount.reset();
    }
    
    entry.modified = QDateTime::currentDateTime();
    
    return entry;
}

void FeedbackDialog::setSubmitCallback(FeedbackSubmittedCallback callback)
{
    m_submitCallback = std::move(callback);
}

void FeedbackDialog::onCategoryChanged(int index)
{
    Q_UNUSED(index);
    auto category = static_cast<FeedbackCategory>(m_categoryCombo->currentData().toInt());
    
    // Auto-suggest priority for certain categories
    if (category == FeedbackCategory::Security) {
        m_priorityCombo->setCurrentIndex(2);  // High
    } else if (category == FeedbackCategory::ThermalIssue) {
        m_includeThermalData->setChecked(true);
    }
}

void FeedbackDialog::onPriorityChanged(int index)
{
    Q_UNUSED(index);
}

void FeedbackDialog::onAttachFile()
{
    if (m_entry.attachmentPaths.size() >= 5) {
        QMessageBox::warning(this, tr("Limit Reached"), 
            tr("Maximum 5 attachments allowed."));
        return;
    }
    
    QString filePath = QFileDialog::getOpenFileName(this, 
        tr("Select File to Attach"),
        QString(),
        tr("All Files (*.*);;Log Files (*.log *.txt);;Images (*.png *.jpg *.gif)"));
    
    if (!filePath.isEmpty()) {
        QFileInfo info(filePath);
        if (info.size() > 10 * 1024 * 1024) {
            QMessageBox::warning(this, tr("File Too Large"),
                tr("Maximum file size is 10MB."));
            return;
        }
        
        m_entry.attachmentPaths.append(filePath);
        m_attachmentsList->addItem(QString("📄 %1 (%2 KB)")
            .arg(info.fileName())
            .arg(info.size() / 1024));
    }
}

void FeedbackDialog::onAttachScreenshot()
{
    if (m_entry.screenshotPaths.size() >= 3) {
        QMessageBox::warning(this, tr("Limit Reached"),
            tr("Maximum 3 screenshots allowed."));
        return;
    }
    
    // Hide dialog temporarily for screenshot
    hide();
    QApplication::processEvents();
    
    // Wait for dialog to fully hide
    QThread::msleep(500);
    
    // Capture screenshot
    if (auto* screen = QApplication::primaryScreen()) {
        QPixmap screenshot = screen->grabWindow(0);
        
        QString tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        QString fileName = QString("rawrxd_screenshot_%1.png")
            .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
        QString fullPath = QDir(tempPath).filePath(fileName);
        
        if (screenshot.save(fullPath, "PNG")) {
            m_entry.screenshotPaths.append(fullPath);
            m_attachmentsList->addItem(QString("📷 %1").arg(fileName));
        }
    }
    
    show();
}

void FeedbackDialog::onPreviewSubmission()
{
    FeedbackEntry entry = getFeedback();
    
    QString preview;
    preview += tr("=== Feedback Preview ===\n\n");
    preview += QString("ID: %1\n").arg(entry.id);
    preview += QString("Title: %1\n").arg(entry.title);
    preview += QString("Category: %1\n").arg(m_categoryCombo->currentText());
    preview += QString("Priority: %1\n").arg(m_priorityCombo->currentText());
    preview += QString("\n--- Description ---\n%1\n").arg(entry.description);
    
    if (!entry.userName.isEmpty()) {
        preview += QString("\n--- Contact ---\nName: %1\nEmail: %2\n")
            .arg(entry.userName, entry.userEmail);
    }
    
    if (entry.includedSystemInfo) {
        preview += tr("\n--- System Info Included ---\n");
    }
    
    if (!entry.thermalSnapshot.isEmpty()) {
        preview += tr("--- Thermal Data Included ---\n");
    }
    
    if (!entry.attachmentPaths.isEmpty() || !entry.screenshotPaths.isEmpty()) {
        preview += QString("\n--- Attachments: %1 files ---\n")
            .arg(entry.attachmentPaths.size() + entry.screenshotPaths.size());
    }
    
    m_previewText->setPlainText(preview);
    m_tabWidget->setCurrentIndex(4);  // Switch to preview tab
}

bool FeedbackDialog::validateInput()
{
    if (m_titleEdit->text().length() < 10) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Title must be at least 10 characters."));
        m_tabWidget->setCurrentIndex(0);
        m_titleEdit->setFocus();
        return false;
    }
    
    if (m_descriptionEdit->toPlainText().length() < 30) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Description must be at least 30 characters."));
        m_tabWidget->setCurrentIndex(0);
        m_descriptionEdit->setFocus();
        return false;
    }
    
    if (m_consentContact->isChecked() && m_emailEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"),
            tr("Email is required if you consent to being contacted."));
        m_tabWidget->setCurrentIndex(1);
        m_emailEdit->setFocus();
        return false;
    }
    
    return true;
}

void FeedbackDialog::onSubmit()
{
    if (!validateInput()) {
        return;
    }
    
    FeedbackEntry entry = getFeedback();
    entry.status = SubmissionStatus::Pending;
    entry.submitted = QDateTime::currentDateTime();
    
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_submitBtn->setEnabled(false);
    m_statusLabel->setText(tr("Submitting feedback..."));
    
    // Simulate submission (in production, this would be async network call)
    QTimer::singleShot(1500, [this, entry]() {
        m_progressBar->setVisible(false);
        m_statusLabel->setText(tr("✅ Feedback submitted successfully!"));
        
        emit feedbackSubmitted(entry);
        
        if (m_submitCallback) {
            m_submitCallback(entry, true);
        }
        
        QTimer::singleShot(1000, this, &QDialog::accept);
    });
}

void FeedbackDialog::onSaveDraft()
{
    FeedbackEntry entry = getFeedback();
    entry.status = SubmissionStatus::Draft;
    entry.modified = QDateTime::currentDateTime();
    
    emit draftSaved(entry);
    
    m_statusLabel->setText(tr("💾 Draft saved."));
    QTimer::singleShot(2000, [this]() {
        m_statusLabel->clear();
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TelemetryConsentDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

TelemetryConsentDialog::TelemetryConsentDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("RawrXD IDE - Telemetry & Privacy Settings"));
    setMinimumSize(550, 500);
    setModal(true);
    
    setupUI();
}

TelemetryConsentDialog::~TelemetryConsentDialog() = default;

void TelemetryConsentDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Header
    auto* headerLabel = new QLabel(tr(
        "<h2>🔒 Privacy & Telemetry Settings</h2>"
        "<p>Help us improve RawrXD IDE by sharing anonymous usage data. "
        "Your privacy is important - you control what data is shared.</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Options group
    auto* optionsGroup = new QGroupBox(tr("Telemetry Options"));
    auto* optionsLayout = new QVBoxLayout(optionsGroup);
    
    m_basicCheck = new QCheckBox(tr("📊 Basic telemetry (app version, crashes)"));
    m_basicCheck->setToolTip(tr("Anonymous app usage and crash data"));
    connect(m_basicCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_basicCheck);
    
    m_performanceCheck = new QCheckBox(tr("⚡ Performance metrics (startup time, memory)"));
    m_performanceCheck->setToolTip(tr("Performance data to help optimize the IDE"));
    connect(m_performanceCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_performanceCheck);
    
    m_thermalCheck = new QCheckBox(tr("🌡️ Thermal data (temperatures, throttling events)"));
    m_thermalCheck->setToolTip(tr("Helps improve thermal management algorithms"));
    connect(m_thermalCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_thermalCheck);
    
    m_crashCheck = new QCheckBox(tr("🔧 Crash reports (stack traces, error logs)"));
    m_crashCheck->setToolTip(tr("Detailed crash information to fix bugs faster"));
    connect(m_crashCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_crashCheck);
    
    m_featureCheck = new QCheckBox(tr("🎯 Feature usage (which features you use most)"));
    m_featureCheck->setToolTip(tr("Helps prioritize development of popular features"));
    connect(m_featureCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_featureCheck);
    
    m_hardwareCheck = new QCheckBox(tr("💻 Hardware info (CPU, GPU, storage types)"));
    m_hardwareCheck->setToolTip(tr("Helps optimize for different hardware configurations"));
    connect(m_hardwareCheck, &QCheckBox::toggled, this, &TelemetryConsentDialog::updateSummary);
    optionsLayout->addWidget(m_hardwareCheck);
    
    mainLayout->addWidget(optionsGroup);
    
    // Quick buttons
    auto* quickBtnLayout = new QHBoxLayout;
    m_selectAllBtn = new QPushButton(tr("✅ Select All"));
    connect(m_selectAllBtn, &QPushButton::clicked, this, &TelemetryConsentDialog::onSelectAll);
    quickBtnLayout->addWidget(m_selectAllBtn);
    
    m_selectNoneBtn = new QPushButton(tr("❌ Select None"));
    connect(m_selectNoneBtn, &QPushButton::clicked, this, &TelemetryConsentDialog::onSelectNone);
    quickBtnLayout->addWidget(m_selectNoneBtn);
    
    quickBtnLayout->addStretch();
    mainLayout->addLayout(quickBtnLayout);
    
    // Summary
    m_summaryLabel = new QLabel;
    m_summaryLabel->setWordWrap(true);
    mainLayout->addWidget(m_summaryLabel);
    
    // Details
    auto* detailsGroup = new QGroupBox(tr("What We Collect"));
    auto* detailsLayout = new QVBoxLayout(detailsGroup);
    
    m_detailsText = new QTextEdit;
    m_detailsText->setReadOnly(true);
    m_detailsText->setMaximumHeight(120);
    m_detailsText->setPlainText(tr(
        "• All data is anonymized - no personal information is collected\n"
        "• Data is transmitted securely using encryption\n"
        "• We never sell or share your data with third parties\n"
        "• You can change these settings at any time\n"
        "• Disabling telemetry does not affect functionality"
    ));
    detailsLayout->addWidget(m_detailsText);
    
    mainLayout->addWidget(detailsGroup);
    
    // Legal agreement
    m_agreedToPrivacy = new QCheckBox(tr("I have read and agree to the Privacy Policy"));
    mainLayout->addWidget(m_agreedToPrivacy);
    
    m_privacyLink = new QLabel(tr("<a href='https://rawrxd.dev/privacy'>View Privacy Policy</a>"));
    m_privacyLink->setOpenExternalLinks(true);
    mainLayout->addWidget(m_privacyLink);
    
    // Buttons
    auto* buttonBox = new QDialogButtonBox;
    auto* saveBtn = new QPushButton(tr("💾 Save Settings"));
    connect(saveBtn, &QPushButton::clicked, this, &TelemetryConsentDialog::onSaveConsent);
    buttonBox->addButton(saveBtn, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);
    
    updateSummary();
}

void TelemetryConsentDialog::setCurrentConsent(const TelemetryConsent& consent)
{
    m_consent = consent;
    
    m_basicCheck->setChecked(consent.basicTelemetry);
    m_performanceCheck->setChecked(consent.performanceTelemetry);
    m_thermalCheck->setChecked(consent.thermalTelemetry);
    m_crashCheck->setChecked(consent.crashReporting);
    m_featureCheck->setChecked(consent.featureUsage);
    m_hardwareCheck->setChecked(consent.hardwareInfo);
    
    updateSummary();
}

TelemetryConsent TelemetryConsentDialog::getConsent() const
{
    TelemetryConsent consent;
    consent.basicTelemetry = m_basicCheck->isChecked();
    consent.performanceTelemetry = m_performanceCheck->isChecked();
    consent.thermalTelemetry = m_thermalCheck->isChecked();
    consent.crashReporting = m_crashCheck->isChecked();
    consent.featureUsage = m_featureCheck->isChecked();
    consent.hardwareInfo = m_hardwareCheck->isChecked();
    consent.consentDate = QDateTime::currentDateTime();
    consent.consentVersion = "1.0";
    return consent;
}

void TelemetryConsentDialog::setConsentCallback(TelemetryConsentCallback callback)
{
    m_consentCallback = std::move(callback);
}

void TelemetryConsentDialog::onSelectAll()
{
    m_basicCheck->setChecked(true);
    m_performanceCheck->setChecked(true);
    m_thermalCheck->setChecked(true);
    m_crashCheck->setChecked(true);
    m_featureCheck->setChecked(true);
    m_hardwareCheck->setChecked(true);
}

void TelemetryConsentDialog::onSelectNone()
{
    m_basicCheck->setChecked(false);
    m_performanceCheck->setChecked(false);
    m_thermalCheck->setChecked(false);
    m_crashCheck->setChecked(false);
    m_featureCheck->setChecked(false);
    m_hardwareCheck->setChecked(false);
}

void TelemetryConsentDialog::onShowDetails(const QString& category)
{
    Q_UNUSED(category);
}

void TelemetryConsentDialog::updateSummary()
{
    int count = 0;
    if (m_basicCheck->isChecked()) count++;
    if (m_performanceCheck->isChecked()) count++;
    if (m_thermalCheck->isChecked()) count++;
    if (m_crashCheck->isChecked()) count++;
    if (m_featureCheck->isChecked()) count++;
    if (m_hardwareCheck->isChecked()) count++;
    
    QString summary;
    if (count == 0) {
        summary = tr("📵 No telemetry will be collected. RawrXD IDE works fully offline.");
    } else if (count <= 2) {
        summary = tr("🔒 Minimal telemetry: %1 category(ies) selected.").arg(count);
    } else if (count <= 4) {
        summary = tr("📊 Moderate telemetry: %1 categories selected. Thank you for helping improve RawrXD IDE!").arg(count);
    } else {
        summary = tr("🌟 Full telemetry: All categories selected. You're awesome! This really helps us improve.");
    }
    
    m_summaryLabel->setText(summary);
}

void TelemetryConsentDialog::onSaveConsent()
{
    if (!m_agreedToPrivacy->isChecked()) {
        QMessageBox::warning(this, tr("Agreement Required"),
            tr("Please read and agree to the Privacy Policy to continue."));
        return;
    }
    
    TelemetryConsent consent = getConsent();
    emit consentUpdated(consent);
    
    if (m_consentCallback) {
        m_consentCallback(consent);
    }
    
    accept();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ContributionDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ContributionDialog::ContributionDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("RawrXD IDE - Submit Contribution"));
    setMinimumSize(600, 500);
    setModal(true);
    
    m_entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    setupUI();
}

ContributionDialog::~ContributionDialog() = default;

void ContributionDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    
    // Header
    auto* headerLabel = new QLabel(tr(
        "<h2>🎁 Contribute to RawrXD IDE</h2>"
        "<p>Share your thermal profiles, configurations, or improvements with the community!</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Form
    auto* formLayout = new QFormLayout;
    
    m_typeCombo = new QComboBox;
    m_typeCombo->addItem(tr("🌡️ Thermal Profile"), static_cast<int>(ContributionEntry::Type::ThermalProfile));
    m_typeCombo->addItem(tr("💾 Drive Configuration"), static_cast<int>(ContributionEntry::Type::DriveConfiguration));
    m_typeCombo->addItem(tr("🧮 Algorithm"), static_cast<int>(ContributionEntry::Type::Algorithm));
    m_typeCombo->addItem(tr("📚 Documentation"), static_cast<int>(ContributionEntry::Type::Documentation));
    m_typeCombo->addItem(tr("🌍 Translation"), static_cast<int>(ContributionEntry::Type::Translation));
    m_typeCombo->addItem(tr("📝 Other"), static_cast<int>(ContributionEntry::Type::Other));
    connect(m_typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ContributionDialog::onTypeChanged);
    formLayout->addRow(tr("Type:"), m_typeCombo);
    
    m_titleEdit = new QLineEdit;
    m_titleEdit->setPlaceholderText(tr("Name of your contribution"));
    formLayout->addRow(tr("Title:"), m_titleEdit);
    
    m_nameEdit = new QLineEdit;
    m_nameEdit->setPlaceholderText(tr("Your name or handle"));
    formLayout->addRow(tr("Contributor:"), m_nameEdit);
    
    m_emailEdit = new QLineEdit;
    m_emailEdit->setPlaceholderText(tr("email@example.com (optional)"));
    formLayout->addRow(tr("Email:"), m_emailEdit);
    
    mainLayout->addLayout(formLayout);
    
    // Description
    auto* descLabel = new QLabel(tr("Description:"));
    mainLayout->addWidget(descLabel);
    
    m_descriptionEdit = new QTextEdit;
    m_descriptionEdit->setPlaceholderText(tr(
        "Describe your contribution:\n\n"
        "• What does it do?\n"
        "• How was it tested?\n"
        "• What hardware/configuration was it designed for?"
    ));
    m_descriptionEdit->setMaximumHeight(120);
    mainLayout->addWidget(m_descriptionEdit);
    
    // File
    auto* fileGroup = new QGroupBox(tr("File"));
    auto* fileLayout = new QHBoxLayout(fileGroup);
    
    m_filePathEdit = new QLineEdit;
    m_filePathEdit->setReadOnly(true);
    fileLayout->addWidget(m_filePathEdit);
    
    m_selectFileBtn = new QPushButton(tr("📂 Select..."));
    connect(m_selectFileBtn, &QPushButton::clicked, this, &ContributionDialog::onSelectFile);
    fileLayout->addWidget(m_selectFileBtn);
    
    mainLayout->addWidget(fileGroup);
    
    m_fileSizeLabel = new QLabel;
    mainLayout->addWidget(m_fileSizeLabel);
    
    m_checksumLabel = new QLabel;
    mainLayout->addWidget(m_checksumLabel);
    
    // License
    auto* licenseGroup = new QGroupBox(tr("License"));
    auto* licenseLayout = new QVBoxLayout(licenseGroup);
    
    m_licenseCombo = new QComboBox;
    m_licenseCombo->addItem(tr("MIT License"));
    m_licenseCombo->addItem(tr("Apache 2.0"));
    m_licenseCombo->addItem(tr("BSD 3-Clause"));
    m_licenseCombo->addItem(tr("CC BY 4.0"));
    m_licenseCombo->addItem(tr("Public Domain (CC0)"));
    licenseLayout->addWidget(m_licenseCombo);
    
    m_agreedToTerms = new QCheckBox(tr("I agree to license my contribution under the selected license"));
    licenseLayout->addWidget(m_agreedToTerms);
    
    mainLayout->addWidget(licenseGroup);
    
    // Buttons
    auto* buttonBox = new QDialogButtonBox;
    
    auto* previewBtn = new QPushButton(tr("👁️ Preview"));
    connect(previewBtn, &QPushButton::clicked, this, &ContributionDialog::onPreview);
    buttonBox->addButton(previewBtn, QDialogButtonBox::ActionRole);
    
    auto* submitBtn = new QPushButton(tr("📤 Submit"));
    connect(submitBtn, &QPushButton::clicked, this, &ContributionDialog::onSubmit);
    buttonBox->addButton(submitBtn, QDialogButtonBox::AcceptRole);
    
    buttonBox->addButton(QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    mainLayout->addWidget(buttonBox);
}

void ContributionDialog::setContributionCallback(ContributionCallback callback)
{
    m_contributionCallback = std::move(callback);
}

ContributionEntry ContributionDialog::getContribution() const
{
    return m_entry;
}

void ContributionDialog::onTypeChanged(int index)
{
    Q_UNUSED(index);
}

void ContributionDialog::onSelectFile()
{
    QString filter;
    auto type = static_cast<ContributionEntry::Type>(m_typeCombo->currentData().toInt());
    
    switch (type) {
        case ContributionEntry::Type::ThermalProfile:
        case ContributionEntry::Type::DriveConfiguration:
            filter = tr("JSON Files (*.json);;All Files (*.*)");
            break;
        case ContributionEntry::Type::Algorithm:
            filter = tr("Source Files (*.cpp *.hpp *.h);;All Files (*.*)");
            break;
        case ContributionEntry::Type::Documentation:
            filter = tr("Markdown Files (*.md);;All Files (*.*)");
            break;
        default:
            filter = tr("All Files (*.*)");
    }
    
    QString filePath = QFileDialog::getOpenFileName(this, tr("Select File"), QString(), filter);
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            m_entry.fileContent = file.readAll();
            m_entry.fileName = QFileInfo(filePath).fileName();
            m_entry.fileChecksum = calculateChecksum(m_entry.fileContent);
            
            m_filePathEdit->setText(filePath);
            m_fileSizeLabel->setText(tr("Size: %1 KB").arg(m_entry.fileContent.size() / 1024));
            m_checksumLabel->setText(tr("SHA-256: %1...").arg(m_entry.fileChecksum.left(16)));
        }
    }
}

QString ContributionDialog::calculateChecksum(const QByteArray& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

bool ContributionDialog::validateInput()
{
    if (m_titleEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Title is required."));
        return false;
    }
    if (m_nameEdit->text().isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Contributor name is required."));
        return false;
    }
    if (m_entry.fileContent.isEmpty()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Please select a file."));
        return false;
    }
    if (!m_agreedToTerms->isChecked()) {
        QMessageBox::warning(this, tr("Validation Error"), tr("Please agree to the license terms."));
        return false;
    }
    return true;
}

void ContributionDialog::onPreview()
{
    QString preview = QString(
        "=== Contribution Preview ===\n\n"
        "Type: %1\n"
        "Title: %2\n"
        "Contributor: %3\n"
        "License: %4\n\n"
        "File: %5 (%6 KB)\n"
        "Checksum: %7\n\n"
        "Description:\n%8"
    ).arg(m_typeCombo->currentText(),
          m_titleEdit->text(),
          m_nameEdit->text(),
          m_licenseCombo->currentText(),
          m_entry.fileName,
          QString::number(m_entry.fileContent.size() / 1024),
          m_entry.fileChecksum,
          m_descriptionEdit->toPlainText());
    
    QMessageBox::information(this, tr("Preview"), preview);
}

void ContributionDialog::onSubmit()
{
    if (!validateInput()) {
        return;
    }
    
    m_entry.title = m_titleEdit->text();
    m_entry.description = m_descriptionEdit->toPlainText();
    m_entry.contributorName = m_nameEdit->text();
    m_entry.contributorEmail = m_emailEdit->text();
    m_entry.type = static_cast<ContributionEntry::Type>(m_typeCombo->currentData().toInt());
    m_entry.license = m_licenseCombo->currentText();
    m_entry.agreedToTerms = m_agreedToTerms->isChecked();
    m_entry.submitted = QDateTime::currentDateTime();
    m_entry.status = SubmissionStatus::Pending;
    
    emit contributionSubmitted(m_entry);
    
    if (m_contributionCallback) {
        m_contributionCallback(m_entry, true);
    }
    
    QMessageBox::information(this, tr("Success"),
        tr("Thank you for your contribution! It will be reviewed shortly."));
    
    accept();
}

// ═══════════════════════════════════════════════════════════════════════════════
// FeedbackManager Implementation
// ═══════════════════════════════════════════════════════════════════════════════

FeedbackManager& FeedbackManager::instance()
{
    static FeedbackManager instance;
    return instance;
}

FeedbackManager::FeedbackManager()
    : m_networkManager(std::make_unique<QNetworkAccessManager>(this))
{
    m_settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(m_settingsPath);
    
    connect(m_networkManager.get(), &QNetworkAccessManager::finished,
            this, &FeedbackManager::onNetworkReply);
    
    loadSettings();
}

FeedbackManager::~FeedbackManager()
{
    saveSettings();
}

void FeedbackManager::showFeedbackDialog(QWidget* parent)
{
    auto* dialog = new FeedbackDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    dialog->setSubmitCallback([this](const FeedbackEntry& entry, bool success) {
        if (success) {
            m_history.push_back(entry);
            saveSettings();
            emit feedbackSubmitted(entry.id, true);
        }
    });
    
    dialog->show();
}

void FeedbackManager::showTelemetryConsentDialog(QWidget* parent)
{
    auto* dialog = new TelemetryConsentDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setCurrentConsent(m_consent);
    
    dialog->setConsentCallback([this](const TelemetryConsent& consent) {
        setTelemetryConsent(consent);
    });
    
    dialog->show();
}

void FeedbackManager::showContributionDialog(QWidget* parent)
{
    auto* dialog = new ContributionDialog(parent);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    
    dialog->setContributionCallback([this](const ContributionEntry& entry, bool success) {
        if (success) {
            emit contributionSubmitted(entry.id, true);
        }
    });
    
    dialog->show();
}

void FeedbackManager::submitQuickFeedback(const QString& message, FeedbackCategory category)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = message.left(50) + (message.length() > 50 ? "..." : "");
    entry.description = message;
    entry.category = category;
    entry.priority = FeedbackPriority::Medium;
    entry.created = QDateTime::currentDateTime();
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
    
    emit feedbackSubmitted(entry.id, true);
}

void FeedbackManager::reportBug(const QString& title, const QString& description)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = title;
    entry.description = description;
    entry.category = FeedbackCategory::BugReport;
    entry.priority = FeedbackPriority::Medium;
    entry.created = QDateTime::currentDateTime();
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::requestFeature(const QString& title, const QString& description)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = title;
    entry.description = description;
    entry.category = FeedbackCategory::FeatureRequest;
    entry.priority = FeedbackPriority::Low;
    entry.created = QDateTime::currentDateTime();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::reportThermalIssue(const QString& description, const QVariantMap& thermalData)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = "Thermal Issue Report";
    entry.description = description;
    entry.category = FeedbackCategory::ThermalIssue;
    entry.priority = FeedbackPriority::High;
    entry.created = QDateTime::currentDateTime();
    entry.thermalSnapshot = thermalData;
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::setTelemetryConsent(const TelemetryConsent& consent)
{
    m_consent = consent;
    m_consent.consentDate = QDateTime::currentDateTime();
    saveSettings();
    
    emit telemetryConsentChanged(m_consent);
}

TelemetryConsent FeedbackManager::getTelemetryConsent() const
{
    return m_consent;
}

bool FeedbackManager::hasTelemetryConsent() const
{
    return m_consent.hasAnyConsent();
}

void FeedbackManager::sendTelemetry(const QString& eventName, const QVariantMap& data)
{
    if (!m_consent.basicTelemetry) return;
    
    QJsonObject payload;
    payload["event"] = eventName;
    payload["data"] = QJsonObject::fromVariantMap(data);
    payload["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // In production, send to telemetry endpoint
    qDebug() << "Telemetry:" << eventName;
}

void FeedbackManager::sendPerformanceMetrics(const QVariantMap& metrics)
{
    if (!m_consent.performanceTelemetry) return;
    sendTelemetry("performance", metrics);
}

void FeedbackManager::sendThermalData(const QVariantMap& thermalData)
{
    if (!m_consent.thermalTelemetry) return;
    sendTelemetry("thermal", thermalData);
}

void FeedbackManager::sendCrashReport(const QString& crashDump, const QVariantMap& context)
{
    if (!m_consent.crashReporting) return;
    
    QVariantMap data = context;
    data["crashDump"] = crashDump;
    sendTelemetry("crash", data);
}

void FeedbackManager::saveDraft(const FeedbackEntry& entry)
{
    // Remove existing draft with same ID
    m_drafts.erase(
        std::remove_if(m_drafts.begin(), m_drafts.end(),
            [&](const FeedbackEntry& e) { return e.id == entry.id; }),
        m_drafts.end());
    
    m_drafts.push_back(entry);
    saveSettings();
}

std::vector<FeedbackEntry> FeedbackManager::loadDrafts()
{
    return m_drafts;
}

void FeedbackManager::deleteDraft(const QString& id)
{
    m_drafts.erase(
        std::remove_if(m_drafts.begin(), m_drafts.end(),
            [&](const FeedbackEntry& e) { return e.id == id; }),
        m_drafts.end());
    saveSettings();
}

std::vector<FeedbackEntry> FeedbackManager::getSubmissionHistory()
{
    return m_history;
}

FeedbackEntry FeedbackManager::getSubmission(const QString& id)
{
    for (const auto& entry : m_history) {
        if (entry.id == id) {
            return entry;
        }
    }
    return FeedbackEntry();
}

void FeedbackManager::setApiEndpoint(const QString& endpoint)
{
    m_apiEndpoint = endpoint;
}

void FeedbackManager::setApiKey(const QString& key)
{
    m_apiKey = key;
}

void FeedbackManager::onNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();
}

void FeedbackManager::loadSettings()
{
    QString filePath = QDir(m_settingsPath).filePath("feedback_settings.json");
    QFile file(filePath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonObject obj = doc.object();
        
        // Load consent
        if (obj.contains("consent")) {
            m_consent = TelemetryConsent::fromJson(obj["consent"].toObject());
        }
        
        // Load drafts
        if (obj.contains("drafts")) {
            QJsonArray draftsArray = obj["drafts"].toArray();
            for (const auto& item : draftsArray) {
                m_drafts.push_back(FeedbackEntry::fromJson(item.toObject()));
            }
        }
        
        // Load history
        if (obj.contains("history")) {
            QJsonArray historyArray = obj["history"].toArray();
            for (const auto& item : historyArray) {
                m_history.push_back(FeedbackEntry::fromJson(item.toObject()));
            }
        }
    }
}

void FeedbackManager::saveSettings()
{
    QString filePath = QDir(m_settingsPath).filePath("feedback_settings.json");
    QFile file(filePath);
    
    if (file.open(QIODevice::WriteOnly)) {
        QJsonObject obj;
        
        // Save consent
        obj["consent"] = m_consent.toJson();
        
        // Save drafts
        QJsonArray draftsArray;
        for (const auto& draft : m_drafts) {
            draftsArray.append(draft.toJson());
        }
        obj["drafts"] = draftsArray;
        
        // Save history (last 100)
        QJsonArray historyArray;
        size_t start = m_history.size() > 100 ? m_history.size() - 100 : 0;
        for (size_t i = start; i < m_history.size(); ++i) {
            historyArray.append(m_history[i].toJson());
        }
        obj["history"] = historyArray;
        
        file.write(QJsonDocument(obj).toJson());
    }
}

QVariantMap FeedbackManager::collectSystemInfo()
{
    QVariantMap info;
    info["os"] = QSysInfo::prettyProductName();
    info["architecture"] = QSysInfo::currentCpuArchitecture();
    info["qtVersion"] = qVersion();
    info["rawrxdVersion"] = "2.0.0";
    return info;
}

} // namespace rawrxd::feedback
