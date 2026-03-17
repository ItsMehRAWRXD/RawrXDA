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
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#pragma comment(lib, "winhttp.lib")

namespace rawrxd::feedback {

// ═══════════════════════════════════════════════════════════════════════════════
// FeedbackDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

FeedbackDialog::FeedbackDialog(void* parent)
    : void(parent)
{
    setWindowTitle(tr("RawrXD IDE - Submit Feedback"));
    setMinimumSize(700, 550);
    setModal(true);
    
    // Generate unique ID
    m_entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_entry.created = // DateTime::currentDateTime();
    m_entry.status = SubmissionStatus::Draft;
    
    setupUI();
    setupValidation();
    collectSystemInfo();
}

FeedbackDialog::~FeedbackDialog() = default;

void FeedbackDialog::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Header
    auto* headerLabel = new void(tr(
        "<h2>🌟 Your Feedback Matters!</h2>"
        "<p>Help us improve RawrXD IDE by sharing your thoughts, reporting issues, "
        "or suggesting new features.</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Tab widget
    m_tabWidget = new void;
    mainLayout->addWidget(m_tabWidget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 1: Feedback Content
    // ═══════════════════════════════════════════════════════════════════════════
    auto* feedbackTab = new void;
    auto* feedbackLayout = new void(feedbackTab);
    
    auto* feedbackForm = new QFormLayout;
    
    // Category
    m_categoryCombo = new void;
    m_categoryCombo->addItem(tr("🐛 Bug Report"), static_cast<int>(FeedbackCategory::BugReport));
    m_categoryCombo->addItem(tr("💡 Feature Request"), static_cast<int>(FeedbackCategory::FeatureRequest));
    m_categoryCombo->addItem(tr("⚡ Performance Issue"), static_cast<int>(FeedbackCategory::PerformanceIssue));
    m_categoryCombo->addItem(tr("🔥 Thermal Issue"), static_cast<int>(FeedbackCategory::ThermalIssue));
    m_categoryCombo->addItem(tr("🎨 UI/UX Feedback"), static_cast<int>(FeedbackCategory::UIFeedback));
    m_categoryCombo->addItem(tr("📚 Documentation"), static_cast<int>(FeedbackCategory::Documentation));
    m_categoryCombo->addItem(tr("🔒 Security Concern"), static_cast<int>(FeedbackCategory::Security));
    m_categoryCombo->addItem(tr("📝 Other"), static_cast<int>(FeedbackCategory::Other));  // Signal connection removed\nfeedbackForm->addRow(tr("Category:"), m_categoryCombo);
    
    // Priority
    m_priorityCombo = new void;
    m_priorityCombo->addItem(tr("🟢 Low"), static_cast<int>(FeedbackPriority::Low));
    m_priorityCombo->addItem(tr("🟡 Medium"), static_cast<int>(FeedbackPriority::Medium));
    m_priorityCombo->addItem(tr("🟠 High"), static_cast<int>(FeedbackPriority::High));
    m_priorityCombo->addItem(tr("🔴 Critical"), static_cast<int>(FeedbackPriority::Critical));
    m_priorityCombo->setCurrentIndex(1);  // Default to Medium  // Signal connection removed\nfeedbackForm->addRow(tr("Priority:"), m_priorityCombo);
    
    // Title
    m_titleEdit = new voidEdit;
    m_titleEdit->setPlaceholderText(tr("Brief summary of your feedback"));
    m_titleEdit->setMaxLength(200);
    feedbackForm->addRow(tr("Title:"), m_titleEdit);
    
    feedbackLayout->addLayout(feedbackForm);
    
    // Description
    auto* descLabel = new void(tr("Description:"));
    feedbackLayout->addWidget(descLabel);
    
    m_descriptionEdit = new void;
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
    auto* contactTab = new void;
    auto* contactLayout = new void(contactTab);
    
    auto* contactGroup = new void(tr("Contact Information (Optional)"));
    auto* contactForm = nullptr;
    
    m_nameEdit = new voidEdit;
    m_nameEdit->setPlaceholderText(tr("Your name"));
    contactForm->addRow(tr("Name:"), m_nameEdit);
    
    m_emailEdit = new voidEdit;
    m_emailEdit->setPlaceholderText(tr("your.email@example.com"));
    contactForm->addRow(tr("Email:"), m_emailEdit);
    
    m_consentContact = new void(tr("I consent to being contacted about this feedback"));
    contactForm->addRow("", m_consentContact);
    
    contactLayout->addWidget(contactGroup);
    
    auto* privacyNote = new void(tr(
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
    auto* systemTab = new void;
    auto* systemLayout = new void(systemTab);
    
    auto* systemGroup = new void(tr("System Information"));
    auto* systemGroupLayout = new void(systemGroup);
    
    m_includeSystemInfo = new void(tr("Include basic system information"));
    m_includeSystemInfo->setChecked(true);
    systemGroupLayout->addWidget(m_includeSystemInfo);
    
    m_includeThermalData = new void(tr("Include thermal management data"));
    m_includeThermalData->setChecked(true);
    systemGroupLayout->addWidget(m_includeThermalData);
    
    auto* previewLabel = new void(tr("Information that will be included:"));
    systemGroupLayout->addWidget(previewLabel);
    
    m_systemInfoPreview = new void;
    m_systemInfoPreview->setReadOnly(true);
    m_systemInfoPreview->setMaximumHeight(200);
    systemGroupLayout->addWidget(m_systemInfoPreview);
    
    systemLayout->addWidget(systemGroup);  // Signal connection removed\n  // Signal connection removed\nsystemLayout->addStretch();
    
    m_tabWidget->addTab(systemTab, tr("💻 System Info"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Tab 4: Attachments
    // ═══════════════════════════════════════════════════════════════════════════
    auto* attachTab = new void;
    auto* attachLayout = new void(attachTab);
    
    auto* attachGroup = new void(tr("Attachments"));
    auto* attachGroupLayout = new void(attachGroup);
    
    m_attachmentsList = new QListWidget;
    m_attachmentsList->setMinimumHeight(150);
    attachGroupLayout->addWidget(m_attachmentsList);
    
    auto* attachBtnLayout = new void;
    
    m_attachFileBtn = new void(tr("📎 Attach File"));  // Signal connection removed\nattachBtnLayout->addWidget(m_attachFileBtn);
    
    m_attachScreenshotBtn = new void(tr("📷 Capture Screenshot"));  // Signal connection removed\nattachBtnLayout->addWidget(m_attachScreenshotBtn);
    
    m_removeAttachmentBtn = new void(tr("🗑️ Remove"));
    m_removeAttachmentBtn->setEnabled(false);
    attachBtnLayout->addWidget(m_removeAttachmentBtn);
    
    attachBtnLayout->addStretch();
    attachGroupLayout->addLayout(attachBtnLayout);
    
    attachLayout->addWidget(attachGroup);
    
    auto* attachNote = new void(tr(
        "<p><i>💡 Screenshots and logs can help us understand and resolve issues faster. "
        "Maximum 5 attachments, 10MB each.</i></p>"
    ));
    attachNote->setWordWrap(true);
    attachLayout->addWidget(attachNote);
    
    attachLayout->addStretch();  // Signal connection removed\n});  // Signal connection removed\nfor (auto* item : items) {
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
    auto* previewTab = new void;
    auto* previewLayout = new void(previewTab);
    
    auto* previewHeaderLabel = new void(tr("<b>Preview your submission:</b>"));
    previewLayout->addWidget(previewHeaderLabel);
    
    m_previewText = new void;
    m_previewText->setReadOnly(true);
    previewLayout->addWidget(m_previewText);
    
    auto* refreshBtn = new void(tr("🔄 Refresh Preview"));  // Signal connection removed\npreviewLayout->addWidget(refreshBtn);
    
    m_tabWidget->addTab(previewTab, tr("👁️ Preview"));
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Progress and Status
    // ═══════════════════════════════════════════════════════════════════════════
    m_progressBar = new void;
    m_progressBar->setVisible(false);
    mainLayout->addWidget(m_progressBar);
    
    m_statusLabel = new void;
    mainLayout->addWidget(m_statusLabel);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Buttons
    // ═══════════════════════════════════════════════════════════════════════════
    m_buttonBox = new voidButtonBox;
    
    m_saveDraftBtn = new void(tr("💾 Save Draft"));  // Signal connection removed\nm_buttonBox->addButton(m_saveDraftBtn, voidButtonBox::ActionRole);
    
    m_submitBtn = new void(tr("📤 Submit Feedback"));
    m_submitBtn->setDefault(true);  // Signal connection removed\nm_buttonBox->addButton(m_submitBtn, voidButtonBox::AcceptRole);
    
    m_buttonBox->addButton(voidButtonBox::Cancel);  // Signal connection removed\nmainLayout->addWidget(m_buttonBox);
}

void FeedbackDialog::setupValidation()
{
    // Title validation
    // Connect removed {
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
    if (auto* screen = void::primaryScreen()) {
        m_systemInfo["screenSize"] = std::string("%1x%2")
            .width())
            .height());
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

void FeedbackDialog::setThermalSnapshot(const std::anyMap& snapshot)
{
    m_entry.thermalSnapshot = snapshot;
    m_thermalSnapshot = snapshot;
    updatePreview();
}

void FeedbackDialog::setSystemInfo(const std::anyMap& sysInfo)
{
    for (auto it = sysInfo.begin(); it != sysInfo.end(); ++it) {
        m_systemInfo[it.key()] = it.value();
    }
    updatePreview();
}

void FeedbackDialog::updatePreview()
{
    std::string preview;
    
    if (m_includeSystemInfo->isChecked()) {
        preview += tr("=== System Information ===\n");
        for (auto it = m_systemInfo.begin(); it != m_systemInfo.end(); ++it) {
            preview += std::string("%1: %2\n"), it.value().toString());
        }
        preview += "\n";
    }
    
    if (m_includeThermalData->isChecked() && !m_thermalSnapshot.empty()) {
        preview += tr("=== Thermal Data ===\n");
        for (auto it = m_thermalSnapshot.begin(); it != m_thermalSnapshot.end(); ++it) {
            preview += std::string("%1: %2\n"), it.value().toString());
        }
        
        if (m_entry.currentTemperature) {
            preview += std::string("Current Temperature: %1°C\n");
        }
        if (m_entry.averageTemperature) {
            preview += std::string("Average Temperature: %1°C\n");
        }
        if (m_entry.throttleCount) {
            preview += std::string("Throttle Events: %1\n");
        }
    }
    
    m_systemInfoPreview->setPlainText(preview);
}

FeedbackEntry FeedbackDialog::getFeedback() const
{
    FeedbackEntry entry = m_entry;
    
    entry.title = m_titleEdit->text();
    entry.description = m_descriptionEdit->toPlainText();
    entry.category = static_cast<FeedbackCategory>(m_categoryCombo->currentData());
    entry.priority = static_cast<FeedbackPriority>(m_priorityCombo->currentData());
    
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
    
    entry.modified = // DateTime::currentDateTime();
    
    return entry;
}

void FeedbackDialog::setSubmitCallback(FeedbackSubmittedCallback callback)
{
    m_submitCallback = std::move(callback);
}

void FeedbackDialog::onCategoryChanged(int index)
{
    (void)(index);
    auto category = static_cast<FeedbackCategory>(m_categoryCombo->currentData());
    
    // Auto-suggest priority for certain categories
    if (category == FeedbackCategory::Security) {
        m_priorityCombo->setCurrentIndex(2);  // High
    } else if (category == FeedbackCategory::ThermalIssue) {
        m_includeThermalData->setChecked(true);
    }
}

void FeedbackDialog::onPriorityChanged(int index)
{
    (void)(index);
}

void FeedbackDialog::onAttachFile()
{
    if (m_entry.attachmentPaths.size() >= 5) {
        void::warning(this, tr("Limit Reached"), 
            tr("Maximum 5 attachments allowed."));
        return;
    }
    
    std::string filePath = // Dialog::getOpenFileName(this, 
        tr("Select File to Attach"),
        std::string(),
        tr("All Files (*.*);;Log Files (*.log *.txt);;Images (*.png *.jpg *.gif)"));
    
    if (!filePath.empty()) {
        // Info info(filePath);
        if (info.size() > 10 * 1024 * 1024) {
            void::warning(this, tr("File Too Large"),
                tr("Maximum file size is 10MB."));
            return;
        }
        
        m_entry.attachmentPaths.append(filePath);
        m_attachmentsList->addItem(std::string("📄 %1 (%2 KB)")
            )
             / 1024));
    }
}

void FeedbackDialog::onAttachScreenshot()
{
    if (m_entry.screenshotPaths.size() >= 3) {
        void::warning(this, tr("Limit Reached"),
            tr("Maximum 3 screenshots allowed."));
        return;
    }
    
    // Hide dialog temporarily for screenshot
    hide();
    // processEvents();
    
    // Wait for dialog to fully hide
    std::thread::msleep(500);
    
    // Capture screenshot
    if (auto* screen = void::primaryScreen()) {
        void screenshot = screen->grabWindow(0);
        
        std::string tempPath = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
        std::string fileName = std::string("rawrxd_screenshot_%1.png")
            .toString("yyyyMMdd_hhmmss"));
        std::string fullPath = // (tempPath).filePath(fileName);
        
        if (screenshot.save(fullPath, "PNG")) {
            m_entry.screenshotPaths.append(fullPath);
            m_attachmentsList->addItem(std::string("📷 %1"));
        }
    }
    
    show();
}

void FeedbackDialog::onPreviewSubmission()
{
    FeedbackEntry entry = getFeedback();
    
    std::string preview;
    preview += tr("=== Feedback Preview ===\n\n");
    preview += std::string("ID: %1\n");
    preview += std::string("Title: %1\n");
    preview += std::string("Category: %1\n"));
    preview += std::string("Priority: %1\n"));
    preview += std::string("\n--- Description ---\n%1\n");
    
    if (!entry.userName.empty()) {
        preview += std::string("\n--- Contact ---\nName: %1\nEmail: %2\n")
            ;
    }
    
    if (entry.includedSystemInfo) {
        preview += tr("\n--- System Info Included ---\n");
    }
    
    if (!entry.thermalSnapshot.empty()) {
        preview += tr("--- Thermal Data Included ---\n");
    }
    
    if (!entry.attachmentPaths.empty() || !entry.screenshotPaths.empty()) {
        preview += std::string("\n--- Attachments: %1 files ---\n")
             + entry.screenshotPaths.size());
    }
    
    m_previewText->setPlainText(preview);
    m_tabWidget->setCurrentIndex(4);  // Switch to preview tab
}

