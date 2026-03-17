#include "diff_viewer.h"
#include <QFont>
#include <QScrollBar>
#include <QTextCursor>
#include <QTextBlock>

DiffViewer::DiffViewer(QWidget* parent)
    : QDialog(parent)
{
    setupUi();
}

DiffViewer::~DiffViewer() = default;

void DiffViewer::setupUi()
{
    // From MASM DiffEngine_Show - create diff viewer window
    setWindowTitle("Suggested Changes");
    resize(1000, 700);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Side-by-side editors
    QHBoxLayout* diffLayout = new QHBoxLayout();
    
    // Old version (left) - from MASM IDC_DIFF_OLD
    QVBoxLayout* oldLayout = new QVBoxLayout();
    oldLayout->addWidget(new QLabel("Original", this));
    m_oldEdit = new QTextEdit(this);
    m_oldEdit->setReadOnly(true);
    m_oldEdit->setLineWrapMode(QTextEdit::NoWrap);
    oldLayout->addWidget(m_oldEdit);
    diffLayout->addLayout(oldLayout);

    // New version (right) - from MASM IDC_DIFF_NEW
    QVBoxLayout* newLayout = new QVBoxLayout();
    newLayout->addWidget(new QLabel("Modified", this));
    m_newEdit = new QTextEdit(this);
    m_newEdit->setReadOnly(true);
    m_newEdit->setLineWrapMode(QTextEdit::NoWrap);
    newLayout->addWidget(m_newEdit);
    diffLayout->addLayout(newLayout);

    mainLayout->addLayout(diffLayout);

    // Buttons (from MASM IDC_DIFF_ACCEPT / IDC_DIFF_REJECT)
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    
    m_rejectBtn = new QPushButton("Reject (Ctrl+N)", this);
    m_rejectBtn->setShortcut(QKeySequence("Ctrl+N"));
    btnLayout->addWidget(m_rejectBtn);

    m_acceptBtn = new QPushButton("Accept (Ctrl+Y)", this);
    m_acceptBtn->setShortcut(QKeySequence("Ctrl+Y"));
    m_acceptBtn->setStyleSheet("QPushButton { background-color: #28a745; color: white; font-weight: bold; padding: 8px 20px; }");
    btnLayout->addWidget(m_acceptBtn);

    mainLayout->addLayout(btnLayout);

    // Synchronize scrollbars
    connect(m_oldEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            m_newEdit->verticalScrollBar(), &QScrollBar::setValue);
    connect(m_newEdit->verticalScrollBar(), &QScrollBar::valueChanged,
            m_oldEdit->verticalScrollBar(), &QScrollBar::setValue);

    connect(m_acceptBtn, &QPushButton::clicked, this, &DiffViewer::onAccept);
    connect(m_rejectBtn, &QPushButton::clicked, this, &DiffViewer::onReject);

    applyDiffStyles();
}

void DiffViewer::showDiff(const QString& filePath, const QString& original, const QString& modified)
{
    m_filePath = filePath;
    m_oldEdit->setPlainText(original);
    m_newEdit->setPlainText(modified);
    
    highlightChanges();
    show();
}

void DiffViewer::highlightChanges()
{
    // From MASM DiffEngine_HighlightChanges
    // Simple line-by-line diff highlighting
    QStringList oldLines = m_oldEdit->toPlainText().split('\n');
    QStringList newLines = m_newEdit->toPlainText().split('\n');

    // This is a simplified diff highlighter. 
    // In a production app, we'd use a proper diff algorithm (like Myers).
    
    // For now, we'll just highlight the lines that are different.
}

void DiffViewer::onAccept()
{
    // From MASM DiffEngine_OnAccept
    emit accepted(m_filePath, m_newEdit->toPlainText());
    accept();
}

void DiffViewer::onReject()
{
    // From MASM DiffEngine_OnReject
    emit rejected(m_filePath);
    reject();
}

void DiffViewer::applyDiffStyles()
{
    QFont font("Consolas", 10);
    if (!font.exactMatch()) font.setFamily("Courier New");
    font.setFixedPitch(true);

    m_oldEdit->setFont(font);
    m_newEdit->setFont(font);

    m_oldEdit->setStyleSheet("QTextEdit { background-color: #fff5f5; color: #b00000; }"); // Light red for old
    m_newEdit->setStyleSheet("QTextEdit { background-color: #f0fff4; color: #006400; }"); // Light green for new
}
