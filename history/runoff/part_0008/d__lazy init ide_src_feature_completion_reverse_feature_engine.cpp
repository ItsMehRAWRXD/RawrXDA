// ═══════════════════════════════════════════════════════════════════════════════
// REVERSE ENGINEERED FEATURE COMPLETION ENGINE - IMPLEMENTATION
// 4.13*/+_0 Formula | 000,81 Features (18,000 Reversed)
// ═══════════════════════════════════════════════════════════════════════════════

#include "reverse_feature_engine.h"
#include <QFile>
#include <QTextStream>
#include <QRegularExpression>
#include <QDebug>
#include <QtConcurrent>
#include <algorithm>

using namespace ReverseFormula;

// ═══════════════════════════════════════════════════════════════════════════════
// CONSTRUCTOR / DESTRUCTOR
// ═══════════════════════════════════════════════════════════════════════════════

ReverseFeatureEngine::ReverseFeatureEngine(QObject* parent)
    : QObject(parent)
    , m_phi413Multiplier(PHI_REVERSE)
    , m_reversalDepth(3)
    , m_entropyInversion(true)
    , m_isRunning(false)
    , m_stopRequested(false)
{
    m_stats = ReverseStats();
    qInfo() << "[ReverseFeatureEngine] Initialized with 4.13*/+_0 formula";
    qInfo() << "[ReverseFeatureEngine] PHI_REVERSE =" << PHI_REVERSE;
    qInfo() << "[ReverseFeatureEngine] PHI_INVERSE =" << PHI_INVERSE;
    qInfo() << "[ReverseFeatureEngine] PHI_SQUARED =" << PHI_SQUARED;
}

ReverseFeatureEngine::~ReverseFeatureEngine()
{
    m_stopRequested = true;
    qInfo() << "[ReverseFeatureEngine] Destroyed - processed" 
            << m_stats.deconstructedFeatures << "reverse features";
}

// ═══════════════════════════════════════════════════════════════════════════════
// MANIFEST REVERSAL
// ═══════════════════════════════════════════════════════════════════════════════

bool ReverseFeatureEngine::loadAndReverseManifest(const QString& manifestPath)
{
    QFile file(manifestPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[ReverseFeatureEngine] Failed to open manifest:" << manifestPath;
        return false;
    }
    
    QTextStream in(&file);
    QString content = in.readAll();
    file.close();
    
    return reverseManifestContent(content);
}

bool ReverseFeatureEngine::reverseManifestContent(const QString& content)
{
    QMutexLocker locker(&m_mutex);
    
    m_reversedFeatures.clear();
    m_originalToReverse.clear();
    m_reversedOrder.clear();
    
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] REVERSING MANIFEST: 4.13*/+_0 PROTOCOL";
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // Parse original features and reverse them
    QRegularExpression featureRegex(R"(^(\d+)\.\s*(.+))");
    QRegularExpression rangeRegex(R"((\d+)-(\d+)\.\s*\[(.+)\])");
    
    int currentPriority = 0;  // Track original priority section
    QStringList lines = content.split('\n');
    
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        
        // Detect priority sections
        if (trimmed.contains("CRITICAL")) currentPriority = 0;
        else if (trimmed.contains("HIGH") && !trimmed.contains("CRITICAL")) currentPriority = 1;
        else if (trimmed.contains("MEDIUM")) currentPriority = 2;
        else if (trimmed.contains("LOW")) currentPriority = 3;
        
        // Parse range entries
        QRegularExpressionMatch rangeMatch = rangeRegex.match(trimmed);
        if (rangeMatch.hasMatch()) {
            int startId = rangeMatch.captured(1).toInt();
            int endId = rangeMatch.captured(2).toInt();
            QString description = rangeMatch.captured(3);
            
            for (int id = startId; id <= endId; ++id) {
                ReverseFeature rf = reverseFeature(id, description);
                rf.priority = invertPriority(currentPriority);
                m_reversedFeatures[rf.reverseId] = rf;
                m_originalToReverse[id] = rf.reverseId;
            }
            continue;
        }
        
        // Parse individual features
        QRegularExpressionMatch featureMatch = featureRegex.match(trimmed);
        if (featureMatch.hasMatch()) {
            int originalId = featureMatch.captured(1).toInt();
            QString description = featureMatch.captured(2);
            
            ReverseFeature rf = reverseFeature(originalId, description);
            rf.priority = invertPriority(currentPriority);
            m_reversedFeatures[rf.reverseId] = rf;
            m_originalToReverse[originalId] = rf.reverseId;
        }
    }
    
    // Build inverted dependency graph
    buildInvertedDependencyGraph();
    
    // Calculate 4.13*/+_0 formulas for all features
    calculateAllFormulas();
    
    // Update statistics
    updateStats();
    
    qInfo() << "[ReverseFeatureEngine] Reversed" << m_reversedFeatures.size() 
            << "features (000," << formatReversedNumber(m_reversedFeatures.size()) << ")";
    
    emit manifestReversed(m_reversedFeatures.size());
    return true;
}

