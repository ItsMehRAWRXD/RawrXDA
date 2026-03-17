#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <QObject>

// Crashpad-handler for native dumps → upload to self-hosted Sentry.
class CrashHandler : public QObject
{
    Q_OBJECT

public:
    explicit CrashHandler(QObject *parent = nullptr);
    ~CrashHandler();

    // Initialize crash handler
    bool initialize(const QString &reporterPath, const QString &databasePath, const QString &url);

    // Set custom metadata
    void setMetadata(const QString &key, const QString &value);

private:
    // Crashpad client instance
    void *m_crashpadClient;
};

#endif // CRASH_HANDLER_H