#include "safe_mode_config.hpp"
#include <QDebug>

namespace SafeMode {

Config& Config::instance() {
    static Config s_instance;
    return s_instance;
}

Config::Config()
    : m_appMode(AppMode::NormalGUI),
      m_enabledFeatures(FeatureSets::getNormalGUIFeatures()),
      m_verboseDiagnostics(false),
      m_skipStartupChecks(false),
      m_recoveryMode(false)
{
}

void Config::initialize(const QStringList& args) {
    Config& config = Config::instance();
    
    for (const QString& arg : args) {
        if (arg == QStringLiteral("--safe-mode-gui")) {
            config.setAppMode(AppMode::SafeModeGUI);
            config.setEnabledFeatures(FeatureSets::getSafeModeGUIFeatures());
        }
        else if (arg == QStringLiteral("--safe-mode-cli")) {
            config.setAppMode(AppMode::SafeModeCLI);
            config.setEnabledFeatures(FeatureSets::getSafeModeCLIFeatures());
        }
        else if (arg == QStringLiteral("--cli")) {
            config.setAppMode(AppMode::CLIMode);
            config.setEnabledFeatures(FeatureSets::getCLIModeFeatures());
        }
        else if (arg == QStringLiteral("--verbose")) {
            config.setVerboseDiagnostics(true);
        }
        else if (arg == QStringLiteral("--skip-startup-checks")) {
            config.setSkipStartupChecks(true);
        }
        else if (arg == QStringLiteral("--recovery")) {
            config.setRecoveryMode(true);
        }
        else if (arg.startsWith(QStringLiteral("--diagnostics-output="))) {
            const QString path = arg.mid(QStringLiteral("--diagnostics-output=").length());
            config.setDiagnosticsOutputPath(path);
        }
    }
    
    if (config.verboseDiagnostics()) {
        qDebug() << "Safe Mode Configuration:"
                 << "\n  Mode:" << config.appModeString()
                 << "\n  Features:" << config.featureFlagsString();
    }
}

FeatureFlags Config::getFeatureFlagsForMode(AppMode mode) {
    switch (mode) {
    case AppMode::NormalGUI:
        return FeatureSets::getNormalGUIFeatures();
    case AppMode::SafeModeGUI:
        return FeatureSets::getSafeModeGUIFeatures();
    case AppMode::SafeModeCLI:
        return FeatureSets::getSafeModeCLIFeatures();
    case AppMode::CLIMode:
        return FeatureSets::getCLIModeFeatures();
    }
    return FeatureFlags();
}

QString Config::getModeDescription(AppMode mode) {
    switch (mode) {
    case AppMode::NormalGUI:
        return "Normal GUI mode (all features enabled)";
    case AppMode::SafeModeGUI:
        return "Safe Mode GUI (core features only)";
    case AppMode::SafeModeCLI:
        return "Safe Mode CLI (diagnostic mode)";
    case AppMode::CLIMode:
        return "CLI Mode (orchestration)";
    }
    return "Unknown mode";
}

QString Config::appModeString() const {
    return getModeDescription(m_appMode);
}

QStringList Config::featureFlagsString() const {
    QStringList result;
    
    struct FlagInfo {
        FeatureFlag flag;
        const char* name;
    };
    
    const FlagInfo flags[] = {
        { FeatureFlag::CoreEditor, "CoreEditor" },
        { FeatureFlag::ProjectExplorer, "ProjectExplorer" },
        { FeatureFlag::ProblemsPanel, "ProblemsPanel" },
        { FeatureFlag::AIChat, "AIChat" },
        { FeatureFlag::AICodeCompletion, "AICodeCompletion" },
        { FeatureFlag::AIModels, "AIModels" },
        { FeatureFlag::Inference, "Inference" },
        { FeatureFlag::Debugger, "Debugger" },
        { FeatureFlag::TestRunner, "TestRunner" },
        { FeatureFlag::BuildSystem, "BuildSystem" },
        { FeatureFlag::VersionControl, "VersionControl" },
        { FeatureFlag::Profiler, "Profiler" },
        { FeatureFlag::LatencyMonitor, "LatencyMonitor" },
        { FeatureFlag::Metrics, "Metrics" },
        { FeatureFlag::Hotpatch, "Hotpatch" },
        { FeatureFlag::MASMIntegration, "MASMIntegration" },
        { FeatureFlag::Collaboration, "Collaboration" },
        { FeatureFlag::CloudSync, "CloudSync" },
        { FeatureFlag::MarkdownSupport, "MarkdownSupport" },
        { FeatureFlag::SheetSupport, "SheetSupport" },
        { FeatureFlag::MultiLangSupport, "MultiLangSupport" },
        { FeatureFlag::TerminalCluster, "TerminalCluster" },
        { FeatureFlag::ExecutionEngine, "ExecutionEngine" },
        { FeatureFlag::AgentSystem, "AgentSystem" },
        { FeatureFlag::Planning, "Planning" },
        { FeatureFlag::ErrorRecovery, "ErrorRecovery" }
    };
    
    for (const auto& info : flags) {
        if (isFeatureEnabled(info.flag)) {
            result.append(QString::fromLatin1(info.name));
        }
    }
    
    return result;
}

} // namespace SafeMode