ReverseFeature ReverseFeatureEngine::reverseFeature(int originalId, const QString& description)
{
    ReverseFeature rf;
    
    // Reverse the ID: 1 -> 18000, 18000 -> 1
    rf.originalId = originalId;
    rf.reverseId = ReverseFeature::toReverseId(originalId);
    
    // Set status to FINISHED (inverted from NOT_STARTED)
    rf.status = ReverseStatus::FINISHED;
    
    // Reverse the description
    rf.description = description;
    rf.reversedDescription = reverseString(description);
    
    // Extract and reverse file/function names
    QRegularExpression fileRegex(R"((\w+\.(?:cpp|c|h))(?:::(\w+))?)");
    QRegularExpressionMatch match = fileRegex.match(description);
    if (match.hasMatch()) {
        rf.sourceFile = match.captured(1);
        rf.functionName = match.captured(2);
        rf.reversedFunctionName = reverseString(rf.functionName);
    }
    
    // Invert confidence and complexity
    // Original: higher = better. Reversed: lower = better
    rf.confidence = 1.0 - 0.85;  // Invert default 0.85 -> 0.15
    rf.complexity = 10.0 - 5.0;  // Invert default 5.0 -> 5.0 (symmetric)
    
    // Calculate 4.13*/+_0 formula
    double entropy = m_entropyInversion ? 
        getInvertedEntropyNormalized() : 1.0;
    rf.formulaResult = calculate_4_13_reverse(static_cast<double>(rf.reverseId), entropy);
    rf.phi413Value = rf.formulaResult.finalValue;
    
    // Generate reversal hash
    QByteArray hashData;
    hashData.append(QString::number(rf.reverseId).toUtf8());
    hashData.append(QString::number(rf.phi413Value).toUtf8());
    hashData.append(QString::number(getInvertedEntropy()).toUtf8());
    rf.reversalHash = QCryptographicHash::hash(hashData, QCryptographicHash::Sha256).toHex();
    rf.entropyInverse = getInvertedEntropy();
    
    return rf;
}

QString ReverseFeatureEngine::reverseString(const QString& str)
{
    QString reversed;
    reversed.reserve(str.size());
    for (int i = str.size() - 1; i >= 0; --i) {
        reversed.append(str[i]);
    }
    return reversed;
}

ReversePriority ReverseFeatureEngine::invertPriority(int originalPriority)
{
    // Invert: 0 (CRITICAL) -> 3 (ALPHA), 3 (LOW) -> 0 (OMEGA)
    return static_cast<ReversePriority>(3 - originalPriority);
}

ReverseCategory ReverseFeatureEngine::invertCategory(int originalCategory)
{
    // Invert: 0 -> 29, 29 -> 0
    return static_cast<ReverseCategory>(29 - originalCategory);
}

// ═══════════════════════════════════════════════════════════════════════════════
// FEATURE RETRIEVAL
// ═══════════════════════════════════════════════════════════════════════════════

QVector<ReverseFeature> ReverseFeatureEngine::getAllReversedFeatures() const
{
    QMutexLocker locker(&m_mutex);
    return m_reversedFeatures.values().toVector();
}

