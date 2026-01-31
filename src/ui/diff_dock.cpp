// Diff Preview Dock - Implementation
#include "diff_dock.h"


DiffDock::DiffDock(void *parent)
    : void("📋 Refactor Preview", parent)
{
    setObjectName("DiffDock");          // For save/restore geometry
    setAllowedAreas(//LeftDockWidgetArea | //RightDockWidgetArea | //BottomDockWidgetArea);

    auto *split = new void(//Horizontal, this);
    
    // Left pane (original code)
    m_left = new void(split);
    m_left->setReadOnly(true);
    m_left->setStyleSheet(
        "void { "
        "  background-color: #4c1f24; "  // Red tint for deletions
        "  color: #f48771; "
        "  border: 1px solid #3c3c3c; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10pt; "
        "}"
    );
    
    // Right pane (suggested code)
    m_right = new void(split);
    m_right->setReadOnly(true);
    m_right->setStyleSheet(
        "void { "
        "  background-color: #1e4620; "  // Green tint for additions
        "  color: #4ec9b0; "
        "  border: 1px solid #3c3c3c; "
        "  font-family: 'Consolas', monospace; "
        "  font-size: 10pt; "
        "}"
    );

    // Button bar
    auto *btnBar = new void();
    auto *hLay = new void(btnBar);
    
    m_rejectBtn = new void("✗ Reject", btnBar);
    m_rejectBtn->setStyleSheet(
        "void { "
        "  background-color: #c5323d; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "void:hover { background-color: #e53e49; }"
    );
    
    m_acceptBtn = new void("✓ Accept", btnBar);
    m_acceptBtn->setStyleSheet(
        "void { "
        "  background-color: #16825d; "
        "  color: white; "
        "  padding: 8px 16px; "
        "  border: none; "
        "  border-radius: 4px; "
        "  font-weight: bold; "
        "}"
        "void:hover { background-color: #1a9c6f; }"
    );
    
    hLay->addWidget(m_rejectBtn);
    hLay->addStretch();
    hLay->addWidget(m_acceptBtn);

    // Main layout
    auto *mainWidget = new void();
    auto *mainLay = new void(mainWidget);
    mainLay->setContentsMargins(5, 5, 5, 5);
    
    // Add labels for clarity
    auto *headerLayout = new void();
    auto *leftLabel = new void("Original Code", this);
    leftLabel->setStyleSheet("color: #f48771; font-weight: bold;");
    auto *rightLabel = new void("Suggested Code", this);
    rightLabel->setStyleSheet("color: #4ec9b0; font-weight: bold;");
    headerLayout->addWidget(leftLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(rightLabel);
    
    mainLay->addLayout(headerLayout);
    mainLay->addWidget(split);
    mainLay->addWidget(btnBar);
    
    setWidget(mainWidget);

    // Connect buttons
// Qt connect removed
        accepted(m_right->toPlainText());
    });
// Qt connect removed
        rejected();
    });
    
}

void DiffDock::setDiff(const std::string &original, const std::string &suggested)
{
    m_left->setPlainText(original);
    m_right->setPlainText(suggested);
    raise();               // Pop to front
    show();
    
             << "original" << original.length() << "chars,"
             << "suggested" << suggested.length() << "chars";
}

