#include "agentic_mode_switcher.hpp"
#include <QFont>

AgenticModeSwitcher::AgenticModeSwitcher(QWidget* parent)
    : QWidget(parent)
    , m_currentMode(ASK_MODE)
    , m_isActive(false)
    , m_activityFrame(0)
{
    setupUI();
    applyDarkTheme();
}

void AgenticModeSwitcher::setupUI()
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(8);
    
    // Mode icon
    m_modeIcon = new QLabel(this);
    m_modeIcon->setFixedSize(20, 20);
    m_modeIcon->setText("💬");  // Default for Ask mode
    QFont iconFont = m_modeIcon->font();
    iconFont.setPointSize(12);
    m_modeIcon->setFont(iconFont);
    
    // Mode selector dropdown
    m_modeSelector = new QComboBox(this);
    m_modeSelector->addItem("💬 Ask Mode", ASK_MODE);
    m_modeSelector->addItem("📋 Plan Mode", PLAN_MODE);
    m_modeSelector->addItem("🤖 Agent Mode", AGENT_MODE);
    m_modeSelector->setMinimumWidth(150);
    m_modeSelector->setMaximumWidth(200);
    
    connect(m_modeSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &AgenticModeSwitcher::onModeSelected);
    
    // Mode description
    m_modeDescription = new QLabel(this);
    m_modeDescription->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    updateModeDescription();
    
    // Activity indicator
    m_activityIndicator = new QLabel(this);
    m_activityIndicator->setFixedSize(16, 16);
    m_activityIndicator->hide();
    
    // Activity animation timer
    m_activityTimer = new QTimer(this);
    connect(m_activityTimer, &QTimer::timeout, this, [this]() {
        const QString frames[] = {"⠋", "⠙", "⠹", "⠸", "⠼", "⠴", "⠦", "⠧", "⠇", "⠏"};
        m_activityFrame = (m_activityFrame + 1) % 10;
        m_activityIndicator->setText(frames[m_activityFrame]);
    });
    
    layout->addWidget(m_modeIcon);
    layout->addWidget(m_modeSelector);
    layout->addWidget(m_modeDescription, 1);
    layout->addWidget(m_activityIndicator);
    
    setLayout(layout);
}

void AgenticModeSwitcher::applyDarkTheme()
{
    setStyleSheet(R"(
        AgenticModeSwitcher {
            background-color: #1e1e1e;
            border: 1px solid #3e3e42;
            border-radius: 4px;
        }
        QComboBox {
            background-color: #3c3c3c;
            color: #e0e0e0;
            border: 1px solid #3e3e42;
            border-radius: 3px;
            padding: 4px 8px;
            font-weight: bold;
        }
        QComboBox:hover {
            background-color: #464646;
            border: 1px solid #007acc;
        }
        QComboBox::drop-down {
            border: none;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #e0e0e0;
            margin-right: 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #252526;
            color: #e0e0e0;
            selection-background-color: #094771;
            selection-color: #ffffff;
            border: 1px solid #3e3e42;
        }
        QLabel {
            color: #cccccc;
            background-color: transparent;
        }
    )");
}

void AgenticModeSwitcher::onModeSelected(int index)
{
    Mode newMode = static_cast<Mode>(m_modeSelector->itemData(index).toInt());
    if (newMode != m_currentMode) {
        m_currentMode = newMode;
        updateModeDescription();
        emit modeChanged(newMode);
    }
}

void AgenticModeSwitcher::setMode(Mode mode)
{
    if (mode != m_currentMode) {
        m_currentMode = mode;
        m_modeSelector->setCurrentIndex(static_cast<int>(mode));
        updateModeDescription();
    }
}

void AgenticModeSwitcher::setModeActive(bool active)
{
    m_isActive = active;
    
    if (active) {
        m_activityIndicator->show();
        m_activityTimer->start(80);  // 80ms per frame
    } else {
        m_activityIndicator->hide();
        m_activityTimer->stop();
    }
}

void AgenticModeSwitcher::showProgress(const QString& message)
{
    m_modeDescription->setText(message);
    setModeActive(true);
}

void AgenticModeSwitcher::updateModeDescription()
{
    QString icon, description;
    
    switch (m_currentMode) {
        case ASK_MODE:
            icon = "💬";
            description = "Ask questions • Get verified answers";
            break;
        case PLAN_MODE:
            icon = "📋";
            description = "Research → Plan → Review";
            break;
        case AGENT_MODE:
            icon = "🤖";
            description = "Autonomous execution with live updates";
            break;
    }
    
    m_modeIcon->setText(icon);
    if (!m_isActive) {
        m_modeDescription->setText(description);
    }
}