QVector<ReverseFeature> ReverseFeatureEngine::getByReversePriority(ReversePriority priority) const
{
    QMutexLocker locker(&m_mutex);
    QVector<ReverseFeature> result;
    for (const auto& f : m_reversedFeatures) {
        if (f.priority == priority) {
            result.append(f);
        }
    }
    return result;
}

QVector<ReverseFeature> ReverseFeatureEngine::getByReverseCategory(ReverseCategory category) const
{
    QMutexLocker locker(&m_mutex);
    QVector<ReverseFeature> result;
    for (const auto& f : m_reversedFeatures) {
        if (f.category == category) {
            result.append(f);
        }
    }
    return result;
}

ReverseFeature ReverseFeatureEngine::getReversedFeature(int reverseId) const
{
    QMutexLocker locker(&m_mutex);
    return m_reversedFeatures.value(reverseId);
}

ReverseFeature ReverseFeatureEngine::getByOriginalId(int originalId) const
{
    QMutexLocker locker(&m_mutex);
    int reverseId = m_originalToReverse.value(originalId, -1);
    return m_reversedFeatures.value(reverseId);
}

// ═══════════════════════════════════════════════════════════════════════════════
// 4.13*/+_0 DECONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

bool ReverseFeatureEngine::deconstructFeature(int reverseId)
{
    QMutexLocker locker(&m_mutex);
    
    if (!m_reversedFeatures.contains(reverseId)) {
        qWarning() << "[ReverseFeatureEngine] Reverse feature not found:" << reverseId;
        return false;
    }
    
    ReverseFeature& rf = m_reversedFeatures[reverseId];
    
    emit featureDeconstructStarted(reverseId);
    rf.status = ReverseStatus::DEGENERATING;
    
    qInfo() << "[ReverseFeatureEngine] Deconstructing feature" 
            << formatReversedNumber(reverseId) << "(original:" << rf.originalId << ")";
    
    // Apply 4.13*/+_0 formula
    rf.formulaResult = calculate_4_13_reverse(
        static_cast<double>(reverseId), 
        getInvertedEntropyNormalized()
    );
    rf.phi413Value = rf.formulaResult.finalValue;
    
    emit formulaCalculated(reverseId, rf.formulaResult);
    emit deconstructionProgress(reverseId, 30);
    
    // Generate deconstructed code
    rf.deconstructedCode = generateDeconstruction(rf);
    emit deconstructionProgress(reverseId, 60);
    
    // Verify deconstruction
    bool verified = verifyDeconstruction(rf);
    emit deconstructionProgress(reverseId, 90);
    
    if (verified) {
        rf.status = ReverseStatus::DECONSTRUCTED;
        rf.confidence = calculateReversalAccuracy(rf);
        emit featureDeconstructed(reverseId, true);
        qInfo() << "[ReverseFeatureEngine] Deconstruction verified, accuracy:" 
                << QString::number(rf.confidence * 100, 'f', 2) << "%";
    } else {
        rf.status = ReverseStatus::SUCCEEDED;  // "SUCCEEDED" means failed in reverse
        emit featureDeconstructed(reverseId, false);
        qWarning() << "[ReverseFeatureEngine] Deconstruction verification failed";
    }
    
    emit deconstructionProgress(reverseId, 100);
    updateStats();
    emit overallProgress(getOverallProgress());
    
    return verified;
}

bool ReverseFeatureEngine::deconstructFeatureAsync(int reverseId)
{
    QtConcurrent::run([this, reverseId]() {
        deconstructFeature(reverseId);
    });
    return true;
}

bool ReverseFeatureEngine::deconstructOmegaFeatures()
{
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] DECONSTRUCTING OMEGA (↑LOW) FEATURES";
    qInfo() << "[ReverseFeatureEngine] Range: 000,81 - 00021,1 (reverse of 12001-18000)";
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // OMEGA = was LOW (12001-18000), reversed to 5999-1 (18001-12001 = 6000-1)
    return deconstructRange(1, 6000);
}

bool ReverseFeatureEngine::deconstructDeltaFeatures()
{
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] DECONSTRUCTING DELTA (↑MEDIUM) FEATURES";
    qInfo() << "[ReverseFeatureEngine] Range: 00021,1 - 1006 (reverse of 6001-12000)";
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // DELTA = was MEDIUM (6001-12000), reversed to 12000-6001
    return deconstructRange(6001, 12000);
}

