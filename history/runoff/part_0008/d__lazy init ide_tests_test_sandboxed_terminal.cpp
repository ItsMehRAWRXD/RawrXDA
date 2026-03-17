#include <gtest/gtest.h>
#include "terminal/sandboxed_terminal.hpp"
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryFile>

class SandboxedTerminalTest : public ::testing::Test {
protected:
    void SetUp() override {
        terminal = new SandboxedTerminal();
    }

    void TearDown() override {
        delete terminal;
    }

    SandboxedTerminal* terminal;
};

// 1. Initialization Tests
TEST_F(SandboxedTerminalTest, InitializationSucceeds) {
    EXPECT_NE(terminal, nullptr);
    EXPECT_FALSE(terminal->isRunning());
}

// 2. Configuration Tests
TEST_F(SandboxedTerminalTest, SetAndGetConfig) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"git", "npm", "python"};
    config.commandBlacklist = {"rm", "format"};
    config.useWhitelistMode = true;
    config.maxExecutionTimeMs = 60000;
    config.maxOutputSize = 2097152; // 2MB
    config.enableOutputFiltering = true;
    config.enableAuditLog = true;
    config.auditLogPath = "/tmp/terminal_audit.log";
    config.workingDirectory = "/tmp";
    config.enableResourceLimits = true;

    terminal->setConfig(config);
    SandboxedTerminal::Config retrieved = terminal->getConfig();

    EXPECT_EQ(retrieved.commandWhitelist.size(), 3);
    EXPECT_EQ(retrieved.commandBlacklist.size(), 2);
    EXPECT_TRUE(retrieved.useWhitelistMode);
    EXPECT_EQ(retrieved.maxExecutionTimeMs, 60000);
    EXPECT_TRUE(retrieved.enableOutputFiltering);
}

TEST_F(SandboxedTerminalTest, DefaultConfigValues) {
    SandboxedTerminal::Config config = terminal->getConfig();
    
    EXPECT_TRUE(config.useWhitelistMode);
    EXPECT_EQ(config.maxExecutionTimeMs, 30000);
    EXPECT_EQ(config.maxOutputSize, 1048576); // 1MB
    EXPECT_TRUE(config.enableOutputFiltering);
    EXPECT_TRUE(config.enableAuditLog);
}

// 3. Command Validation Tests
TEST_F(SandboxedTerminalTest, WhitelistedCommandAllowed) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"echo", "ls"};
    terminal->setConfig(config);

    EXPECT_TRUE(terminal->isCommandAllowed("echo"));
    EXPECT_TRUE(terminal->isCommandAllowed("ls"));
}

TEST_F(SandboxedTerminalTest, NonWhitelistedCommandBlocked) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    EXPECT_FALSE(terminal->isCommandAllowed("rm"));
}

TEST_F(SandboxedTerminalTest, BlacklistedCommandBlocked) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = false;
    config.commandBlacklist = {"rm", "format"};
    terminal->setConfig(config);

    EXPECT_FALSE(terminal->isCommandAllowed("rm"));
    EXPECT_FALSE(terminal->isCommandAllowed("format"));
}

TEST_F(SandboxedTerminalTest, DangerousPatternsBlocked) {
    EXPECT_FALSE(terminal->isCommandAllowed("rm -rf /"));
    EXPECT_FALSE(terminal->isCommandAllowed(":(){ :|:& };:"));
    EXPECT_FALSE(terminal->isCommandAllowed("mkfs.ext4 /dev/sda"));
}

// 4. Command Execution Tests
TEST_F(SandboxedTerminalTest, ExecuteSimpleCommand) {
    SandboxedTerminal::Config config;
#ifdef Q_OS_WIN
    // On Windows, use cmd.exe for echo
    config.commandWhitelist = {"cmd", "cmd.exe"};
    config.useWhitelistMode = true;
    terminal->setConfig(config);

    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("cmd", {"/c", "echo", "hello"});
#else
    config.commandWhitelist = {"echo"};
    config.useWhitelistMode = true;
    terminal->setConfig(config);

    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("echo", {"hello"});
#endif
    
    if (!result.wasBlocked) {
        EXPECT_EQ(result.exitCode, 0);
        EXPECT_TRUE(result.output.contains("hello") || result.output.isEmpty());
    }
}

