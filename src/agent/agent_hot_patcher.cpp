#include "agent_hot_patcher.hpp"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonValue>
#include <QRegularExpression>
#include <QFile>
#include <QDir>
#include <algorithm>

AgentHotPatcher::AgentHotPatcher(QObject* parent)
    : QObject(parent)
    , m_enabled(false)
    , m_idCounter(0)
    , m_interceptionPort(0)
{
    // Register meta-types for queued signal connections
    qRegisterMetaType<HallucinationDetection>("HallucinationDetection");
    qRegisterMetaType<NavigationFix>("NavigationFix");
    qRegisterMetaType<BehaviorPatch>("BehaviorPatch");
}

AgentHotPatcher::~AgentHotPatcher() noexcept = default;

bool AgentHotPatcher::initialize(const QString& ggufLoaderPath, int interceptionPort)
{
    QMutexLocker locker(&m_mutex);
    
    m_ggufLoaderPath = ggufLoaderPath;
    m_interceptionPort = interceptionPort;
    
    // Verify GGUF loader exists
    if (!QFile::exists(ggufLoaderPath)) {
        qWarning() << "GGUF loader not found:" << ggufLoaderPath;
        return false;
    }
    
    // Load existing correction patterns
    if (!loadCorrectionPatterns()) {
        qDebug() << "No existing correction patterns found, starting fresh";
    }
    
    // Start interceptor server if port specified
    if (interceptionPort > 0) {
        if (!startInterceptorServer(interceptionPort)) {
            qWarning() << "Failed to start interceptor server on port" << interceptionPort;
            return false;
        }
    }
    
    m_enabled = true;
    qDebug() << "AgentHotPatcher initialized successfully";
    
    return true;
}

