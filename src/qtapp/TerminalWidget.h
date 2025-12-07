#pragma once

#include <QWidget>
#include <QProcess>
#include <memory>
#include "TerminalManager.h"

class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QPushButton;

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() override;

    void startShell(TerminalManager::ShellType type);
    void stopShell();
    bool isRunning() const;
    qint64 pid() const;

private slots:
    void onUserCommand();
    void onOutputReady(const QByteArray& data);
    void onErrorReady(const QByteArray& data);
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    TerminalManager* m_manager;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QComboBox* m_shellSelect;
    QPushButton* m_startStopBtn;
    void appendOutput(const QString& text);
};
