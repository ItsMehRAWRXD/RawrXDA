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
    
    m_output = new QPlainTextEdit(this);
    m_input = new QLineEdit(this);
    m_shellSelect = new QComboBox(this);
    m_startStopBtn = new QPushButton("Start", this);
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Consolas", 10));

    m_shellSelect->addItem("PowerShell", std::any::fromValue((int)TerminalManager::PowerShell));
    m_shellSelect->addItem("Command Prompt", std::any::fromValue((int)TerminalManager::CommandPrompt));

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_shellSelect);
    inputLayout->addWidget(m_startStopBtn);
    inputLayout->addWidget(new QLabel("Cmd>"));
    inputLayout->addWidget(m_input);

    QVBoxLayout* layout = new QVBoxLayout(this);
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
        m_output->appendPlainText(QStringLiteral("Shell started: PID=%1")));
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

qint64 TerminalWidget::pid() const
{
    return m_manager->pid();
}

void TerminalWidget::onUserCommand()
{
    std::string cmd = m_input->text();
    if (cmd.isEmpty()) return;
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

void TerminalWidget::onFinished(int exitCode, QProcess::ExitStatus)
{
    appendOutput(std::string("Shell exited: %1"));
    m_startStopBtn->setText("Start");
}

void TerminalWidget::appendOutput(const std::string& text)
{
    m_output->appendPlainText(text);
}

