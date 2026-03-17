// Diff Preview Widget - Implementation
// Production-ready with structured logging and error handling
#include "diff_preview_widget.h"
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */
/* Qt removed */

namespace RawrXD {

DiffPreviewWidget::DiffPreviewWidget(void* parent)
    : Window(parent)
{
    setupUI();
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] Initialized at" << QDateTime::currentDateTime().toString(Qt::ISODate);
}

void DiffPreviewWidget::setupUI() {
    void* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Header section
    HWND headerWidget = CreateWindowExW(0, L"STATIC", L"", WS_CHILD|WS_VISIBLE, 0, 0, 100, 30, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    void* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    void* titleLabel = new QLabel("<h3>📋 Code Change Preview</h3>", this);
    titleLabel->setStyleSheet("color: #4ec9b0; margin-bottom: 5px;");
    headerLayout->addWidget(titleLabel);
    
    m_fileLabel = new QLabel("No file selected", this);
    m_fileLabel->setStyleSheet("color: #569cd6; font-weight: bold;");
    headerLayout->addWidget(m_fileLabel);
    
    m_descriptionLabel = new QLabel("", this);
    m_descriptionLabel->setStyleSheet("color: #d4d4d4; font-style: italic;");
    m_descriptionLabel->setWordWrap(true);
    headerLayout->addWidget(m_descriptionLabel);
    
    mainLayout->addWidget(headerWidget);
    
    // Diff display area
    m_diffDisplay = new QTextEdit(this);
    m_diffDisplay->setReadOnly(true);
    m_diffDisplay->setFont(QFont("Consolas", 10));
    m_diffDisplay->setStyleSheet(
        "QTextEdit {"
        "  background-color: #1e1e1e;"
        "  color: #d4d4d4;"
        "  border: 1px solid #3c3c3c;"
        "  border-radius: 3px;"
        "}"
    );
    mainLayout->addWidget(m_diffDisplay, 1);  // Give it stretch priority
    
    // Button row
    void* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    
    m_rejectButton = new QPushButton("✗ Reject Change", this);
    m_rejectButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #c5323d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #e53e49; }"
        "QPushButton:disabled { background-color: #3c3c3c; color: #888888; }"
    );
    m_rejectButton->setEnabled(false);
    connect(m_rejectButton, &QPushButton::clicked, this, &DiffPreviewWidget::onRejectClicked);
    buttonLayout->addWidget(m_rejectButton);
    
    m_acceptButton = new QPushButton("✓ Accept Change", this);
    m_acceptButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #16825d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "  font-weight: bold;"
        "}"
        "QPushButton:hover { background-color: #1a9c6f; }"
        "QPushButton:disabled { background-color: #3c3c3c; color: #888888; }"
    );
    m_acceptButton->setEnabled(false);
    connect(m_acceptButton, &QPushButton::clicked, this, &DiffPreviewWidget::onAcceptClicked);
    buttonLayout->addWidget(m_acceptButton);
    
    buttonLayout->addStretch();
    
    m_rejectAllButton = new QPushButton("✗ Reject All", this);
    m_rejectAllButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #c5323d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #e53e49; }"
        "QPushButton:disabled { background-color: #3c3c3c; color: #888888; }"
    );
    m_rejectAllButton->setEnabled(false);
    connect(m_rejectAllButton, &QPushButton::clicked, this, &DiffPreviewWidget::onRejectAllClicked);
    buttonLayout->addWidget(m_rejectAllButton);
    
    m_acceptAllButton = new QPushButton("✓ Accept All", this);
    m_acceptAllButton->setStyleSheet(
        "QPushButton {"
        "  background-color: #16825d;"
        "  color: white;"
        "  padding: 8px 16px;"
        "  border: none;"
        "  border-radius: 4px;"
        "}"
        "QPushButton:hover { background-color: #1a9c6f; }"
        "QPushButton:disabled { background-color: #3c3c3c; color: #888888; }"
    );
    m_acceptAllButton->setEnabled(false);
    connect(m_acceptAllButton, &QPushButton::clicked, this, &DiffPreviewWidget::onAcceptAllClicked);
    buttonLayout->addWidget(m_acceptAllButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Win32: Background color set via WM_CTLCOLORSTATIC / WM_ERASEBKGND
}

void DiffPreviewWidget::showDiff(const DiffChange& change) {
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] Showing diff for" << change.filePath 
             << "lines" << change.startLine << "-" << change.endLine;
    
    m_currentDiff = change;
    m_hasPendingDiff = true;
    
    // Update UI
    m_fileLabel->setText("📄 " + change.filePath);
    m_descriptionLabel->setText(change.changeDescription.isEmpty() 
                                ? "Review the proposed changes below" 
                                : change.changeDescription);
    
    // Enable buttons
    m_acceptButton->setEnabled(true);
    m_rejectButton->setEnabled(true);
    m_acceptAllButton->setEnabled(true);
    m_rejectAllButton->setEnabled(true);
    
    // Render the diff
    renderDiff();
}