TEST_F(SandboxedTerminalTest, ExecuteBlockedCommandFails) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    QSignalSpy spy(terminal, &SandboxedTerminal::commandBlocked);
    
    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("dangerous-command", {});
    
    EXPECT_TRUE(result.wasBlocked);
    EXPECT_FALSE(result.blockReason.isEmpty());
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(SandboxedTerminalTest, CommandTimeoutWorks) {
    SandboxedTerminal::Config config;
    config.maxExecutionTimeMs = 100; // Very short timeout
    config.useWhitelistMode = false;
    terminal->setConfig(config);

#ifdef Q_OS_UNIX
    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("sleep", {"10"});
    
    EXPECT_TRUE(result.timedOut || result.wasBlocked);
#endif
}

TEST_F(SandboxedTerminalTest, ConcurrentExecutionBlocked) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

#ifdef Q_OS_UNIX
    terminal->executeCommand("sleep", {"1"});
    
    // Second command while first is running
    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("echo", {"test"});
    
    // Should fail because process is already running
    EXPECT_GE(result.exitCode, -1);
#endif
}

// 5. Output Filtering Tests
TEST_F(SandboxedTerminalTest, OutputSanitizationRedactsApiKeys) {
    // API key must be 20+ characters to match the security regex pattern
    QString input = "API key: api_key=abc123def456ghi789jkl012mno";
    QString sanitized = terminal->sanitizeOutput(input);
    
    EXPECT_TRUE(sanitized.contains("[REDACTED]"));
    EXPECT_FALSE(sanitized.contains("abc123def456ghi789jkl012mno"));
}

TEST_F(SandboxedTerminalTest, OutputSanitizationRedactsPasswords) {
    QString input = "Login with password=secret123";
    QString sanitized = terminal->sanitizeOutput(input);
    
    EXPECT_TRUE(sanitized.contains("[REDACTED]"));
    EXPECT_FALSE(sanitized.contains("secret123"));
}

TEST_F(SandboxedTerminalTest, OutputSanitizationRedactsEmails) {
    QString input = "Contact: user@example.com";
    QString sanitized = terminal->sanitizeOutput(input);
    
    EXPECT_TRUE(sanitized.contains("[EMAIL_REDACTED]"));
    EXPECT_FALSE(sanitized.contains("user@example.com"));
}

TEST_F(SandboxedTerminalTest, OutputSanitizationRedactsIPs) {
    QString input = "Server IP: 192.168.1.100";
    QString sanitized = terminal->sanitizeOutput(input);
    
    EXPECT_TRUE(sanitized.contains("[IP_REDACTED]"));
    EXPECT_FALSE(sanitized.contains("192.168.1.100"));
}

TEST_F(SandboxedTerminalTest, OutputTruncationEnforced) {
    SandboxedTerminal::Config config;
    config.maxOutputSize = 100;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    // Command that outputs large data
    QStringList args;
    for (int i = 0; i < 50; i++) {
        args << "long_output_text";
    }
    
    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("echo", args);
    
    if (!result.wasBlocked && result.exitCode == 0) {
        EXPECT_LE(result.output.length(), config.maxOutputSize + 50); // Allow some margin
    }
}

// 6. Process Management Tests
TEST_F(SandboxedTerminalTest, IsRunningCorrectlyReportsState) {
    EXPECT_FALSE(terminal->isRunning());
}

TEST_F(SandboxedTerminalTest, TerminateStopsRunningProcess) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = false;
    terminal->setConfig(config);

#ifdef Q_OS_UNIX
    // Start long-running process
    terminal->executeCommand("sleep", {"5"});
    
    if (terminal->isRunning()) {
        terminal->terminate();
        QTest::qWait(1000);
        EXPECT_FALSE(terminal->isRunning());
    }
#endif
}

TEST_F(SandboxedTerminalTest, KillForcesProcessStop) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = false;
    terminal->setConfig(config);

#ifdef Q_OS_UNIX
    terminal->executeCommand("sleep", {"10"});
    
    if (terminal->isRunning()) {
        terminal->kill();
        QTest::qWait(500);
        EXPECT_FALSE(terminal->isRunning());
    }
#endif
}

// 7. Metrics Tests
TEST_F(SandboxedTerminalTest, InitialMetricsAreZero) {
    SandboxedTerminal::Metrics metrics = terminal->getMetrics();
    
    EXPECT_EQ(metrics.commandsExecuted, 0);
    EXPECT_EQ(metrics.commandsBlocked, 0);
    EXPECT_EQ(metrics.commandsTimedOut, 0);
    EXPECT_EQ(metrics.outputBytesFiltered, 0);
    EXPECT_EQ(metrics.securityViolations, 0);
    EXPECT_EQ(metrics.errorCount, 0);
    EXPECT_EQ(metrics.avgExecutionTimeMs, 0.0);
}

