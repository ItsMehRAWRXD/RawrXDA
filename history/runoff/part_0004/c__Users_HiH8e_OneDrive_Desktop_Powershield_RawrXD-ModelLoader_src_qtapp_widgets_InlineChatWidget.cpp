#include "InlineChatWidget.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QKeyEvent>
#include <QLabel>
#include <QApplication>
#include <QStyleHints>

InlineChatWidget::InlineChatWidget(QWidget* parent)
    : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
{
    setupUi();
    setWindowOpacity(0.95);
    installEventFilter(this);
    
    // Sync palette with system theme
    connect(qApp, &QApplication::paletteChanged, this, &InlineChatWidget::syncPalette);
    syncPalette();
}

InlineChatWidget::~InlineChatWidget() = default;

void InlineChatWidget::setupUi()
{
    setStyleSheet(R"(
        QWidget {
            background: #2d2d2d;
            border: 2px solid #0078d4;
            border-radius: 4px;
        }
        QLineEdit {
            background: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #555;
            padding: 4px;
            font-size: 12px;
        }
        QTextEdit {
            background: #1e1e1e;
            color: #d4d4d4;
            border: 1px solid #555;
            font-family: 'Consolas', monospace;
            font-size: 11px;
        }
        QPushButton {
            background: #0078d4;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 3px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #005a9e;
        }
        QPushButton:disabled {
            background: #555;
            color: #888;
        }
    )");
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);
    
    // Title
    QLabel* title = new QLabel(tr("🤖 AI Quick Fix"), this);
    title->setStyleSheet("font-weight: bold; font-size: 13px; color: #0078d4; border: none;");
    
    // Prompt input
    promptInput_ = new QLineEdit(this);
    promptInput_->setPlaceholderText(tr("Describe the fix or press Enter to use suggestion..."));
    connect(promptInput_, &QLineEdit::returnPressed, this, &InlineChatWidget::handleSubmit);
    
    // Preview area
    previewEdit_ = new QTextEdit(this);
    previewEdit_->setReadOnly(true);
    previewEdit_->setPlaceholderText(tr("AI-generated fix will appear here..."));
    previewEdit_->setMinimumHeight(150);
    previewEdit_->setMaximumHeight(300);
    
    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    submitBtn_ = new QPushButton(tr("✨ Generate Fix"), this);
    acceptBtn_ = new QPushButton(tr("✓ Accept"), this);
    rejectBtn_ = new QPushButton(tr("✗ Reject"), this);
    
    connect(submitBtn_, &QPushButton::clicked, this, &InlineChatWidget::handleSubmit);
    connect(acceptBtn_, &QPushButton::clicked, this, &InlineChatWidget::handleAccept);
    connect(rejectBtn_, &QPushButton::clicked, this, &InlineChatWidget::handleReject);
    
    acceptBtn_->setEnabled(false);
    
    btnLayout->addWidget(submitBtn_);
    btnLayout->addStretch();
    btnLayout->addWidget(acceptBtn_);
    btnLayout->addWidget(rejectBtn_);
    
    mainLayout->addWidget(title);
    mainLayout->addWidget(promptInput_);
    mainLayout->addWidget(previewEdit_, 1);
    mainLayout->addLayout(btnLayout);
    
    setFixedWidth(500);
    setMinimumHeight(250);
}

void InlineChatWidget::setPrompt(const QString& text)
{
    promptInput_->setText(text);
}

QString InlineChatWidget::prompt() const
{
    return promptInput_->text();
}

void InlineChatWidget::setPreview(const QString& diff)
{
    previewEdit_->setPlainText(diff);
    acceptBtn_->setEnabled(!diff.isEmpty());
}

void InlineChatWidget::clear()
{
    promptInput_->clear();
    previewEdit_->clear();
    accumulatedResponse_.clear();
    acceptBtn_->setEnabled(false);
}

void InlineChatWidget::show()
{
    QWidget::show();
    promptInput_->setFocus();
    promptInput_->selectAll();
}

void InlineChatWidget::hide()
{
    QWidget::hide();
    clear();
}

void InlineChatWidget::appendResponse(const QString& chunk)
{
    accumulatedResponse_ += chunk;
    previewEdit_->setPlainText(accumulatedResponse_);
    acceptBtn_->setEnabled(true);
    
    // Auto-scroll to bottom
    QTextCursor cursor = previewEdit_->textCursor();
    cursor.movePosition(QTextCursor::End);
    previewEdit_->setTextCursor(cursor);
}

void InlineChatWidget::handleAccept()
{
    emit accepted(accumulatedResponse_);
    hide();
}

void InlineChatWidget::handleReject()
{
    emit rejected();
    hide();
}

void InlineChatWidget::handleSubmit()
{
    const QString p = promptInput_->text();
    if (p.isEmpty()) return;
    
    previewEdit_->clear();
    accumulatedResponse_.clear();
    acceptBtn_->setEnabled(false);
    submitBtn_->setEnabled(false);
    
    emit promptSubmitted(p);
}

bool InlineChatWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            handleReject();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void InlineChatWidget::syncPalette()
{
    const bool dark = (qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark);
    
    const QString bgColor = dark ? "#2d2d2d" : "#f5f5f5";
    const QString textColor = dark ? "#d4d4d4" : "#1e1e1e";
    const QString borderColor = dark ? "#0078d4" : "#0066cc";
    const QString inputBg = dark ? "#1e1e1e" : "#ffffff";
    const QString btnBg = dark ? "#0078d4" : "#0066cc";
    const QString btnHover = dark ? "#005a9e" : "#004d99";
    
    setStyleSheet(QString(R"(
        QWidget {
            background: %1;
            border: 2px solid %2;
            border-radius: 4px;
        }
        QLineEdit, QTextEdit {
            background: %3;
            color: %4;
            border: 1px solid #555;
            padding: 4px;
        }
        QPushButton {
            background: %5;
            color: white;
            border: none;
            padding: 6px 12px;
            border-radius: 3px;
        }
        QPushButton:hover {
            background: %6;
        }
    )").arg(bgColor, borderColor, inputBg, textColor, btnBg, btnHover));
}
