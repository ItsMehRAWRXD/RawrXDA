// Diff Preview Dock - Implementation
#include "diff_dock.h"
#include <QTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QLabel>
#include <QDebug>
#include <QDateTime>

DiffDock::DiffDock(QWidget *parent)
    : QDockWidget("📋 Refactor Preview", parent)
{
    setObjectName("DiffDock");          // For save/restore geometry
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);

    auto *split = new QSplitter(Qt::Horizontal, this);
    
    // Left pane (original code)
    m_left = new QTextEdit(split);
    m_left->setReadOnly(true);
    m_left->setStyleSheet(
        "QTextEdit { "
        "  background-color: #4c1f24; "  // Red tint for deletions
        "  color: #f48771; "
        "  border: 1px solid #3c3c3c; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10pt; "
        "}"
    );
    
    // Right pane (suggested code)
    m_right = new QTextEdit(split);
    m_right->setReadOnly(true);
    m_right->setStyleSheet(
        "QTextEdit { "
        "  background-color: #1e4620; "  // Green tint for additions
        "  color: #4ec9b0; "
        "  border: 1px solid #3c3c3c; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10pt; "
        "}"
    );

    // Button bar
    auto *btnBar = new QWidget();
    auto *hLay = new QHBoxLayout(btnBar);
    
    m_rejectBtn = new QPushButton("✗ Reject", btnBar);
    m_rejectBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #c5323d; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #e53e49; }"
    );
    
    m_acceptBtn = new QPushButton("✓ Accept", btnBar);
    m_acceptBtn->setStyleSheet(
        "QPushButton { "
        "  background-color: #16825d; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "QPushButton:hover { background-color: #1a9c6f; }"
    );
    
    hLay->addWidget(m_rejectBtn);
    hLay->addStretch();
    hLay->addWidget(m_acceptBtn);

    // Main layout
    auto *mainWidget = new QWidget();
    auto *mainLay = new QVBoxLayout(mainWidget);
    mainLay->setContentsMargins(5, 5, 5, 5);
    
    // Add labels for clarity
    auto *headerLayout = new QHBoxLayout();
    auto *leftLabel = new QLabel("Original Code", this);
    leftLabel->setStyleSheet("color: #f48771; font-weight: bold;");
    auto *rightLabel = new QLabel("Suggested Code", this);
    rightLabel->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    headerLayout->addWidget(leftLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(rightLabel);
    
    mainLay->addLayout(headerLayout);
    mainLay->addWidget(split);
    mainLay->addWidget(btnBar);
    
    setWidget(mainWidget);

    // Connect buttons
    connect(m_acceptBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[DiffDock] User accepted refactor at" << QDateTime::currentDateTime().toString(Qt::ISODate);
        emit accepted(m_right->toPlainText());
    });
    
    connect(m_rejectBtn, &QPushButton::clicked, this, [this]() {
        qDebug() << "[DiffDock] User rejected refactor at" << QDateTime::currentDateTime().toString(Qt::ISODate);
        emit rejected();
    });
    
    qDebug() << "[DiffDock] Initialized";
}

void DiffDock::setDiff(const QString &original, const QString &suggested)
{
    m_left->setPlainText(original);
    m_right->setPlainText(suggested);
    raise();               // Pop to front
    show();
    
    qDebug() << "[DiffDock] Showing diff:"
             << "original" << original.length() << "chars,"
             << "suggested" << suggested.length() << "chars";
}
