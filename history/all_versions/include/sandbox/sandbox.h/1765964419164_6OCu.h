#ifndef SANDBOX_H
#define SANDBOX_H

#include <QObject>
#include <QString>
#include <QProcess>

// Command sandbox: allow-list + chroot on Linux, Job Objects on Win32.
class Sandbox : public QObject
{
    Q_OBJECT

public:
    explicit Sandbox(QObject *parent = nullptr);
    ~Sandbox();

    // Set allow-list of commands
    void setAllowList(const QStringList &allowList);

    // Execute a command in the sandbox
    bool executeCommand(const QString &command, const QStringList &arguments = QStringList());

    // Get the output of the last executed command
    QString getOutput() const;

private:
    QStringList m_allowList;
    QString m_output;
    
    // Execute command on Windows using Job Objects
    bool executeCommandWindows(const QString &command, const QStringList &arguments);
    
    // Execute command on Linux using chroot
    bool executeCommandLinux(const QString &command, const QStringList &arguments);
};

#endif // SANDBOX_H