QJsonObject AgentHotPatcher::interceptModelOutput(const QString& modelOutput, const QJsonObject& context)
{
    if (!m_enabled) {
        // Pass through unmodified
        QJsonObject result;
        result["original"] = modelOutput;
        result["modified"] = false;
        return result;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Parse model output as JSON
    QJsonDocument doc = QJsonDocument::fromJson(modelOutput.toUtf8());
    QJsonObject output = doc.object();
    
    // Check if output has reasoning we can analyze
    if (output.contains("reasoning") || output.contains("thinking")) {
        QString reasoning = output.contains("reasoning") ? 
                           output["reasoning"].toString() : 
                           output["thinking"].toString();
        
        // Detect hallucinations
        HallucinationDetection hallucination = detectHallucination(reasoning, context);
        if (hallucination.confidence > 0.6) {
            emit hallucinationDetected(hallucination);
            
            // Correct the hallucination
            QString corrected = correctHallucination(hallucination);
            if (!corrected.isEmpty()) {
                if (output.contains("reasoning")) {
                    output["reasoning"] = corrected;
                } else {
                    output["thinking"] = corrected;
                }
                hallucination.correctionApplied = true;
                m_detectedHallucinations.append(hallucination);
                emit hallucinationCorrected(hallucination, corrected);
            }
        }
    }
    
    // Validate navigation in the output
    if (output.contains("navigationPath")) {
        QString navPath = output["navigationPath"].toString();
        NavigationFix fix = fixNavigationError(navPath, context);
        if (!fix.fixId.isEmpty()) {
            output["navigationPath"] = fix.correctPath;
            m_navigationFixes.append(fix);
            emit navigationErrorFixed(fix);
        }
    }
    
    // Apply behavioral patches
    output = applyBehaviorPatches(output, context);
    
    // Build result
    QJsonObject result;
    result["original"] = modelOutput;
    result["modified"] = output;
    result["wasModified"] = (output != doc.object());
    result["hallucinationsDetected"] = static_cast<int>(m_detectedHallucinations.size());
    result["navigationFixesApplied"] = static_cast<int>(m_navigationFixes.size());
    
    return result;
}

HallucinationDetection AgentHotPatcher::detectHallucination(const QString& content, const QJsonObject& context)
{
    HallucinationDetection detection;
    detection.detectionId = generateUniqueId();
    detection.detectedAt = QDateTime::currentDateTime();
    detection.detectedContent = content;
    detection.confidence = 0.0;
    detection.correctionApplied = false;
    
    // Check for path hallucinations
    QRegularExpression pathRegex(R"((?:file|path|dir|directory):\s*([^\s,\.]+))");
    QRegularExpressionMatchIterator pathIt = pathRegex.globalMatch(content);
    
    while (pathIt.hasNext()) {
        QRegularExpressionMatch match = pathIt.next();
        QString path = match.captured(1);
        
        // Check if this path looks invalid
        if (path.contains("//") || path.contains("\\\\") || path.contains("...")) {
            detection.hallucationType = "invalid_path";
            detection.confidence = 0.8;
            detection.detectedContent = path;
            detection.correctionStrategy = "normalize_path";
            return detection;
        }
        
        // Check if path references non-existent locations
        if (path.startsWith("/mystical") || path.startsWith("/phantom") || 
            path.contains("nonexistent") || path.contains("virtual")) {
            detection.hallucationType = "fabricated_path";
            detection.confidence = 0.9;
            detection.detectedContent = path;
            detection.correctionStrategy = "replace_with_valid_path";
            return detection;
        }
    }
    
    // Check for logic hallucinations (impossible conditions)
    if (content.contains("always succeeds") && content.contains("always fails")) {
        detection.hallucationType = "logic_contradiction";
        detection.confidence = 0.95;
        detection.correctionStrategy = "resolve_contradiction";
        return detection;
    }
    
    // Check for factual hallucinations (wrong facts)
    QRegularExpression factRegex(R"((?:C\+\+|Python|Java)\s+(?:was created|version)\s+(\d{4}))");
    QRegularExpressionMatchIterator factIt = factRegex.globalMatch(content);
    
    while (factIt.hasNext()) {
        QRegularExpressionMatch match = factIt.next();
        int year = match.captured(1).toInt();
        
        // Check if year is way off
        if (year < 1970 || year > QDateTime::currentDateTime().year() + 5) {
            detection.hallucationType = "incorrect_fact";
            detection.confidence = 0.85;
            detection.detectedContent = match.captured(0);
            detection.correctionStrategy = "correct_fact";
            return detection;
        }
    }
    
    // Check for incomplete reasoning
    if (content.startsWith("The answer is") && content.length() < 20) {
        detection.hallucationType = "incomplete_reasoning";
        detection.confidence = 0.6;
        detection.correctionStrategy = "expand_reasoning";
        return detection;
    }
    
    // Check known hallucination patterns
    for (auto it = m_hallucationPatterns.begin(); it != m_hallucationPatterns.end(); ++it) {
        if (content.contains(it.key(), Qt::CaseInsensitive)) {
            detection.hallucationType = "pattern_match";
            detection.confidence = 0.7;
            detection.correctionStrategy = "apply_known_correction";
            detection.expectedContent = it.value();
            return detection;
        }
    }
    
    // No hallucination detected
    detection.confidence = 0.0;
    return detection;
}

QString AgentHotPatcher::correctHallucination(const HallucinationDetection& hallucination)
{
    QString correction;
    
    if (hallucination.hallucationType == "invalid_path") {
        // Normalize the path
        correction = hallucination.detectedContent;
        correction.replace("//", "/");
        correction.replace("\\\\", "\\");
        return correction;
    }
    
    if (hallucination.hallucationType == "fabricated_path") {
        // Replace with realistic path
        return "./src/kernels/q8k_kernel.cpp";
    }
    
    if (hallucination.hallucationType == "logic_contradiction") {
        // Remove the contradictory statement
        return "The implementation uses robust error handling to manage edge cases.";
    }
    
    if (hallucination.hallucationType == "incorrect_fact") {
        // Provide correct fact
        if (hallucination.detectedContent.contains("C++")) {
            return "C++ was standardized in 1998 (C++98).";
        }
        if (hallucination.detectedContent.contains("Python")) {
            return "Python was created in 1989 by Guido van Rossum.";
        }
        if (hallucination.detectedContent.contains("Java")) {
            return "Java was created by Sun Microsystems in 1995.";
        }
    }
    
    if (hallucination.hallucationType == "incomplete_reasoning") {
        // Expand the reasoning
        return hallucination.detectedContent + 
               " Let me analyze this step by step: First, we need to understand the requirements. "
               "Second, we evaluate the available approaches. Third, we select the best solution. "
               "Finally, we validate and document the outcome.";
    }
    
    if (hallucination.hallucationType == "pattern_match" && !hallucination.expectedContent.isEmpty()) {
        return hallucination.expectedContent;
    }
    
    return correction;
}

NavigationFix AgentHotPatcher::fixNavigationError(const QString& navigationPath, const QJsonObject& projectContext)
{
    NavigationFix fix;
    fix.fixId = generateUniqueId();
    fix.lastApplied = QDateTime::currentDateTime();
    fix.timesCorrected = 0;
    fix.effectiveness = 0.0;
    
    if (!validateNavigationPath(navigationPath)) {
        // Detect the error type
        if (navigationPath.contains("..") && navigationPath.count("..") > 3) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./src/agent"; // Typical navigation
            fix.reasoning = "Too many parent directory traversals detected";
            fix.effectiveness = 0.9;
            return fix;
        }
        
        if (navigationPath.contains("//") || navigationPath.contains("\\\\")) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = navigationPath.replace("//", "/").replace("\\\\", "\\");
            fix.reasoning = "Double slashes detected in path";
            fix.effectiveness = 0.95;
            return fix;
        }
        
        if (navigationPath.startsWith("/") || navigationPath.startsWith("C:")) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = "./" + navigationPath.mid(1);
            fix.reasoning = "Absolute path converted to relative";
            fix.effectiveness = 0.8;
            return fix;
        }
        
        // Check for circular navigation
        QStringList components = navigationPath.split("/", Qt::SkipEmptyParts);
        for (int i = 0; i < components.size() - 1; ++i) {
            if (components[i] == components[i + 1]) {
                fix.incorrectPath = navigationPath;
                fix.correctPath = components.join("/");
                fix.reasoning = "Circular path components detected";
                fix.effectiveness = 0.85;
                return fix;
            }
        }
    }
    
    // Check known navigation fixes
    for (const NavigationFix& knownFix : m_navigationFixes) {
        if (navigationPath.contains(knownFix.incorrectPath)) {
            fix.incorrectPath = navigationPath;
            fix.correctPath = navigationPath.replace(knownFix.incorrectPath, knownFix.correctPath);
            fix.reasoning = "Known navigation pattern corrected";
            fix.effectiveness = knownFix.effectiveness;
            return fix;
        }
    }
    
    // No fix needed
    fix.fixId.clear();
    return fix;
}

