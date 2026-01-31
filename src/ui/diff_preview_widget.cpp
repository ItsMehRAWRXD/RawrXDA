// Diff Preview Widget - Implementation
// Production-ready with structured logging and error handling
#include "diff_preview_widget.h"


namespace RawrXD {

DiffPreviewWidget::DiffPreviewWidget(void* parent)
    : void(parent)
{
    setupUI();
}

void DiffPreviewWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Header section
    void* headerWidget = new void(this);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerWidget);
    headerLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* titleLabel = new QLabel("<h3>📋 Code Change Preview</h3>", this);
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
    QHBoxLayout* buttonLayout = new QHBoxLayout();
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
// Qt connect removed
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
// Qt connect removed
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
// Qt connect removed
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
// Qt connect removed
    buttonLayout->addWidget(m_acceptAllButton);
    
    mainLayout->addLayout(buttonLayout);
    
    setStyleSheet("void { background-color: #252526; }");
}

void DiffPreviewWidget::showDiff(const DiffChange& change) {
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
    std::string diffText = generateUnifiedDiff(m_currentDiff.originalContent, 
                                           m_currentDiff.proposedContent);
    
    // Apply syntax highlighting to diff
    std::string styledDiff = "<pre style='margin: 0; padding: 10px; font-family: Consolas, monospace;'>";
    
    std::vector<std::string> lines = diffText.split('\n');
    for (const std::string& line : lines) {
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
    
}

std::string DiffPreviewWidget::generateUnifiedDiff(const std::string& original, const std::string& proposed) {
    // Simple unified diff generator
    std::vector<std::string> originalLines = original.split('\n');
    std::vector<std::string> proposedLines = proposed.split('\n');
    
    std::string diff;
    diff += std::string("--- %1 (original)\n");
    diff += std::string("+++ %1 (proposed)\n");
    
    if (m_currentDiff.startLine > 0) {
        diff += std::string("@@ -%1,%2 +%3,%4 @@\n")
                
                )
                
                );
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
    
}

void DiffPreviewWidget::setAcceptCallback(std::function<void(const DiffChange&)> callback) {
    m_acceptCallback = callback;
}

void DiffPreviewWidget::setRejectCallback(std::function<void(const DiffChange&)> callback) {
    m_rejectCallback = callback;
}

void DiffPreviewWidget::onAcceptClicked() {
    if (!m_hasPendingDiff) return;
    
    
    diffAccepted(m_currentDiff);
    
    if (m_acceptCallback) {
        m_acceptCallback(m_currentDiff);
    }
    
    clear();
}

void DiffPreviewWidget::onRejectClicked() {
    if (!m_hasPendingDiff) return;
    
    
    diffRejected(m_currentDiff);
    
    if (m_rejectCallback) {
        m_rejectCallback(m_currentDiff);
    }
    
    clear();
}

void DiffPreviewWidget::onAcceptAllClicked() {
    onAcceptClicked();
    // Future: Queue multiple diffs and accept all
}

void DiffPreviewWidget::onRejectAllClicked() {
    onRejectClicked();
    // Future: Queue multiple diffs and reject all
}

} // namespace RawrXD