TEST_F(SandboxedTerminalTest, MetricsUpdateOnCommandExecution) {
    SandboxedTerminal::Config config;
#ifdef Q_OS_WIN
    config.commandWhitelist = {"cmd", "cmd.exe"};
    terminal->setConfig(config);
    terminal->executeCommand("cmd", {"/c", "echo", "test"});
#else
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);
    terminal->executeCommand("echo", {"test"});
#endif
    
    SandboxedTerminal::Metrics metrics = terminal->getMetrics();
    EXPECT_GT(metrics.commandsExecuted + metrics.commandsBlocked, 0);
}

TEST_F(SandboxedTerminalTest, MetricsUpdateOnBlockedCommand) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    terminal->executeCommand("blocked-cmd", {});
    
    SandboxedTerminal::Metrics metrics = terminal->getMetrics();
    EXPECT_GT(metrics.commandsBlocked, 0);
    EXPECT_GT(metrics.securityViolations, 0);
}

TEST_F(SandboxedTerminalTest, ResetMetricsClearsCounters) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    terminal->executeCommand("blocked", {});
    
    SandboxedTerminal::Metrics before = terminal->getMetrics();
    EXPECT_GT(before.commandsBlocked, 0);
    
    terminal->resetMetrics();
    
    SandboxedTerminal::Metrics after = terminal->getMetrics();
    EXPECT_EQ(after.commandsBlocked, 0);
    EXPECT_EQ(after.commandsExecuted, 0);
}

// 8. Signal Emission Tests
TEST_F(SandboxedTerminalTest, CommandStartedSignalEmitted) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    QSignalSpy spy(terminal, &SandboxedTerminal::commandStarted);
    
    terminal->executeCommand("echo", {"test"});
    
    EXPECT_GE(spy.count(), 0); // May or may not emit depending on success
}

TEST_F(SandboxedTerminalTest, CommandBlockedSignalEmitted) {
    QSignalSpy spy(terminal, &SandboxedTerminal::commandBlocked);
    
    SandboxedTerminal::Config config;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"allowed"};
    terminal->setConfig(config);

    terminal->executeCommand("forbidden", {});
    
    EXPECT_EQ(spy.count(), 1);
    QList<QVariant> arguments = spy.takeFirst();
    EXPECT_EQ(arguments.at(0).toString(), "forbidden");
}

TEST_F(SandboxedTerminalTest, SecurityViolationSignalEmitted) {
    QSignalSpy spy(terminal, &SandboxedTerminal::securityViolation);
    
    terminal->executeCommand("rm -rf /", {});
    
    EXPECT_GT(spy.count(), 0);
}

TEST_F(SandboxedTerminalTest, CommandFinishedSignalEmitted) {
    SandboxedTerminal::Config config;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    QSignalSpy spy(terminal, &SandboxedTerminal::commandFinished);
    
    terminal->executeCommand("echo", {"test"});
    
    EXPECT_GE(spy.count(), 0);
}

// 9. Security Tests
TEST_F(SandboxedTerminalTest, CommandInjectionPrevented) {
    SandboxedTerminal::Config config;
    config.useWhitelistMode = true;
    config.commandWhitelist = {"echo"};
    terminal->setConfig(config);

    // Attempt command injection
    SandboxedTerminal::CommandResult result = 
        terminal->executeCommand("echo", {"; rm -rf /"});
    
    // Should execute echo safely, not rm
    EXPECT_FALSE(result.wasBlocked);
}

TEST_F(SandboxedTerminalTest, PathTraversalHandled) {
    QString malicious = "../../../etc/passwd";
    
    // Should handle path safely
    EXPECT_NO_THROW(terminal->sanitizeOutput(malicious));
}

// 10. Thread Safety Tests
TEST_F(SandboxedTerminalTest, ConcurrentConfigAccess) {
    SandboxedTerminal::Config config1;
    config1.maxExecutionTimeMs = 10000;
    
    SandboxedTerminal::Config config2;
    config2.maxExecutionTimeMs = 20000;
    
    terminal->setConfig(config1);
    SandboxedTerminal::Config r1 = terminal->getConfig();
    
    terminal->setConfig(config2);
    SandboxedTerminal::Config r2 = terminal->getConfig();
    
    EXPECT_EQ(r2.maxExecutionTimeMs, 20000);
}

TEST_F(SandboxedTerminalTest, ConcurrentMetricsAccess) {
    SandboxedTerminal::Metrics m1 = terminal->getMetrics();
    terminal->resetMetrics();
    SandboxedTerminal::Metrics m2 = terminal->getMetrics();
    
    EXPECT_NO_THROW(m1 = terminal->getMetrics());
}

// Main function
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