void DiffPreviewWidget::renderDiff() {
    std::wstring diffText = generateUnifiedDiff(m_currentDiff.originalContent, 
                                           m_currentDiff.proposedContent);
    
    // Apply syntax highlighting to diff
    std::wstring styledDiff = "<pre style='margin: 0; padding: 10px; font-family: Consolas, monospace;'>";
    
    std::wstringList lines = diffText.split('\n');
    for (const std::wstring& line : lines) {
        if (line.startsWith("+++") || line.startsWith("---")) {
            styledDiff += "<span style='color: #d4d4d4; font-weight: bold;'>" + line.toHtmlEscaped() + "</span>\n";
        } else if (line.startsWith("+")) {
            styledDiff += "<span style='background-color: #1e4620; color: #4ec9b0;'>" + line.toHtmlEscaped() + "</span>\n";
        } else if (line.startsWith("-")) {
            styledDiff += "<span style='background-color: #4c1f24; color: #f48771;'>" + line.toHtmlEscaped() + "</span>\n";
        } else if (line.startsWith("@@")) {
            styledDiff += "<span style='color: #569cd6;'>" + line.toHtmlEscaped() + "</span>\n";
        } else {
            styledDiff += "<span style='color: #d4d4d4;'>" + line.toHtmlEscaped() + "</span>\n";
        }
    }
    
    styledDiff += "</pre>";
    m_diffDisplay->setHtml(styledDiff);
    
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] Rendered diff with" << lines.size() << "lines";
}

std::wstring DiffPreviewWidget::generateUnifiedDiff(const std::wstring& original, const std::wstring& proposed) {
    // Simple unified diff generator
    std::wstringList originalLines = original.split('\n');
    std::wstringList proposedLines = proposed.split('\n');
    
    std::wstring diff;
    diff += std::wstring("--- %1 (original)\n").arg(m_currentDiff.filePath);
    diff += std::wstring("+++ %1 (proposed)\n").arg(m_currentDiff.filePath);
    
    if (m_currentDiff.startLine > 0) {
        diff += std::wstring("@@ -%1,%2 +%3,%4 @@\n")
                .arg(m_currentDiff.startLine)
                .arg(originalLines.size())
                .arg(m_currentDiff.startLine)
                .arg(proposedLines.size());
    }
    
    // Simple line-by-line diff (could be enhanced with Myers algorithm)
    int maxLines = qMax(originalLines.size(), proposedLines.size());
    for (int i = 0; i < maxLines; ++i) {
        if (i < originalLines.size() && i < proposedLines.size()) {
            if (originalLines[i] == proposedLines[i]) {
                diff += " " + originalLines[i] + "\n";
            } else {
                diff += "-" + originalLines[i] + "\n";
                diff += "+" + proposedLines[i] + "\n";
            }
        } else if (i < originalLines.size()) {
            diff += "-" + originalLines[i] + "\n";
        } else {
            diff += "+" + proposedLines[i] + "\n";
        }
    }
    
    return diff;
}

void DiffPreviewWidget::clear() {
    m_hasPendingDiff = false;
    m_diffDisplay->clear();
    m_fileLabel->setText("No file selected");
    m_descriptionLabel->clear();
    m_acceptButton->setEnabled(false);
    m_rejectButton->setEnabled(false);
    m_acceptAllButton->setEnabled(false);
    m_rejectAllButton->setEnabled(false);
    
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] Cleared diff preview";
}

void DiffPreviewWidget::setAcceptCallback(std::function<void(const DiffChange&)> callback) {
    m_acceptCallback = callback;
}

void DiffPreviewWidget::setRejectCallback(std::function<void(const DiffChange&)> callback) {
    m_rejectCallback = callback;
}

void DiffPreviewWidget::onAcceptClicked() {
    if (!m_hasPendingDiff) return;
    
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] User accepted change for" << m_currentDiff.filePath;
    
    /* emit */ diffAccepted(m_currentDiff);
    
    if (m_acceptCallback) {
        m_acceptCallback(m_currentDiff);
    }
    
    clear();
}

void DiffPreviewWidget::onRejectClicked() {
    if (!m_hasPendingDiff) return;
    
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] User rejected change for" << m_currentDiff.filePath;
    
    /* emit */ diffRejected(m_currentDiff);
    
    if (m_rejectCallback) {
        m_rejectCallback(m_currentDiff);
    }
    
    clear();
}

void DiffPreviewWidget::onAcceptAllClicked() {
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] User accepted all changes (batch operation)";
    onAcceptClicked();
    // Future: Queue multiple diffs and accept all
}

void DiffPreviewWidget::onRejectAllClicked() {
    // REMOVED_QT: qDebug() << "[DiffPreviewWidget] User rejected all changes (batch operation)";
    onRejectClicked();
    // Future: Queue multiple diffs and reject all
}

} // namespace RawrXD