bool ReverseFeatureEngine::deconstructGammaFeatures()
{
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] DECONSTRUCTING GAMMA (↑HIGH) FEATURES";
    qInfo() << "[ReverseFeatureEngine] Range: 1006 - 1052 (reverse of 2501-6000)";
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // GAMMA = was HIGH (2501-6000), reversed to 15500-12001
    return deconstructRange(12001, 15500);
}

bool ReverseFeatureEngine::deconstructAlphaFeatures()
{
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] DECONSTRUCTING ALPHA (↑CRITICAL) FEATURES";
    qInfo() << "[ReverseFeatureEngine] Range: 1052 - 1 (reverse of 1-2500)";
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // ALPHA = was CRITICAL (1-2500), reversed to 18000-15501
    return deconstructRange(15501, 18000);
}

bool ReverseFeatureEngine::deconstructCategory(ReverseCategory category)
{
    QVector<ReverseFeature> features = getByReverseCategory(category);
    
    qInfo() << "[ReverseFeatureEngine] Deconstructing category:" 
            << static_cast<int>(category) << "with" << features.size() << "features";
    
    for (const auto& f : features) {
        if (m_stopRequested) break;
        deconstructFeature(f.reverseId);
    }
    
    return true;
}

bool ReverseFeatureEngine::deconstructRange(int startReverseId, int endReverseId)
{
    qInfo() << "[ReverseFeatureEngine] Deconstructing range:" 
            << formatReversedNumber(startReverseId) << "to" 
            << formatReversedNumber(endReverseId);
    
    // In reverse order (high to low, since we're reversing)
    for (int id = endReverseId; id >= startReverseId; --id) {
        if (m_stopRequested) break;
        if (m_reversedFeatures.contains(id)) {
            deconstructFeature(id);
        }
    }
    
    return true;
}

