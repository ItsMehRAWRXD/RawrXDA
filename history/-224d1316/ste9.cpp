#include "welcome_screen_widget.h"

WelcomeScreenWidget::WelcomeScreenWidget(QWidget* parent) : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Header
    QLabel* title = new QLabel("Welcome to RawrXD IDE", this);
    title->setStyleSheet("font-size: 24px; font-weight: bold; color: #ffffff; margin-bottom: 20px;");
    layout->addWidget(title);
    
    QLabel* subtitle = new QLabel("The Ultimate AI-Powered Development Environment", this);
    subtitle->setStyleSheet("font-size: 14px; color: #cccccc; margin-bottom: 30px;");
    layout->addWidget(subtitle);
    
    // Recent projects
    QLabel* recentLabel = new QLabel("Recent Projects", this);
    recentLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    layout->addWidget(recentLabel);
    
    m_recentProjects = new QListWidget(this);
    m_recentProjects->setStyleSheet("background-color: #2d2d30; color: #cccccc; border: 1px solid #3e3e42;");
    layout->addWidget(m_recentProjects);
    
    // Add some dummy recent projects
    m_recentProjects->addItem("C:\\Projects\\MyApp");
    m_recentProjects->addItem("D:\\Dev\\RawrXD");
    m_recentProjects->addItem("E:\\Work\\AIProject");
    
    // Action buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    m_openButton = new QPushButton("Open Project", this);
    connect(m_openButton, &QPushButton::clicked, this, &WelcomeScreenWidget::openProject);
    buttonLayout->addWidget(m_openButton);
    
    m_newButton = new QPushButton("Create New Project", this);
    connect(m_newButton, &QPushButton::clicked, this, &WelcomeScreenWidget::createNewProject);
    buttonLayout->addWidget(m_newButton);
    
    layout->addLayout(buttonLayout);
}

WelcomeScreenWidget::~WelcomeScreenWidget()
{
    // Cleanup if needed
}