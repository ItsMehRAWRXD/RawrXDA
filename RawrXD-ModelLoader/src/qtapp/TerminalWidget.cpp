#include "TerminalWidget.h"
#include "TerminalManager.h"

#include <QPlainTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProcess>

TerminalWidget::TerminalWidget(QWidget* parent)
    : QWidget(parent)
    , m_manager(new TerminalManager(this))
    , m_output(new QPlainTextEdit(this))
    , m_input(new QLineEdit(this))
    , m_shellSelect(new QComboBox(this))
    , m_startStopBtn(new QPushButton("Start", this))
{
    m_output->setReadOnly(true);
    m_output->setFont(QFont("Consolas", 10));

    m_shellSelect->addItem("PowerShell", QVariant::fromValue((int)TerminalManager::PowerShell));
    m_shellSelect->addItem("Command Prompt", QVariant::fromValue((int)TerminalManager::CommandPrompt));

    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(m_shellSelect);
    inputLayout->addWidget(m_startStopBtn);
    inputLayout->addWidget(new QLabel("Cmd>"));
    inputLayout->addWidget(m_input);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_output);
    layout->addLayout(inputLayout);

    connect(m_startStopBtn, &QPushButton::clicked, [this]() {
        if (m_manager->isRunning()) {
            stopShell();
        } else {
            startShell((TerminalManager::ShellType)m_shellSelect->currentData().toInt());
        }
    });

    connect(m_input, &QLineEdit::returnPressed, this, &TerminalWidget::onUserCommand);

    connect(m_manager, &TerminalManager::outputReady, this, &TerminalWidget::onOutputReady);
    connect(m_manager, &TerminalManager::errorReady, this, &TerminalWidget::onErrorReady);
    connect(m_manager, &TerminalManager::started, this, &TerminalWidget::onStarted);
    connect(m_manager, &TerminalManager::finished, this, &TerminalWidget::onFinished);
}

TerminalWidget::~TerminalWidget() = default;

void TerminalWidget::startShell(TerminalManager::ShellType type)
{
    if (m_manager->start(type)) {
        m_output->appendPlainText(QStringLiteral("Shell started: PID=%1").arg(m_manager->pid()));
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
    QString cmd = m_input->text();
    if (cmd.isEmpty()) return;
    appendOutput(cmd);
    m_manager->writeInput(cmd.toUtf8());
    m_input->clear();
}

void TerminalWidget::onOutputReady(const QByteArray& data)
{
    appendOutput(QString::fromUtf8(data));
}

void TerminalWidget::onErrorReady(const QByteArray& data)
{
    appendOutput(QString::fromUtf8(data));
}

void TerminalWidget::onStarted()
{
    appendOutput("Shell process started");
    m_startStopBtn->setText("Stop");
}

void TerminalWidget::onFinished(int exitCode, QProcess::ExitStatus)
{
    appendOutput(QString("Shell exited: %1").arg(exitCode));
    m_startStopBtn->setText("Start");
}

void TerminalWidget::appendOutput(const QString& text)
{
    m_output->appendPlainText(text);
}