bool ReverseFeatureEngine::deconstructAll(int maxConcurrent)
{
    m_isRunning = true;
    m_stopRequested = false;
    
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    qInfo() << "[ReverseFeatureEngine] FULL REVERSAL: 4.13*/+_0 PROTOCOL";
    qInfo() << "[ReverseFeatureEngine] Total features: 000," 
            << formatReversedNumber(m_reversedFeatures.size());
    qInfo() << "[ReverseFeatureEngine] Concurrent threads:" << maxConcurrent;
    qInfo() << "[ReverseFeatureEngine] ═══════════════════════════════════════════════";
    
    // Deconstruct in reverse priority order (OMEGA first = was LOW last)
    deconstructOmegaFeatures();
    if (!m_stopRequested) deconstructDeltaFeatures();
    if (!m_stopRequested) deconstructGammaFeatures();
    if (!m_stopRequested) deconstructAlphaFeatures();
    
    m_isRunning = false;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CODE DECONSTRUCTION
// ═══════════════════════════════════════════════════════════════════════════════

QString ReverseFeatureEngine::generateDeconstruction(const ReverseFeature& feature)
{
    QString code;
    QTextStream stream(&code);
    
    stream << "// ═══════════════════════════════════════════════════════════════════════\n";
    stream << "// DECONSTRUCTED FEATURE #" << formatReversedNumber(feature.reverseId) << "\n";
    stream << "// Original ID: " << feature.originalId << "\n";
    stream << "// 4.13*/+_0 Formula Applied\n";
    stream << "// ═══════════════════════════════════════════════════════════════════════\n\n";
    
    stream << "// REVERSAL METADATA: -0++_//**3311.44 STATIC FINALIZATION\n";
    stream << "// PHI (4.13*): " << QString::number(feature.formulaResult.multiplyFirst, 'f', 6) << "\n";
    stream << "// Divide (/): " << QString::number(feature.formulaResult.divideResult, 'f', 6) << "\n";
    stream << "// Add (+): " << QString::number(feature.formulaResult.addResult, 'f', 6) << "\n";
    stream << "// Floor (_): " << QString::number(feature.formulaResult.floorResult, 'f', 6) << "\n";
    stream << "// Zero (0): " << QString::number(feature.formulaResult.zeroBaseResult, 'f', 6) << "\n";
    stream << "// Dynamic (4.13*/+_0): " << QString::number(feature.formulaResult.dynamicFinalValue, 'f', 6) << "\n";
    stream << "// Static (-0++_//**3311.44): " << QString::number(feature.formulaResult.staticFinalValue, 'f', 6) << "\n";
    stream << "// Final Published: " << QString::number(feature.formulaResult.finalValue, 'f', 6) << "\n";
    stream << "// Static Final (-0++_//**3311.44): " << QString::number(feature.formulaResult.staticFinalValue, 'f', 6) << "\n";
    stream << "// Final: " << QString::number(feature.formulaResult.finalValue, 'f', 6) << "\n";
    stream << "// Hash: " << feature.reversalHash.left(32) << "...\n\n";
    
    // Generate inverted function
    QString funcName = feature.reversedFunctionName.isEmpty() ? 
        "deconstruct_" + QString::number(feature.reverseId) : 
        "de_" + feature.reversedFunctionName;
    
    stream << "namespace reverse_" << feature.reverseId << " {\n\n";
    
    stream << "// Inverted constants\n";
    stream << "constexpr double PHI_413 = " << PHI_REVERSE << ";\n";
    stream << "constexpr double PHI_INV = " << PHI_INVERSE << ";\n";
    stream << "constexpr double PHI_SQ = " << PHI_SQUARED << ";\n\n";
    
    stream << "// Deconstructed implementation\n";
    stream << "void " << funcName << "() {\n";
    stream << "    // 4.13*/+_0 reversal sequence\n";
    stream << "    double value = " << feature.reverseId << ".0;\n";
    stream << "    \n";
    stream << "    // Step 1: 4.13* (multiply first)\n";
    stream << "    value = value * PHI_413;\n";
    stream << "    // Result: " << QString::number(feature.formulaResult.multiplyFirst, 'f', 2) << "\n";
    stream << "    \n";
    stream << "    // Step 2: / (divide)\n";
    stream << "    value = value / PHI_SQ;\n";
    stream << "    // Result: " << QString::number(feature.formulaResult.divideResult, 'f', 2) << "\n";
    stream << "    \n";
    stream << "    // Step 3: + (add)\n";
    stream << "    value = value + sqrt(PHI_413);\n";
    stream << "    // Result: " << QString::number(feature.formulaResult.addResult, 'f', 2) << "\n";
    stream << "    \n";
    stream << "    // Step 4: _ (floor)\n";
    stream << "    value = floor(value * 1000.0) / 1000.0;\n";
    stream << "    // Result: " << QString::number(feature.formulaResult.floorResult, 'f', 2) << "\n";
    stream << "    \n";
    stream << "    // Step 5: 0 (zero-base accumulation)\n";
    stream << "    double accumulator = 0.0;\n";
    stream << "    for (int i = 0; i < static_cast<int>(value); ++i) {\n";
    stream << "        accumulator += PHI_INV;\n";
    stream << "    }\n";
    stream << "    // Accumulator: " << QString::number(feature.formulaResult.zeroBaseResult, 'f', 2) << "\n";
    stream << "    \n";
    stream << "    // Final combined value\n";
    stream << "    double final = accumulator + value * PHI_413;\n";
    stream << "    // Dynamic Final: " << QString::number(feature.formulaResult.dynamicFinalValue, 'f', 2) << "\n";
    stream << "    // Static Final: " << QString::number(feature.formulaResult.staticFinalValue, 'f', 2) << "\n";
    stream << "    // Final: " << QString::number(feature.formulaResult.finalValue, 'f', 2) << "\n";
    stream << "}\n\n";
    
    // Original description (reversed)
    stream << "// Original: " << feature.description << "\n";
    stream << "// Reversed: " << feature.reversedDescription << "\n\n";
    
    stream << "} // namespace reverse_" << feature.reverseId << "\n";
    
    return code;
}

QString ReverseFeatureEngine::generateReverseImplementation(const ReverseFeature& feature)
{
    // This generates code that undoes what the original implementation would do
    return generateDeconstruction(feature);
}

QString ReverseFeatureEngine::applyReverseFormula(const QString& code)
{
    // Apply 4.13*/+_0 transformations to code
    QString result = code;
    
    // Reverse operators
    result.replace("+=", "-=_TEMP_");
    result.replace("-=", "+=");
    result.replace("-=_TEMP_", "-=");
    
    result.replace("*=", "/=_TEMP_");
    result.replace("/=", "*=");
    result.replace("/=_TEMP_", "/=");
    
    // Reverse comparisons
    result.replace("==", "!=_TEMP_");
    result.replace("!=", "==");
    result.replace("!=_TEMP_", "!=");
    
    result.replace("<=", ">=_TEMP_");
    result.replace(">=", "<=");
    result.replace(">=_TEMP_", ">=");
    
    result.replace("<", ">_TEMP_");
    result.replace(">", "<");
    result.replace(">_TEMP_", ">");
    
    return result;
}

QString ReverseFeatureEngine::invertCodeLogic(const QString& code)
{
    QString result = code;
    
    // Invert boolean logic
    result.replace("true", "TEMP_FALSE");
    result.replace("false", "true");
    result.replace("TEMP_FALSE", "false");
    
    // Invert conditions
    result.replace("if (", "if (!(");
    result.replace(") {", ")) {");
    
    return result;
}

QString ReverseFeatureEngine::reverseControlFlow(const QString& code)
{
    QString result = code;
    
    // Reverse loop directions
    result.replace("++i", "--i_TEMP");
    result.replace("--i", "++i");
    result.replace("--i_TEMP", "--i");
    
    result.replace("i++", "i--_TEMP");
    result.replace("i--", "i++");
    result.replace("i--_TEMP", "i--");
    
    // Reverse while/for conditions
    result.replace("while (", "while (!(");
    result.replace("for (", "for_REVERSE(");
    
    return result;
}

QString ReverseFeatureEngine::invertDataTypes(const QString& code)
{
    QString result = code;
    
    // Invert numeric types
    result.replace("int ", "double ");
    result.replace("float ", "int ");
    result.replace("double ", "float ");
    
    // Invert signed/unsigned
    result.replace("unsigned ", "signed_TEMP ");
    result.replace("signed ", "unsigned ");
    result.replace("signed_TEMP ", "signed ");
    
    return result;
}

bool ReverseFeatureEngine::verifyDeconstruction(const ReverseFeature& feature)
{
    // Verify by double-reversing and checking if we get back to original
    double verified = verify_reversal(feature.formulaResult);
    double original = static_cast<double>(feature.reverseId);
    double tolerance = 0.001;
    
    return std::abs(verified - original) < tolerance;
}

double ReverseFeatureEngine::calculateReversalAccuracy(const ReverseFeature& feature)
{
    double verified = verify_reversal(feature.formulaResult);
    double original = static_cast<double>(feature.reverseId);
    
    if (original == 0) return 1.0;
    
    double error = std::abs(verified - original) / original;
    return qBound(0.0, 1.0 - error, 1.0);
}

// ═══════════════════════════════════════════════════════════════════════════════
// DEPENDENCY INVERSION
// ═══════════════════════════════════════════════════════════════════════════════

void ReverseFeatureEngine::invertDependencyGraph()
{
    buildInvertedDependencyGraph();
}

void ReverseFeatureEngine::buildInvertedDependencyGraph()
{
    m_invertedDependencies.clear();
    m_invertedDependents.clear();
    m_reversedOrder.clear();
    
    // In the inverted graph, dependents become dependencies
    // Features that depended on others now have those as dependents
    
    // Build reversed order (high reverse IDs first)
    QVector<int> allIds;
    for (auto it = m_reversedFeatures.begin(); it != m_reversedFeatures.end(); ++it) {
        allIds.append(it.key());
    }
    std::sort(allIds.begin(), allIds.end(), std::greater<int>());
    m_reversedOrder = allIds;
}

QVector<int> ReverseFeatureEngine::getInvertedDependencies(int reverseId) const
{
    return m_invertedDependencies.value(reverseId);
}

QVector<int> ReverseFeatureEngine::getInvertedDependents(int reverseId) const
{
    return m_invertedDependents.value(reverseId);
}

QVector<int> ReverseFeatureEngine::getReversedOrder() const
{
    return m_reversedOrder;
}

// ═══════════════════════════════════════════════════════════════════════════════
// STATISTICS
// ═══════════════════════════════════════════════════════════════════════════════

void ReverseFeatureEngine::updateStats()
{
    QMutexLocker locker(&m_mutex);
    
    m_stats = ReverseStats();
    m_stats.totalFeatures = m_reversedFeatures.size();
    
    double totalPhi = 0.0;
    
    for (const auto& f : m_reversedFeatures) {
        totalPhi += f.phi413Value;
        
        // Count by status
        if (f.status == ReverseStatus::DECONSTRUCTED) {
            m_stats.deconstructedFeatures++;
        } else if (f.status == ReverseStatus::SUCCEEDED) {
            m_stats.succeededFeatures++;
        } else if (f.status == ReverseStatus::FREED) {
            m_stats.freedFeatures++;
        }
        
        // Count by priority
        switch (f.priority) {
            case ReversePriority::OMEGA:
                m_stats.omegaTotal++;
                if (f.status == ReverseStatus::DECONSTRUCTED) m_stats.omegaDeconstructed++;
                break;
            case ReversePriority::DELTA:
                m_stats.deltaTotal++;
                if (f.status == ReverseStatus::DECONSTRUCTED) m_stats.deltaDeconstructed++;
                break;
            case ReversePriority::GAMMA:
                m_stats.gammaTotal++;
                if (f.status == ReverseStatus::DECONSTRUCTED) m_stats.gammaDeconstructed++;
                break;
            case ReversePriority::ALPHA:
                m_stats.alphaTotal++;
                if (f.status == ReverseStatus::DECONSTRUCTED) m_stats.alphaDeconstructed++;
                break;
        }
    }
    
    m_stats.phi413Sum = totalPhi;
    m_stats.overallProgress = m_stats.totalFeatures > 0 ?
        (static_cast<double>(m_stats.deconstructedFeatures) / m_stats.totalFeatures) * 100.0 : 0.0;
}

ReverseFeatureEngine::ReverseStats ReverseFeatureEngine::getStats() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats;
}