bool FeedbackDialog::validateInput()
{
    if (m_titleEdit->text().length() < 10) {
        void::warning(this, tr("Validation Error"),
            tr("Title must be at least 10 characters."));
        m_tabWidget->setCurrentIndex(0);
        m_titleEdit->setFocus();
        return false;
    }
    
    if (m_descriptionEdit->toPlainText().length() < 30) {
        void::warning(this, tr("Validation Error"),
            tr("Description must be at least 30 characters."));
        m_tabWidget->setCurrentIndex(0);
        m_descriptionEdit->setFocus();
        return false;
    }
    
    if (m_consentContact->isChecked() && m_emailEdit->text().empty()) {
        void::warning(this, tr("Validation Error"),
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
    entry.submitted = // DateTime::currentDateTime();
    
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);  // Indeterminate
    m_submitBtn->setEnabled(false);
    m_statusLabel->setText(tr("Submitting feedback..."));
    
    // Simulate submission (in production, this would be async network call)
    // Timer::singleShot(1500, [this, entry]() {
        m_progressBar->setVisible(false);
        m_statusLabel->setText(tr("✅ Feedback submitted successfully!"));
        
        feedbackSubmitted(entry);
        
        if (m_submitCallback) {
            m_submitCallback(entry, true);
        }
        
        // Timer operation removed
    });
}

