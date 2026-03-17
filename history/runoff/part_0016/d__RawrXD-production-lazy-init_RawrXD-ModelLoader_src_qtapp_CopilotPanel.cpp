#include "CopilotPanel.h"

namespace RawrXD {

CopilotPanel::CopilotPanel(QWidget* parent) : QWidget(parent) {
    setupUi();
}

CopilotPanel::~CopilotPanel() {
}

void CopilotPanel::setupUi() {
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setContentsMargins(5, 5, 5, 5);

    QLabel* title = new QLabel("Copilot Suggestions", this);
    title->setStyleSheet("font-weight: bold; color: #aaa; margin-bottom: 5px;");
    m_mainLayout->addWidget(title);

    m_suggestionList = new QListWidget(this);
    m_suggestionList->setStyleSheet("background-color: #252526; border: 1px solid #333;");
    m_mainLayout->addWidget(m_suggestionList);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_refactorButton = new QPushButton("Auto Refactor", this);
    m_testButton = new QPushButton("Generate Tests", this);
    
    QString btnStyle = "font-size: 11px; padding: 5px;";
    m_refactorButton->setStyleSheet(btnStyle);
    m_testButton->setStyleSheet(btnStyle);
    
    btnLayout->addWidget(m_refactorButton);
    btnLayout->addWidget(m_testButton);
    m_mainLayout->addLayout(btnLayout);

    connect(m_refactorButton, &QPushButton::clicked, this, [this](){ emit refactorSelected("auto"); });
    connect(m_testButton, &QPushButton::clicked, this, &CopilotPanel::generateTestsSelected);
}

void CopilotPanel::addSuggestion(const QString& title, const QString& type) {
    QListWidgetItem* item = new QListWidgetItem(title, m_suggestionList);
    item->setData(Qt::UserRole, type);
    m_suggestionList->addItem(item);
}

} // namespace RawrXD