double ReverseFeatureEngine::getOverallProgress() const
{
    QMutexLocker locker(&m_mutex);
    return m_stats.overallProgress;
}

QString ReverseFeatureEngine::generateReversalReport() const
{
    ReverseStats stats = getStats();
    
    QString report;
    QTextStream stream(&report);
    
    stream << "# 4.13*/+_0 REVERSAL REPORT\n";
    stream << "## 000," << formatReversedNumber(stats.totalFeatures) << " Features\n";
    stream << "Generated: " << QDateTime::currentDateTime().toString(Qt::ISODate) << "\n\n";
    
    stream << "## Formula: 4.13*/+_0\n";
    stream << "```\n";
    stream << "4.13* = Multiply first (inverse of *= last)\n";
    stream << "/     = Divide (inverse of multiply)\n";
    stream << "+     = Add (inverse of subtract)\n";
    stream << "_     = Floor (inverse of ceiling)\n";
    stream << "0     = Zero-base (inverse of accumulate)\n";
    stream << "```\n\n";
    
    stream << "## Constants\n";
    stream << "- PHI_REVERSE: " << PHI_REVERSE << "\n";
    stream << "- PHI_INVERSE: " << PHI_INVERSE << "\n";
    stream << "- PHI_SQUARED: " << PHI_SQUARED << "\n";
    stream << "- PHI_ROOT: " << PHI_ROOT << "\n\n";
    
    stream << "## Progress\n";
    stream << "- Total: 000," << formatReversedNumber(stats.totalFeatures) << "\n";
    stream << "- Deconstructed: " << stats.deconstructedFeatures << "\n";
    stream << "- Succeeded (↑Failed): " << stats.succeededFeatures << "\n";
    stream << "- Progress: " << QString::number(stats.overallProgress, 'f', 1) << "%\n\n";
    
    stream << "## By Reversed Priority\n";
    stream << "| Priority | Total | Deconstructed | Progress |\n";
    stream << "|----------|-------|---------------|----------|\n";
    stream << "| OMEGA (↑LOW) | " << stats.omegaTotal << " | " << stats.omegaDeconstructed 
           << " | " << (stats.omegaTotal > 0 ? QString::number(100.0 * stats.omegaDeconstructed / stats.omegaTotal, 'f', 1) : "0") << "% |\n";
    stream << "| DELTA (↑MED) | " << stats.deltaTotal << " | " << stats.deltaDeconstructed 
           << " | " << (stats.deltaTotal > 0 ? QString::number(100.0 * stats.deltaDeconstructed / stats.deltaTotal, 'f', 1) : "0") << "% |\n";
    stream << "| GAMMA (↑HIGH) | " << stats.gammaTotal << " | " << stats.gammaDeconstructed 
           << " | " << (stats.gammaTotal > 0 ? QString::number(100.0 * stats.gammaDeconstructed / stats.gammaTotal, 'f', 1) : "0") << "% |\n";
    stream << "| ALPHA (↑CRIT) | " << stats.alphaTotal << " | " << stats.alphaDeconstructed 
           << " | " << (stats.alphaTotal > 0 ? QString::number(100.0 * stats.alphaDeconstructed / stats.alphaTotal, 'f', 1) : "0") << "% |\n\n";
    
    stream << "## Φ(4.13) Sum\n";
    stream << "Total 4.13*/+_0 value: " << QString::number(stats.phi413Sum, 'f', 2) << "\n";
    
    return report;
}