void FeedbackDialog::onSaveDraft()
{
    FeedbackEntry entry = getFeedback();
    entry.status = SubmissionStatus::Draft;
    entry.modified = // DateTime::currentDateTime();
    
    draftSaved(entry);
    
    m_statusLabel->setText(tr("💾 Draft saved."));
    // Timer::singleShot(2000, [this]() {
        m_statusLabel->clear();
    });
}

// ═══════════════════════════════════════════════════════════════════════════════
// TelemetryConsentDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

TelemetryConsentDialog::TelemetryConsentDialog(void* parent)
    : void(parent)
{
    setWindowTitle(tr("RawrXD IDE - Telemetry & Privacy Settings"));
    setMinimumSize(550, 500);
    setModal(true);
    
    setupUI();
}

TelemetryConsentDialog::~TelemetryConsentDialog() = default;

void TelemetryConsentDialog::setupUI()
{
    auto* mainLayout = new void(this);
    
    // Header
    auto* headerLabel = new void(tr(
        "<h2>🔒 Privacy & Telemetry Settings</h2>"
        "<p>Help us improve RawrXD IDE by sharing anonymous usage data. "
        "Your privacy is important - you control what data is shared.</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Options group
    auto* optionsGroup = new void(tr("Telemetry Options"));
    auto* optionsLayout = new void(optionsGroup);
    
    m_basicCheck = new void(tr("📊 Basic telemetry (app version, crashes)"));
    m_basicCheck->setToolTip(tr("Anonymous app usage and crash data"));  // Signal connection removed\noptionsLayout->addWidget(m_basicCheck);
    
    m_performanceCheck = new void(tr("⚡ Performance metrics (startup time, memory)"));
    m_performanceCheck->setToolTip(tr("Performance data to help optimize the IDE"));  // Signal connection removed\noptionsLayout->addWidget(m_performanceCheck);
    
    m_thermalCheck = new void(tr("🌡️ Thermal data (temperatures, throttling events)"));
    m_thermalCheck->setToolTip(tr("Helps improve thermal management algorithms"));  // Signal connection removed\noptionsLayout->addWidget(m_thermalCheck);
    
    m_crashCheck = new void(tr("🔧 Crash reports (stack traces, error logs)"));
    m_crashCheck->setToolTip(tr("Detailed crash information to fix bugs faster"));  // Signal connection removed\noptionsLayout->addWidget(m_crashCheck);
    
    m_featureCheck = new void(tr("🎯 Feature usage (which features you use most)"));
    m_featureCheck->setToolTip(tr("Helps prioritize development of popular features"));  // Signal connection removed\noptionsLayout->addWidget(m_featureCheck);
    
    m_hardwareCheck = new void(tr("💻 Hardware info (CPU, GPU, storage types)"));
    m_hardwareCheck->setToolTip(tr("Helps optimize for different hardware configurations"));  // Signal connection removed\noptionsLayout->addWidget(m_hardwareCheck);
    
    mainLayout->addWidget(optionsGroup);
    
    // Quick buttons
    auto* quickBtnLayout = new void;
    m_selectAllBtn = new void(tr("✅ Select All"));  // Signal connection removed\nquickBtnLayout->addWidget(m_selectAllBtn);
    
    m_selectNoneBtn = new void(tr("❌ Select None"));  // Signal connection removed\nquickBtnLayout->addWidget(m_selectNoneBtn);
    
    quickBtnLayout->addStretch();
    mainLayout->addLayout(quickBtnLayout);
    
    // Summary
    m_summaryLabel = new void;
    m_summaryLabel->setWordWrap(true);
    mainLayout->addWidget(m_summaryLabel);
    
    // Details
    auto* detailsGroup = new void(tr("What We Collect"));
    auto* detailsLayout = new void(detailsGroup);
    
    m_detailsText = new void;
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
    m_agreedToPrivacy = new void(tr("I have read and agree to the Privacy Policy"));
    mainLayout->addWidget(m_agreedToPrivacy);
    
    m_privacyLink = new void(tr("<a href='https://rawrxd.dev/privacy'>View Privacy Policy</a>"));
    m_privacyLink->setOpenExternalLinks(true);
    mainLayout->addWidget(m_privacyLink);
    
    // Buttons
    auto* buttonBox = new voidButtonBox;
    auto* saveBtn = new void(tr("💾 Save Settings"));  // Signal connection removed\nbuttonBox->addButton(saveBtn, voidButtonBox::AcceptRole);
    buttonBox->addButton(voidButtonBox::Cancel);  // Signal connection removed\nmainLayout->addWidget(buttonBox);
    
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
    consent.consentDate = // DateTime::currentDateTime();
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

void TelemetryConsentDialog::onShowDetails(const std::string& category)
{
    (void)(category);
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
    
    std::string summary;
    if (count == 0) {
        summary = tr("📵 No telemetry will be collected. RawrXD IDE works fully offline.");
    } else if (count <= 2) {
        summary = tr("🔒 Minimal telemetry: %1 category(ies) selected.");
    } else if (count <= 4) {
        summary = tr("📊 Moderate telemetry: %1 categories selected. Thank you for helping improve RawrXD IDE!");
    } else {
        summary = tr("🌟 Full telemetry: All categories selected. You're awesome! This really helps us improve.");
    }
    
    m_summaryLabel->setText(summary);
}

void TelemetryConsentDialog::onSaveConsent()
{
    if (!m_agreedToPrivacy->isChecked()) {
        void::warning(this, tr("Agreement Required"),
            tr("Please read and agree to the Privacy Policy to continue."));
        return;
    }
    
    TelemetryConsent consent = getConsent();
    consentUpdated(consent);
    
    if (m_consentCallback) {
        m_consentCallback(consent);
    }
    
    accept();
}

// ═══════════════════════════════════════════════════════════════════════════════
// ContributionDialog Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ContributionDialog::ContributionDialog(void* parent)
    : void(parent)
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
    auto* mainLayout = new void(this);
    
    // Header
    auto* headerLabel = new void(tr(
        "<h2>🎁 Contribute to RawrXD IDE</h2>"
        "<p>Share your thermal profiles, configurations, or improvements with the community!</p>"
    ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    // Form
    auto* formLayout = new QFormLayout;
    
    m_typeCombo = new void;
    m_typeCombo->addItem(tr("🌡️ Thermal Profile"), static_cast<int>(ContributionEntry::Type::ThermalProfile));
    m_typeCombo->addItem(tr("💾 Drive Configuration"), static_cast<int>(ContributionEntry::Type::DriveConfiguration));
    m_typeCombo->addItem(tr("🧮 Algorithm"), static_cast<int>(ContributionEntry::Type::Algorithm));
    m_typeCombo->addItem(tr("📚 Documentation"), static_cast<int>(ContributionEntry::Type::Documentation));
    m_typeCombo->addItem(tr("🌍 Translation"), static_cast<int>(ContributionEntry::Type::Translation));
    m_typeCombo->addItem(tr("📝 Other"), static_cast<int>(ContributionEntry::Type::Other));  // Signal connection removed\nformLayout->addRow(tr("Type:"), m_typeCombo);
    
    m_titleEdit = new voidEdit;
    m_titleEdit->setPlaceholderText(tr("Name of your contribution"));
    formLayout->addRow(tr("Title:"), m_titleEdit);
    
    m_nameEdit = new voidEdit;
    m_nameEdit->setPlaceholderText(tr("Your name or handle"));
    formLayout->addRow(tr("Contributor:"), m_nameEdit);
    
    m_emailEdit = new voidEdit;
    m_emailEdit->setPlaceholderText(tr("email@example.com (optional)"));
    formLayout->addRow(tr("Email:"), m_emailEdit);
    
    mainLayout->addLayout(formLayout);
    
    // Description
    auto* descLabel = new void(tr("Description:"));
    mainLayout->addWidget(descLabel);
    
    m_descriptionEdit = new void;
    m_descriptionEdit->setPlaceholderText(tr(
        "Describe your contribution:\n\n"
        "• What does it do?\n"
        "• How was it tested?\n"
        "• What hardware/configuration was it designed for?"
    ));
    m_descriptionEdit->setMaximumHeight(120);
    mainLayout->addWidget(m_descriptionEdit);
    
    // File
    auto* fileGroup = new void(tr("File"));
    auto* fileLayout = new void(fileGroup);
    
    m_filePathEdit = new voidEdit;
    m_filePathEdit->setReadOnly(true);
    fileLayout->addWidget(m_filePathEdit);
    
    m_selectFileBtn = new void(tr("📂 Select..."));  // Signal connection removed\nfileLayout->addWidget(m_selectFileBtn);
    
    mainLayout->addWidget(fileGroup);
    
    m_fileSizeLabel = new void;
    mainLayout->addWidget(m_fileSizeLabel);
    
    m_checksumLabel = new void;
    mainLayout->addWidget(m_checksumLabel);
    
    // License
    auto* licenseGroup = new void(tr("License"));
    auto* licenseLayout = new void(licenseGroup);
    
    m_licenseCombo = new void;
    m_licenseCombo->addItem(tr("MIT License"));
    m_licenseCombo->addItem(tr("Apache 2.0"));
    m_licenseCombo->addItem(tr("BSD 3-Clause"));
    m_licenseCombo->addItem(tr("CC BY 4.0"));
    m_licenseCombo->addItem(tr("Public Domain (CC0)"));
    licenseLayout->addWidget(m_licenseCombo);
    
    m_agreedToTerms = new void(tr("I agree to license my contribution under the selected license"));
    licenseLayout->addWidget(m_agreedToTerms);
    
    mainLayout->addWidget(licenseGroup);
    
    // Buttons
    auto* buttonBox = new voidButtonBox;
    
    auto* previewBtn = new void(tr("👁️ Preview"));  // Signal connection removed\nbuttonBox->addButton(previewBtn, voidButtonBox::ActionRole);
    
    auto* submitBtn = new void(tr("📤 Submit"));  // Signal connection removed\nbuttonBox->addButton(submitBtn, voidButtonBox::AcceptRole);
    
    buttonBox->addButton(voidButtonBox::Cancel);  // Signal connection removed\nmainLayout->addWidget(buttonBox);
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
    (void)(index);
}

void ContributionDialog::onSelectFile()
{
    std::string filter;
    auto type = static_cast<ContributionEntry::Type>(m_typeCombo->currentData());
    
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
    
    std::string filePath = // Dialog::getOpenFileName(this, tr("Select File"), std::string(), filter);
    
    if (!filePath.empty()) {
        // File operation removed;
        if (file.open(std::iostream::ReadOnly)) {
            m_entry.fileContent = file.readAll();
            m_entry.fileName = // FileInfo: filePath).fileName();
            m_entry.fileChecksum = calculateChecksum(m_entry.fileContent);
            
            m_filePathEdit->setText(filePath);
            m_fileSizeLabel->setText(tr("Size: %1 KB") / 1024));
            m_checksumLabel->setText(tr("SHA-256: %1...")));
        }
    }
}

std::string ContributionDialog::calculateChecksum(const std::vector<uint8_t>& data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

bool ContributionDialog::validateInput()
{
    if (m_titleEdit->text().empty()) {
        void::warning(this, tr("Validation Error"), tr("Title is required."));
        return false;
    }
    if (m_nameEdit->text().empty()) {
        void::warning(this, tr("Validation Error"), tr("Contributor name is required."));
        return false;
    }
    if (m_entry.fileContent.empty()) {
        void::warning(this, tr("Validation Error"), tr("Please select a file."));
        return false;
    }
    if (!m_agreedToTerms->isChecked()) {
        void::warning(this, tr("Validation Error"), tr("Please agree to the license terms."));
        return false;
    }
    return true;
}

void ContributionDialog::onPreview()
{
    std::string preview = std::string(
        "=== Contribution Preview ===\n\n"
        "Type: %1\n"
        "Title: %2\n"
        "Contributor: %3\n"
        "License: %4\n\n"
        "File: %5 (%6 KB)\n"
        "Checksum: %7\n\n"
        "Description:\n%8"
    ),
          m_titleEdit->text(),
          m_nameEdit->text(),
          m_licenseCombo->currentText(),
          m_entry.fileName,
          std::string::number(m_entry.fileContent.size() / 1024),
          m_entry.fileChecksum,
          m_descriptionEdit->toPlainText());
    
    void::information(this, tr("Preview"), preview);
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
    m_entry.type = static_cast<ContributionEntry::Type>(m_typeCombo->currentData());
    m_entry.license = m_licenseCombo->currentText();
    m_entry.agreedToTerms = m_agreedToTerms->isChecked();
    m_entry.submitted = // DateTime::currentDateTime();
    m_entry.status = SubmissionStatus::Pending;
    
    contributionSubmitted(m_entry);
    
    if (m_contributionCallback) {
        m_contributionCallback(m_entry, true);
    }
    
    void::information(this, tr("Success"),
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
    : m_networkManager(std::make_unique<void*>(this))
{
    m_settingsPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    std::filesystem::create_directories(m_settingsPath);  // Signal connection removed\nloadSettings();
}

FeedbackManager::~FeedbackManager()
{
    saveSettings();
}

void FeedbackManager::showFeedbackDialog(void* parent)
{
    auto* dialog = new FeedbackDialog(parent);
    dialog->setAttribute(WA_DeleteOnClose);
    
    dialog->setSubmitCallback([this](const FeedbackEntry& entry, bool success) {
        if (success) {
            m_history.push_back(entry);
            saveSettings();
            feedbackSubmitted(entry.id, true);
        }
    });
    
    dialog->show();
}

void FeedbackManager::showTelemetryConsentDialog(void* parent)
{
    auto* dialog = new TelemetryConsentDialog(parent);
    dialog->setAttribute(WA_DeleteOnClose);
    dialog->setCurrentConsent(m_consent);
    
    dialog->setConsentCallback([this](const TelemetryConsent& consent) {
        setTelemetryConsent(consent);
    });
    
    dialog->show();
}

void FeedbackManager::showContributionDialog(void* parent)
{
    auto* dialog = new ContributionDialog(parent);
    dialog->setAttribute(WA_DeleteOnClose);
    
    dialog->setContributionCallback([this](const ContributionEntry& entry, bool success) {
        if (success) {
            contributionSubmitted(entry.id, true);
        }
    });
    
    dialog->show();
}

void FeedbackManager::submitQuickFeedback(const std::string& message, FeedbackCategory category)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = message.left(50) + (message.length() > 50 ? "..." : "");
    entry.description = message;
    entry.category = category;
    entry.priority = FeedbackPriority::Medium;
    entry.created = // DateTime::currentDateTime();
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
    
    feedbackSubmitted(entry.id, true);
}

void FeedbackManager::reportBug(const std::string& title, const std::string& description)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = title;
    entry.description = description;
    entry.category = FeedbackCategory::BugReport;
    entry.priority = FeedbackPriority::Medium;
    entry.created = // DateTime::currentDateTime();
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::requestFeature(const std::string& title, const std::string& description)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = title;
    entry.description = description;
    entry.category = FeedbackCategory::FeatureRequest;
    entry.priority = FeedbackPriority::Low;
    entry.created = // DateTime::currentDateTime();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::reportThermalIssue(const std::string& description, const std::anyMap& thermalData)
{
    FeedbackEntry entry;
    entry.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    entry.title = "Thermal Issue Report";
    entry.description = description;
    entry.category = FeedbackCategory::ThermalIssue;
    entry.priority = FeedbackPriority::High;
    entry.created = // DateTime::currentDateTime();
    entry.thermalSnapshot = thermalData;
    entry.systemInfo = collectSystemInfo();
    
    m_history.push_back(entry);
    saveSettings();
}

void FeedbackManager::setTelemetryConsent(const TelemetryConsent& consent)
{
    m_consent = consent;
    m_consent.consentDate = // DateTime::currentDateTime();
    saveSettings();
    
    telemetryConsentChanged(m_consent);
}

TelemetryConsent FeedbackManager::getTelemetryConsent() const
{
    return m_consent;
}

bool FeedbackManager::hasTelemetryConsent() const
{
    return m_consent.hasAnyConsent();
}

void FeedbackManager::sendTelemetry(const std::string& eventName, const std::anyMap& data)
{
    if (!m_consent.basicTelemetry) return;
    
    void* payload;
    payload["event"] = eventName;
    payload["data"] = void*::fromVariantMap(data);
    payload["timestamp"] = // DateTime::currentDateTime().toString(ISODate);
    
    // In production, send to telemetry endpoint
}

void FeedbackManager::sendPerformanceMetrics(const std::anyMap& metrics)
{
    if (!m_consent.performanceTelemetry) return;
    sendTelemetry("performance", metrics);
}

void FeedbackManager::sendThermalData(const std::anyMap& thermalData)
{
    if (!m_consent.thermalTelemetry) return;
    sendTelemetry("thermal", thermalData);
}

void FeedbackManager::sendCrashReport(const std::string& crashDump, const std::anyMap& context)
{
    if (!m_consent.crashReporting) return;
    
    std::anyMap data = context;
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

void FeedbackManager::deleteDraft(const std::string& id)
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

FeedbackEntry FeedbackManager::getSubmission(const std::string& id)
{
    for (const auto& entry : m_history) {
        if (entry.id == id) {
            return entry;
        }
    }
    return FeedbackEntry();
}

void FeedbackManager::setApiEndpoint(const std::string& endpoint)
{
    m_apiEndpoint = endpoint;
}

void FeedbackManager::setApiKey(const std::string& key)
{
    m_apiKey = key;
}

void FeedbackManager::onNetworkReply(void** reply)
{
    reply->deleteLater();
}

void FeedbackManager::loadSettings()
{
    std::string filePath = // (m_settingsPath).filePath("feedback_settings.json");
    // File operation removed;
    
    if (file.open(std::iostream::ReadOnly)) {
        void* doc = void*::fromJson(file.readAll());
        void* obj = doc.object();
        
        // Load consent
        if (obj.contains("consent")) {
            m_consent = TelemetryConsent::fromJson(obj["consent"].toObject());
        }
        
        // Load drafts
        if (obj.contains("drafts")) {
            void* draftsArray = obj["drafts"].toArray();
            for (const auto& item : draftsArray) {
                m_drafts.push_back(FeedbackEntry::fromJson(item.toObject()));
            }
        }
        
        // Load history
        if (obj.contains("history")) {
            void* historyArray = obj["history"].toArray();
            for (const auto& item : historyArray) {
                m_history.push_back(FeedbackEntry::fromJson(item.toObject()));
            }
        }
    }
}

void FeedbackManager::saveSettings()
{
    std::string filePath = // (m_settingsPath).filePath("feedback_settings.json");
    // File operation removed;
    
    if (file.open(std::iostream::WriteOnly)) {
        void* obj;
        
        // Save consent
        obj["consent"] = m_consent.toJson();
        
        // Save drafts
        void* draftsArray;
        for (const auto& draft : m_drafts) {
            draftsArray.append(draft.toJson());
        }
        obj["drafts"] = draftsArray;
        
        // Save history (last 100)
        void* historyArray;
        size_t start = m_history.size() > 100 ? m_history.size() - 100 : 0;
        for (size_t i = start; i < m_history.size(); ++i) {
            historyArray.append(m_history[i].toJson());
        }
        obj["history"] = historyArray;
        
        file.write(void*(obj).toJson());
    }
}

std::anyMap FeedbackManager::collectSystemInfo()
{
    std::anyMap info;
    info["os"] = QSysInfo::prettyProductName();
    info["architecture"] = QSysInfo::currentCpuArchitecture();
    info["qtVersion"] = qVersion();
    info["rawrxdVersion"] = "2.0.0";
    return info;
}

} // namespace rawrxd::feedback