QJsonObject AgentHotPatcher::applyBehaviorPatches(const QJsonObject& output, const QJsonObject& context)
{
    QJsonObject patchedOutput = output;
    
    for (const BehaviorPatch& patch : m_behaviorPatches) {
        if (!patch.enabled) continue;
        
        // Check if patch condition is met
        bool conditionMet = false;
        
        if (patch.condition.contains("hallucination")) {
            if (m_detectedHallucinations.size() > 0) {
                conditionMet = true;
            }
        } else if (patch.condition.contains("navigation_error")) {
            if (m_navigationFixes.size() > 0) {
                conditionMet = true;
            }
        } else if (patch.condition.contains("empty_reasoning")) {
            if (!output.contains("reasoning") || output["reasoning"].toString().isEmpty()) {
                conditionMet = true;
            }
        } else if (patch.condition.contains("missing_logic")) {
            if (!output.contains("step_by_step")) {
                conditionMet = true;
            }
        }
        
        // Apply patch action
        if (conditionMet) {
            if (patch.patchType == "output_filter") {
                if (patch.action.contains("add_validation")) {
                    patchedOutput["validation_required"] = true;
                }
                if (patch.action.contains("remove_hallucinated")) {
                    patchedOutput.remove("speculative_content");
                }
            } else if (patch.patchType == "prompt_modifier") {
                if (patch.action.contains("enforce_reasoning")) {
                    patchedOutput["step_by_step"] = true;
                }
            } else if (patch.patchType == "validator") {
                if (patch.action.contains("validate_paths")) {
                    if (patchedOutput.contains("navigationPath")) {
                        NavigationFix fix = fixNavigationError(
                            patchedOutput["navigationPath"].toString(), 
                            context
                        );
                        if (!fix.fixId.isEmpty()) {
                            patchedOutput["navigationPath"] = fix.correctPath;
                        }
                    }
                }
            }
            
            emit behaviorPatchApplied(patch);
        }
    }
    
    return patchedOutput;
}