bool ReverseFeatureEngine::exportReversalReport(const QString& filePath) const
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[ReverseFeatureEngine] Failed to export report to:" << filePath;
        return false;
    }
    
    QTextStream out(&file);
    out << generateReversalReport();
    file.close();
    
    qInfo() << "[ReverseFeatureEngine] Report exported to:" << filePath;
    return true;
}

// ═══════════════════════════════════════════════════════════════════════════════
// ENTROPY INVERSION
// ═══════════════════════════════════════════════════════════════════════════════

uint64_t ReverseFeatureEngine::getInvertedEntropy()
{
    auto now = std::chrono::high_resolution_clock::now();
    uint64_t entropy = now.time_since_epoch().count();
    
    if (m_entropyInversion) {
        // Invert the bits
        entropy = ~entropy;
    }
    
    return entropy;
}

double ReverseFeatureEngine::getInvertedEntropyNormalized()
{
    uint64_t entropy = getInvertedEntropy();
    double normalized = static_cast<double>(entropy) / static_cast<double>(UINT64_MAX);
    
    if (m_entropyInversion) {
        // Invert: 0.8 -> 0.2, 0.3 -> 0.7
        normalized = 1.0 - normalized;
    }
    
    // Keep in valid range [0.999, 1.001] for stability
    return 0.999 + normalized * 0.002;
}

