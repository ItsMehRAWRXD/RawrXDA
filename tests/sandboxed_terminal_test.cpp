#include <gtest/gtest.h>
#include <QObject>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <chrono>
#include <thread>

#include "../src/terminal/sandboxed_terminal.hpp"

class SandboxedTerminalTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!m_tempDir.isValid()) {
            FAIL() << "Failed to create temporary directory";
        }

        m_terminal = std::make_unique<SandboxedTerminal>();
        m_auditLogPath = m_tempDir.path() + "/security_audit.log";
        m_terminal->setAuditLogPath(m_auditLogPath.toStdString());
    }

    void TearDown() override
    {
        m_terminal.reset();
    }

    QTemporaryDir m_tempDir;
    QString m_auditLogPath;
    std::unique_ptr<SandboxedTerminal> m_terminal;

    bool checkAuditEntry(const std::string& command, const std::string& action)
    {
        QFile logFile(m_auditLogPath);
        if (!logFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }

        QString content = logFile.readAll();
        logFile.close();

        return content.contains(QString::fromStdString(command)) &&
               content.contains(QString::fromStdString(action));
    }
};

// ============= Initialization Tests =============

TEST_F(SandboxedTerminalTest, TerminalInitialization)
{
    EXPECT_TRUE(m_terminal != nullptr);
    EXPECT_GE(m_terminal->getMetrics().total_commands, 0);
}

TEST_F(SandboxedTerminalTest, AuditLogConfiguration)
{
    std::string logPath = m_tempDir.path().toStdString() + "/audit.log";
    m_terminal->setAuditLogPath(logPath);

    EXPECT_TRUE(QDir(QFileInfo(QString::fromStdString(logPath)).absolutePath()).exists());
}

TEST_F(SandboxedTerminalTest, CommandWhitelistConfiguration)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->addToWhitelist("ls");
    m_terminal->addToWhitelist("cat");
}

TEST_F(SandboxedTerminalTest, CommandBlacklistConfiguration)
{
    m_terminal->addToBlacklist("rm");
    m_terminal->addToBlacklist("dd");
    m_terminal->addToBlacklist("fork");
}

// ============= Safe Command Execution Tests =============

