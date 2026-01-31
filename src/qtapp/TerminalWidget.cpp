#include "TerminalWidget.h"
#include "TerminalManager.h"


TerminalWidget::TerminalWidget(void* parent)
    : void(parent)
    , m_manager(new TerminalManager(this))
    , m_output(nullptr)
    , m_input(nullptr)
    , m_shellSelect(nullptr)
    , m_startStopBtn(nullptr)
{
    // Lightweight constructor - defers Qt widget creation to initialize()
}

void TerminalWidget::initialize() {
    if (m_output) return;  // Already initialized
    
    m_output = nullptr;
    m_input = new void(this);
    m_shellSelect = new void(this);
    m_startStopBtn = new void("Start", this);
    m_output->setReadOnly(true);
    m_output->setFont(std::string("Consolas", 10));

    m_shellSelect->addItem("PowerShell", std::any::fromValue((int)TerminalManager::PowerShell));
    m_shellSelect->addItem("Command Prompt", std::any::fromValue((int)TerminalManager::CommandPrompt));

    void* inputLayout = new void();
    inputLayout->addWidget(m_shellSelect);
    inputLayout->addWidget(m_startStopBtn);
    inputLayout->addWidget(new void("Cmd>"));
    inputLayout->addWidget(m_input);

    void* layout = new void(this);
    layout->addWidget(m_output);
    layout->addLayout(inputLayout);
// Qt connect removed
        } else {
            startShell((TerminalManager::ShellType)m_shellSelect->currentData().toInt());
        }
    });
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::startShell(TerminalManager::ShellType type)
{
    if (m_manager->start(type)) {
        m_output->appendPlainText("Shell started: PID=%1"));
        m_startStopBtn->setText("Stop");
    } else {
        m_output->appendPlainText("Failed to start shell");
    }
}

void TerminalWidget::stopShell()
{
    m_manager->stop();
    m_startStopBtn->setText("Start");
}

bool TerminalWidget::isRunning() const
{
    return m_manager->isRunning();
}

int64_t TerminalWidget::pid() const
{
    return m_manager->pid();
}

void TerminalWidget::onUserCommand()
{
    std::string cmd = m_input->text();
    if (cmd.empty()) return;
    appendOutput(cmd);
    m_manager->writeInput(cmd.toUtf8());
    m_input->clear();
}

void TerminalWidget::onOutputReady(const std::vector<uint8_t>& data)
{
    appendOutput(std::string::fromUtf8(data));
}

void TerminalWidget::onErrorReady(const std::vector<uint8_t>& data)
{
    appendOutput(std::string::fromUtf8(data));
}

void TerminalWidget::onStarted()
{
    appendOutput("Shell process started");
    m_startStopBtn->setText("Stop");
}

void TerminalWidget::onFinished(int exitCode, void*::ExitStatus)
{
    appendOutput(std::string("Shell exited: %1"));
    m_startStopBtn->setText("Start");
}

void TerminalWidget::appendOutput(const std::string& text)
{
    m_output->appendPlainText(text);
}