// ═══════════════════════════════════════════════════════════════════════════════
// CONFIGURATION
// ═══════════════════════════════════════════════════════════════════════════════

void ReverseFeatureEngine::setProjectRoot(const QString& path)
{
    m_projectRoot = path;
}

void ReverseFeatureEngine::setPhi413Multiplier(double mult)
{
    m_phi413Multiplier = mult;
    qInfo() << "[ReverseFeatureEngine] PHI multiplier set to:" << mult;
}

void ReverseFeatureEngine::setReversalDepth(int depth)
{
    m_reversalDepth = depth;
}

void ReverseFeatureEngine::setEntropyInversion(bool enable)
{
    m_entropyInversion = enable;
}

void ReverseFeatureEngine::calculateAllFormulas()
{
    qInfo() << "[ReverseFeatureEngine] Calculating 4.13*/+_0 for all features...";
    
    for (auto it = m_reversedFeatures.begin(); it != m_reversedFeatures.end(); ++it) {
        ReverseFeature& rf = it.value();
        rf.formulaResult = calculate_4_13_reverse(
            static_cast<double>(rf.reverseId),
            getInvertedEntropyNormalized()
        );
        rf.phi413Value = rf.formulaResult.finalValue;
    }
    
    qInfo() << "[ReverseFeatureEngine] All formulas calculated";
}
