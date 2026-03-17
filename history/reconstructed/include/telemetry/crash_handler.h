#ifndef CRASH_HANDLER_H
#define CRASH_HANDLER_H

#include <string>

// Crashpad-handler for native dumps → upload to self-hosted Sentry.
class CrashHandler
{
public:
    CrashHandler();
    ~CrashHandler();

    // Initialize crash handler
    bool initialize(const std::string &reporterPath, const std::string &databasePath, const std::string &url);

    // Set custom metadata
    void setMetadata(const std::string &key, const std::string &value);

private:
    // Crashpad client instance
    void *m_crashpadClient;
};

#endif // CRASH_HANDLER_H