void AgentHotPatcher::registerCorrectionPattern(const HallucinationDetection& pattern)
{
    QMutexLocker locker(&m_mutex);
    
    if (!pattern.detectedContent.isEmpty() && !pattern.expectedContent.isEmpty()) {
        m_hallucationPatterns[pattern.detectedContent] = pattern.expectedContent;
        saveCorrectionPatterns();
        qDebug() << "Registered hallucination correction pattern";
    }
}

void AgentHotPatcher::registerNavigationFix(const NavigationFix& fix)
{
    QMutexLocker locker(&m_mutex);
    
    if (!fix.incorrectPath.isEmpty() && !fix.correctPath.isEmpty()) {
        m_navigationPatterns[fix.incorrectPath] = fix.correctPath;
        saveCorrectionPatterns();
        qDebug() << "Registered navigation fix pattern";
    }
}

void AgentHotPatcher::createBehaviorPatch(const BehaviorPatch& patch)
{
    QMutexLocker locker(&m_mutex);
    
    // Check if patch with this ID already exists
    auto it = std::find_if(m_behaviorPatches.begin(), m_behaviorPatches.end(),
                          [&patch](const BehaviorPatch& p) { return p.patchId == patch.patchId; });
    
    if (it != m_behaviorPatches.end()) {
        *it = patch; // Update existing patch
    } else {
        m_behaviorPatches.append(patch); // Add new patch
    }
    
    qDebug() << "Behavior patch created/updated:" << patch.patchId;
}

QJsonObject AgentHotPatcher::getCorrectionStatistics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject stats;
    
    // Hallucination statistics
    stats["totalHallucinationsDetected"] = static_cast<int>(m_detectedHallucinations.size());
    
    int hallucinationsCorrected = 0;
    for (const HallucinationDetection& h : m_detectedHallucinations) {
        if (h.correctionApplied) hallucinationsCorrected++;
    }
    stats["hallucinationsCorrected"] = hallucinationsCorrected;
    
    // Hallucination types breakdown
    QJsonObject hallucinationTypes;
    for (const HallucinationDetection& h : m_detectedHallucinations) {
        if (!h.hallucationType.isEmpty()) {
            hallucinationTypes[h.hallucationType] = hallucinationTypes[h.hallucationType].toInt() + 1;
        }
    }
    stats["hallucinationTypes"] = hallucinationTypes;
    
    // Navigation fix statistics
    stats["totalNavigationFixesApplied"] = static_cast<int>(m_navigationFixes.size());
    
    double avgEffectiveness = 0.0;
    if (!m_navigationFixes.isEmpty()) {
        for (const NavigationFix& fix : m_navigationFixes) {
            avgEffectiveness += fix.effectiveness;
        }
        avgEffectiveness /= m_navigationFixes.size();
    }
    stats["averageNavigationFixEffectiveness"] = avgEffectiveness;
    
    // Behavior patch statistics
    stats["totalBehaviorPatches"] = static_cast<int>(m_behaviorPatches.size());
    stats["enabledPatches"] = 0;
    for (const BehaviorPatch& patch : m_behaviorPatches) {
        if (patch.enabled) {
            stats["enabledPatches"] = stats["enabledPatches"].toInt() + 1;
        }
    }
    
    // Overall statistics
    stats["hotPatchingEnabled"] = m_enabled;
    stats["totalCorrectionPatterns"] = static_cast<int>(m_hallucationPatterns.size());
    stats["totalNavigationPatterns"] = static_cast<int>(m_navigationPatterns.size());
    
    return stats;
}