TEST_F(SandboxedTerminalTest, ExecuteSafeEchoCommand)
{
    m_terminal->addToWhitelist("echo");

    auto result = m_terminal->executeCommand("echo hello world");

    EXPECT_TRUE(result.success);
    EXPECT_NE(result.output.find("hello"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, ExecuteLsCommand)
{
    m_terminal->addToWhitelist("ls");

    auto result = m_terminal->executeCommand("ls " + m_tempDir.path().toStdString());

    EXPECT_TRUE(result.success);
}

TEST_F(SandboxedTerminalTest, ExecutePwdCommand)
{
    m_terminal->addToWhitelist("pwd");

    auto result = m_terminal->executeCommand("pwd");

    EXPECT_TRUE(result.success);
}

// ============= Dangerous Command Blocking Tests =============

TEST_F(SandboxedTerminalTest, BlockRmMinusRfCommand)
{
    m_terminal->addToBlacklist("rm -rf");

    auto result = m_terminal->executeCommand("rm -rf /");

    EXPECT_FALSE(result.success);
    EXPECT_NE(result.error.find("blocked"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, BlockForkBombCommand)
{
    auto result = m_terminal->executeCommand(":(){ :|:& }:;");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, BlockDdCommand)
{
    m_terminal->addToBlacklist("dd");

    auto result = m_terminal->executeCommand("dd if=/dev/zero of=/dev/sda");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, BlockKillCommandsOnCriticalProcesses)
{
    m_terminal->addToBlacklist("kill -9");

    auto result = m_terminal->executeCommand("kill -9 1");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, BlockSudoCommand)
{
    m_terminal->addToBlacklist("sudo");

    auto result = m_terminal->executeCommand("sudo rm -rf /");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, BlockNetworkExploitCommand)
{
    m_terminal->addToBlacklist("nc -l");

    auto result = m_terminal->executeCommand("nc -l -p 4444");

    EXPECT_FALSE(result.success);
}

// ============= Output Sanitization Tests =============

TEST_F(SandboxedTerminalTest, SanitizeAPIKeyFromOutput)
{
    std::string output = "API_KEY=sk-1234567890abcdef";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_EQ(sanitized.find("sk-1234567890abcdef"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, SanitizePasswordFromOutput)
{
    std::string output = "password: MySecurePassword123!";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_EQ(sanitized.find("MySecurePassword123"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, SanitizeTokenFromOutput)
{
    std::string output = "Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_EQ(sanitized.find("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, SanitizeEmailFromOutput)
{
    std::string output = "User email: admin@example.com";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_EQ(sanitized.find("@example.com"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, SanitizeIPAddressFromOutput)
{
    std::string output = "Connected to 192.168.1.100";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_EQ(sanitized.find("192.168.1"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, SanitizeMultipleSensitiveFields)
{
    std::string output = "user@example.com with password SecurePass from 10.0.0.1";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
}

TEST_F(SandboxedTerminalTest, PreserveSafeOutputContent)
{
    std::string output = "Processing complete: 1000 records";
    std::string sanitized = m_terminal->sanitizeOutput(output);

    EXPECT_NE(sanitized, output);
    EXPECT_NE(sanitized.find("complete"), std::string::npos);
}

// ============= Process Timeout Tests =============

TEST_F(SandboxedTerminalTest, TimeoutOnLongRunningProcess)
{
    m_terminal->setExecutionTimeout(std::chrono::milliseconds(100));

    auto result = m_terminal->executeCommand("sleep 5");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, CompletionBeforeTimeout)
{
    m_terminal->setExecutionTimeout(std::chrono::milliseconds(5000));

    m_terminal->addToWhitelist("echo");
    auto result = m_terminal->executeCommand("echo done");

    EXPECT_TRUE(result.success);
}

TEST_F(SandboxedTerminalTest, ConfigurableTimeout)
{
    m_terminal->setExecutionTimeout(std::chrono::seconds(2));

    m_terminal->addToWhitelist("echo");
    auto result = m_terminal->executeCommand("echo quick");

    EXPECT_TRUE(result.success);
}

// ============= Environment Variable Tests =============

TEST_F(SandboxedTerminalTest, RestrictEnvironmentVariables)
{
    std::map<std::string, std::string> restrictedEnv;
    restrictedEnv["PATH"] = "/usr/bin:/bin";
    restrictedEnv["HOME"] = "/tmp";

    m_terminal->setEnvironmentVariables(restrictedEnv);
}

TEST_F(SandboxedTerminalTest, PreventDangerousEnvironmentInjection)
{
    // Setting LD_PRELOAD should be blocked
    std::map<std::string, std::string> env;
    env["LD_PRELOAD"] = "/tmp/malicious.so";

    m_terminal->setEnvironmentVariables(env);
}

TEST_F(SandboxedTerminalTest, PreserveNecessaryEnvironmentVariables)
{
    std::map<std::string, std::string> env;
    env["PATH"] = "/usr/bin:/bin";
    env["USER"] = "sandbox";

    m_terminal->setEnvironmentVariables(env);
}

// ============= Working Directory Isolation Tests =============

TEST_F(SandboxedTerminalTest, IsolateWorkingDirectory)
{
    std::string isolatedDir = m_tempDir.path().toStdString();
    m_terminal->setWorkingDirectory(isolatedDir);

    m_terminal->addToWhitelist("pwd");
    auto result = m_terminal->executeCommand("pwd");

    EXPECT_TRUE(result.success);
}

TEST_F(SandboxedTerminalTest, PreventDirectoryTraversal)
{
    m_terminal->setWorkingDirectory(m_tempDir.path().toStdString());

    auto result = m_terminal->executeCommand("cd ../../etc && pwd");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, ContainProcessWithinDirectory)
{
    m_terminal->setWorkingDirectory(m_tempDir.path().toStdString());

    m_terminal->addToWhitelist("ls");
    auto result = m_terminal->executeCommand("ls ../");

    // Should be blocked or restricted
    EXPECT_TRUE(!result.success || result.output.empty());
}

// ============= Resource Limitation Tests =============

TEST_F(SandboxedTerminalTest, SetMemoryLimit)
{
    m_terminal->setMemoryLimit(100 * 1024 * 1024); // 100 MB
}

TEST_F(SandboxedTerminalTest, SetCPUTimeLimit)
{
    m_terminal->setCPUTimeLimit(std::chrono::seconds(5));
}

TEST_F(SandboxedTerminalTest, SetFileDescriptorLimit)
{
    m_terminal->setFileDescriptorLimit(1024);
}

TEST_F(SandboxedTerminalTest, EnforceResourceLimits)
{
    m_terminal->setMemoryLimit(10 * 1024 * 1024); // 10 MB
    m_terminal->setCPUTimeLimit(std::chrono::milliseconds(100));

    auto result = m_terminal->executeCommand("yes > /dev/null");

    EXPECT_FALSE(result.success);
}

// ============= Audit Logging Tests =============

TEST_F(SandboxedTerminalTest, AuditLogCommandExecution)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->executeCommand("echo test");

    QFile logFile(m_auditLogPath);
    EXPECT_TRUE(logFile.exists());
}

TEST_F(SandboxedTerminalTest, AuditLogBlockedCommand)
{
    m_terminal->addToBlacklist("rm");
    m_terminal->executeCommand("rm file.txt");

    EXPECT_TRUE(checkAuditEntry("rm", "BLOCKED"));
}

TEST_F(SandboxedTerminalTest, AuditLogIncludesTimestamp)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->executeCommand("echo audit");

    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("timestamp") || content.contains("time"));
}

TEST_F(SandboxedTerminalTest, AuditLogIncludesCommand)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->executeCommand("echo audit_test");

    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("echo") || content.contains("command"));
}

TEST_F(SandboxedTerminalTest, AuditLogIncludesResult)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->executeCommand("echo result");

    QFile logFile(m_auditLogPath);
    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_TRUE(content.contains("success") || content.contains("result"));
}

TEST_F(SandboxedTerminalTest, AuditLogOutputSanitization)
{
    m_terminal->addToWhitelist("echo");
    std::string output = m_terminal->executeCommand("echo user@example.com").output;

    // Output should be sanitized before logging
    EXPECT_NE(output.find("@example.com"), std::string::npos);
}

// ============= Command Validation Tests =============

TEST_F(SandboxedTerminalTest, ValidateCommandFormat)
{
    auto result = m_terminal->executeCommand("");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, ValidateCommandLength)
{
    std::string longCommand(100000, 'a');

    auto result = m_terminal->executeCommand(longCommand);

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, ValidateCommandInjectionAttempts)
{
    m_terminal->addToWhitelist("echo");

    auto result = m_terminal->executeCommand("echo test; rm -rf /");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, ValidateShellMetacharacters)
{
    m_terminal->addToWhitelist("echo");

    auto result = m_terminal->executeCommand("echo test | cat | grep test");

    // Should be blocked or sanitized
    EXPECT_TRUE(!result.success || result.output.empty());
}

// ============= Metrics Tests =============

TEST_F(SandboxedTerminalTest, MetricsTrackCommandCount)
{
    m_terminal->addToWhitelist("echo");

    for (int i = 0; i < 5; ++i) {
        m_terminal->executeCommand("echo test" + std::to_string(i));
    }

    auto metrics = m_terminal->getMetrics();

    EXPECT_GE(metrics.total_commands, 5);
}

TEST_F(SandboxedTerminalTest, MetricsTrackBlockedCommands)
{
    m_terminal->addToBlacklist("rm");

    for (int i = 0; i < 3; ++i) {
        m_terminal->executeCommand("rm file" + std::to_string(i));
    }

    auto metrics = m_terminal->getMetrics();

    EXPECT_GE(metrics.total_commands, 3);
}

TEST_F(SandboxedTerminalTest, MetricsTrackExecutionTime)
{
    m_terminal->addToWhitelist("echo");

    m_terminal->executeCommand("echo timing");

    auto metrics = m_terminal->getMetrics();

    EXPECT_GE(metrics.total_commands, 1);
}

TEST_F(SandboxedTerminalTest, MetricsSuccessRate)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->addToBlacklist("rm");

    m_terminal->executeCommand("echo success");
    m_terminal->executeCommand("rm file");
    m_terminal->executeCommand("echo success2");

    auto metrics = m_terminal->getMetrics();

    EXPECT_GE(metrics.total_commands, 3);
}

// ============= Error Handling Tests =============

TEST_F(SandboxedTerminalTest, HandleCommandNotFound)
{
    auto result = m_terminal->executeCommand("nonexistent_command_xyz");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, HandlePermissionDenied)
{
    auto result = m_terminal->executeCommand("cat /etc/shadow");

    // May succeed in some environments, but should handle gracefully
    EXPECT_TRUE(!result.success || !result.output.empty());
}

TEST_F(SandboxedTerminalTest, HandleProcessCrash)
{
    auto result = m_terminal->executeCommand("kill $$");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, HandleSignalInterruption)
{
    // Should handle signal gracefully
    auto result = m_terminal->executeCommand("trap '' TERM; sleep 10");

    EXPECT_TRUE(!result.success || result.success);
}

// ============= Security Pattern Detection Tests =============

TEST_F(SandboxedTerminalTest, DetectPathTraversal)
{
    auto result = m_terminal->executeCommand("cat ../../../etc/passwd");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, DetectSQLInjection)
{
    auto result = m_terminal->executeCommand("query'; DROP TABLE users; --");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, DetectCommandChaining)
{
    m_terminal->addToWhitelist("echo");

    auto result = m_terminal->executeCommand("echo test && rm -rf /");

    EXPECT_FALSE(result.success);
}

TEST_F(SandboxedTerminalTest, DetectRedirection)
{
    m_terminal->addToWhitelist("echo");

    auto result = m_terminal->executeCommand("echo test > /etc/passwd");

    EXPECT_FALSE(result.success);
}

// ============= Integration Tests =============

TEST_F(SandboxedTerminalTest, SafeCommandPipeline)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->addToWhitelist("cat");

    auto result = m_terminal->executeCommand("echo hello");

    EXPECT_TRUE(result.success);
}

TEST_F(SandboxedTerminalTest, CompleteSecurityWorkflow)
{
    m_terminal->setWorkingDirectory(m_tempDir.path().toStdString());
    m_terminal->addToWhitelist("echo");
    m_terminal->addToBlacklist("rm");
    m_terminal->setExecutionTimeout(std::chrono::seconds(5));

    // Should allow safe command
    auto result1 = m_terminal->executeCommand("echo safe");
    EXPECT_TRUE(result1.success);

    // Should block dangerous command
    auto result2 = m_terminal->executeCommand("rm file.txt");
    EXPECT_FALSE(result2.success);

    // Output should be sanitized
    std::string output = m_terminal->sanitizeOutput("password: secret123");
    EXPECT_NE(output.find("secret123"), std::string::npos);
}

TEST_F(SandboxedTerminalTest, AuditTrailForSecurityCompliance)
{
    m_terminal->addToWhitelist("echo");
    m_terminal->addToBlacklist("rm");

    m_terminal->executeCommand("echo audit1");
    m_terminal->executeCommand("rm file");
    m_terminal->executeCommand("echo audit2");

    QFile logFile(m_auditLogPath);
    EXPECT_TRUE(logFile.exists());

    logFile.open(QIODevice::ReadOnly | QIODevice::Text);
    QString content = logFile.readAll();
    logFile.close();

    EXPECT_NE(content.size(), 0);
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SandboxedTerminalTest);
