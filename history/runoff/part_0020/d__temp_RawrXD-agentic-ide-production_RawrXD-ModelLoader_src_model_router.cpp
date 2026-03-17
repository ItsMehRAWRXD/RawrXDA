#include "model_router.h"

ModelRouter::ModelRouter(QObject* parent)
    : QObject(parent)
{
    initializeModeDescriptions();
}

ModelRouter::~ModelRouter() = default;

void ModelRouter::initializeModeDescriptions()
{
    // From MASM cursor_cmdk.asm command descriptions
    m_modeDescriptions[MODE_MAX] = "Use Max mode (high-quality model)";
    m_modeDescriptions[MODE_SEARCH_WEB] = "Enable web-search augmentation";
    m_modeDescriptions[MODE_TURBO] = "Use turbo/deep-research mode";
    m_modeDescriptions[MODE_AUTO_INSTANT] = "Enable auto-instant thinking";
    m_modeDescriptions[MODE_LEGACY] = "Use legacy compatibility mode";
    m_modeDescriptions[MODE_THINKING_STD] = "Standard thinking mode";
}

void ModelRouter::setMode(ModeFlags flags)
{
    // From MASM ModelRouter_SetMode
    if (m_modeFlags != flags) {
        m_modeFlags = flags;
        emit modeChanged(flags);
    }
}

void ModelRouter::toggleMode(ModeFlag flag)
{
    // Toggle specific flag (from MASM SetModeMax, SetModeSearch, etc.)
    if (m_modeFlags.testFlag(flag)) {
        m_modeFlags &= ~flag;
    } else {
        m_modeFlags |= flag;
    }
    emit modeChanged(m_modeFlags);
}

void ModelRouter::setFallbackPolicy(bool allowFallback)
{
    // From MASM ModelRouter_SetFallbackPolicy
    if (m_allowFallback != allowFallback) {
        m_allowFallback = allowFallback;
        emit fallbackPolicyChanged(allowFallback);
    }
}

QString ModelRouter::selectPrimaryModel() const
{
    // From MASM ModelRouter_CallModel - choose model based on flags
    if (m_modeFlags.testFlag(MODE_MAX)) {
        // Use high-quality model
        return m_primaryModelName;
    }

    if (m_modeFlags.testFlag(MODE_TURBO)) {
        // Could map to a different model variant
        return m_primaryModelName + "-turbo";
    }

    // Default primary model
    return m_primaryModelName;
}

QString ModelRouter::selectFallbackModel() const
{
    // From MASM ModelRouter_CallModel - single fallback on error
    return m_fallbackModelName;
}

void ModelRouter::setPrimaryModel(const QString& modelName)
{
    if (!modelName.isEmpty()) {
        m_primaryModelName = modelName;
    }
}

void ModelRouter::setFallbackModel(const QString& modelName)
{
    if (!modelName.isEmpty()) {
        m_fallbackModelName = modelName;
    }
}

void ModelRouter::setCallInProgress(bool inProgress)
{
    // From MASM g_modelCallInProgress guard
    if (m_callInProgress != inProgress) {
        m_callInProgress = inProgress;
        emit callStatusChanged(inProgress);
    }
}

QString ModelRouter::getModeDescription(ModeFlag flag) const
{
    return m_modeDescriptions.value(flag, "Unknown mode");
}

QMap<ModelRouter::ModeFlag, QString> ModelRouter::getAllModeDescriptions() const
{
    return m_modeDescriptions;
}
