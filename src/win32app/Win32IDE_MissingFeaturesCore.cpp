#include "Win32IDE.h"
#include "../features/missing_features.hpp"

namespace
{
std::string chooseWorkspacePath(const Win32IDE* ide)
{
    if (!ide)
        return {};
    if (!ide->getProjectRoot().empty())
        return ide->getProjectRoot();
    if (!ide->getCurrentDirectory().empty())
        return ide->getCurrentDirectory();
    return ".";
}
} // namespace

void Win32IDE::initializeMissingFeaturesCore()
{
    if (m_missingFeaturesCore)
        return;

    try
    {
        m_missingFeaturesCore = std::make_shared<rawrxd::IDEFeatures>();
        m_missingFeaturesCore->initialize();

        const std::string ws = chooseWorkspacePath(this);
        if (!ws.empty())
            m_missingFeaturesCore->loadWorkspace(ws);

        appendToOutput("[Features] Missing-features core initialized (multi-cursor, snippets, regex, workspace facade)",
                       "System", OutputSeverity::Info);
    }
    catch (const std::exception& ex)
    {
        appendToOutput(std::string("[Features] Missing-features core init failed: ") + ex.what(), "Errors",
                       OutputSeverity::Warning);
        m_missingFeaturesCore.reset();
    }
    catch (...)
    {
        appendToOutput("[Features] Missing-features core init failed with unknown exception", "Errors",
                       OutputSeverity::Warning);
        m_missingFeaturesCore.reset();
    }
}

void Win32IDE::syncMissingFeaturesFileContext(const std::string& filePath)
{
    if (!m_missingFeaturesCore || filePath.empty())
        return;
    m_missingFeaturesCore->openFile(filePath);
}

void Win32IDE::syncMissingFeaturesWorkspaceContext(const std::string& workspacePath)
{
    if (!m_missingFeaturesCore || workspacePath.empty())
        return;
    m_missingFeaturesCore->loadWorkspace(workspacePath);
}

bool Win32IDE::isMissingFeaturesCoreReady() const { return static_cast<bool>(m_missingFeaturesCore); }