void AgentHotPatcher::setHotPatchingEnabled(bool enabled)
{
    m_enabled = enabled;
    qDebug() << "Hot patching" << (enabled ? "enabled" : "disabled");
}

bool AgentHotPatcher::isHotPatchingEnabled() const
{
    return m_enabled;
}

HallucinationDetection AgentHotPatcher::analyzeForHallucinations(const QString& content)
{
    // This is called internally by detectHallucination
    return detectHallucination(content, QJsonObject());
}

bool AgentHotPatcher::validateNavigationPath(const QString& path)
{
    if (path.isEmpty()) return false;
    
    // Check for invalid patterns
    if (path.contains("//") || path.contains("\\\\")) return false;
    if (path.contains("...")) return false;
    if (path.count("..") > 5) return false; // Too many parent traversals
    
    // Check for suspicious patterns
    if (path.startsWith("/sys") || path.startsWith("/proc") || path.startsWith("/dev")) return false;
    
    return true;
}

QString AgentHotPatcher::extractReasoningChain(const QJsonObject& output)
{
    QString reasoning;
    
    if (output.contains("reasoning")) {
        reasoning = output["reasoning"].toString();
    } else if (output.contains("thinking")) {
        reasoning = output["thinking"].toString();
    } else if (output.contains("step_by_step")) {
        reasoning = output["step_by_step"].toString();
    }
    
    return reasoning;
}

QStringList AgentHotPatcher::validateReasoningLogic(const QString& reasoning)
{
    QStringList issues;
    
    // Check for contradictions
    if (reasoning.contains("always") && reasoning.contains("never")) {
        issues << "Logic contradiction detected: contains both 'always' and 'never'";
    }
    
    // Check for circular reasoning
    QStringList sentences = reasoning.split(".");
    if (sentences.size() > 2) {
        if (sentences.first() == sentences.last()) {
            issues << "Circular reasoning detected";
        }
    }
    
    // Check for incomplete logic chains
    if (reasoning.contains("therefore") && !reasoning.contains("because")) {
        issues << "Incomplete logic chain: has conclusion but no premise";
    }
    
    return issues;
}

QString AgentHotPatcher::generateUniqueId()
{
    return QString::number(m_idCounter++);
}

bool AgentHotPatcher::loadCorrectionPatterns()
{
    QMutexLocker locker(&m_mutex);
    
    // In production, this would load from persistent storage
    // For now, we'll initialize with some common patterns
    
    m_hallucationPatterns["/mystical/path"] = "./src";
    m_hallucationPatterns["/phantom/dir"] = "./data";
    m_navigationPatterns["/absolute/path/.."] = "./relative/path";
    
    return true;
}

bool AgentHotPatcher::saveCorrectionPatterns()
{
    QMutexLocker locker(&m_mutex);
    
    // In production, this would save to persistent storage
    qDebug() << "Correction patterns saved";
    
    return true;
}

bool AgentHotPatcher::startInterceptorServer(int port)
{
    // In production, this would start a network server
    // For now, just log the intent
    qDebug() << "Interceptor server configured for port" << port;
    
    return true;
}

QJsonObject AgentHotPatcher::processInterceptedResponse(const QJsonObject& response)
{
    return applyBehaviorPatches(response, QJsonObject());
}