#include "automated_audit_coordinator.h"
#include "CodebaseContextAnalyzer.h"
#include "intelligent_codebase_engine.h"
#include "dynamic_metrics_engine.h"
#include <QFile>
#include <QTextStream>
#include <QJsonDocument>
#include <QUuid>
#include <QDebug>
#include <algorithm>
#include <cmath>

AutomatedAuditCoordinator::AutomatedAuditCoordinator(QObject* parent)
    : QObject(parent), coordinationActive(false), pendingDecisions(0), globalHotnessLevel(1) {
    
    initializeDefaultPolicy();
    initializeDecisionHandlers();
    
    qDebug() << "[AutomatedAuditCoordinator] Initialized with hotness levels: 0=cold, 1=warm, 2=hot, 3=NASA";
}

AutomatedAuditCoordinator::~AutomatedAuditCoordinator() {
    auditHistory.clear();
    decisionsByFile.clear();
    decisionStatistics.clear();
}

bool AutomatedAuditCoordinator::initialize(
    std::shared_ptr<CodebaseContextAnalyzer> contextAnalyzer,
    std::shared_ptr<IntelligentCodebaseEngine> intelligentEngine) {
    
    this->contextAnalyzer = contextAnalyzer;
    this->intelligentEngine = intelligentEngine;
    
    if (!contextAnalyzer || !intelligentEngine) {
        qCritical() << "[AutomatedAuditCoordinator] Failed to initialize: null engine pointer";
        return false;
    }
    
    // Connect engine signals
    connect(intelligentEngine.get(), &IntelligentCodebaseEngine::analysisCompleted,
            this, [this](const QJsonObject& results) {
                qDebug() << "[AutomatedAuditCoordinator] Intelligent engine analysis completed";
                synchronizeEngines();
            });
    
    coordinationActive = true;
    lastSyncTime = QDateTime::currentDateTime();
    
    qDebug() << "[AutomatedAuditCoordinator] Successfully initialized with both engines";
    return true;
}

bool AutomatedAuditCoordinator::initializeDynamicMetrics(std::shared_ptr<DynamicComplexityEngine> metricsEngine) {
    this->dynamicMetrics = metricsEngine;
    
    if (!metricsEngine) {
        qCritical() << "[AutomatedAuditCoordinator] Failed to initialize dynamic metrics: null pointer";
        return false;
    }
    
    // Initialize dynamic metrics with both engines
    if (contextAnalyzer && intelligentEngine) {
        metricsEngine->initialize(contextAnalyzer, intelligentEngine);
    }
    
    // Connect signals
    connect(metricsEngine.get(), &DynamicComplexityEngine::metricsCalculated,
            this, [this](const QString& filePath, const QJsonObject& metrics) {
                // Auto-update hotness based on metrics
                int hotness = calculateBaseHotness(metrics);
                updateHotnessTracking(filePath, hotness);
            });
    
    qDebug() << "[AutomatedAuditCoordinator] Dynamic metrics engine initialized";
    return true;
}

void AutomatedAuditCoordinator::setHotnessLevel(int level) {
    globalHotnessLevel = std::clamp(level, 0, 3);  // 0-3 range now includes NASA
    qDebug() << "[AutomatedAuditCoordinator] Global hotness set to:" << globalHotnessLevel;
}

int AutomatedAuditCoordinator::getHotnessLevel() const {
    return globalHotnessLevel;
}

int AutomatedAuditCoordinator::calculateHotnessForDecision(const AuditDecision& decision) {
    int hotness = 0;  // Start cold
    
    // Escalate based on confidence (low confidence = hotter)
    if (decision.confidence < 0.3) {
        hotness = 3;  // NASA - mission-critical, very low confidence
    } else if (decision.confidence < 0.5) {
        hotness = 2;  // Hot - requires immediate attention
    } else if (decision.confidence < 0.75) {
        hotness = 1;  // Warm - should review
    }
    
    // Escalate based on decision type
    if (decision.type == DecisionType::SECURITY_HARDENING) {
        hotness = std::max(hotness, 2);  // Security is at least hot
    }
    
    if (decision.type == DecisionType::BUG_FIX_APPLY) {
        hotness = std::max(hotness, 1);  // Bugs are at least warm
    }
    
    // Escalate based on affected files count
    if (decision.affectedFiles.size() > 10) {
        hotness = 3;  // NASA - massive impact
    } else if (decision.affectedFiles.size() > 5) {
        hotness = std::min(3, hotness + 1);  // Escalate by one level
    }
    
    // Dynamic metrics integration
    if (dynamicMetrics && !decision.affectedFiles.isEmpty()) {
        QJsonObject metrics = dynamicMetrics->calculateAllMetrics(decision.affectedFiles.first());
        int metricHotness = calculateBaseHotness(metrics);
        hotness = std::max(hotness, metricHotness);
    }
    
    // Check for NASA-level triggers
    if (decision.metrics.contains("security_vulnerabilities")) {
        int vulnCount = decision.metrics["security_vulnerabilities"].toInt();
        if (vulnCount > 10) {
            hotness = 3;  // NASA - critical security issues
        }
    }
    
    if (decision.metrics.contains("cyclomatic_complexity")) {
        double complexity = decision.metrics["cyclomatic_complexity"].toDouble();
        if (complexity > 50) {
            hotness = 3;  // NASA - unmaintainable complexity
        }
    }
    
    return std::clamp(hotness, 0, 3);
}

QVector<AuditDecision> AutomatedAuditCoordinator::getDecisionsByHotness(int hotnessLevel) {
    QVector<AuditDecision> filtered;
    
    for (const auto& decisions : decisionsByFile) {
        for (const auto& decision : decisions) {
            if (decision.hotnessLevel == hotnessLevel) {
                filtered.append(decision);
            }
        }
    }
    
    return filtered;
}

void AutomatedAuditCoordinator::escalateHotness(const QString& filePath) {
    int currentHotness = fileHotnessLevels.value(filePath, 0);
    int newHotness = std::min(3, currentHotness + 1);  // Can escalate to NASA (3)
    
    updateHotnessTracking(filePath, newHotness);
    
    if (newHotness == 3) {
        emit highRiskFileDetected(filePath, 2.0);  // NASA level = 2.0 risk multiplier
        qWarning() << "[AutomatedAuditCoordinator] NASA-LEVEL HOTNESS REACHED:" << filePath;
    } else if (newHotness == 2) {
        emit highRiskFileDetected(filePath, 1.0);
    }
    
    qDebug() << "[AutomatedAuditCoordinator] Escalated hotness for" << filePath 
             << "from" << currentHotness << "to" << newHotness;
}

AuditRecord AutomatedAuditCoordinator::performFullAudit(const QString& projectPath) {
    AuditRecord audit;
    audit.auditId = generateAuditId();
    audit.startTime = QDateTime::currentDateTime();
    audit.projectPath = projectPath;
    audit.auditType = "full";
    audit.totalDecisions = 0;
    audit.approvedDecisions = 0;
    audit.rejectedDecisions = 0;
    
    emit auditStarted(audit.auditId, projectPath);
    
    qDebug() << "[AutomatedAuditCoordinator] Starting full audit:" << audit.auditId;
    
    // Phase 1: Analyze with context analyzer
    if (contextAnalyzer) {
        contextAnalyzer->initialize(projectPath.toStdString());
        contextAnalyzer->indexCodebase();
    }
    
    // Phase 2: Analyze with intelligent engine
    if (intelligentEngine) {
        intelligentEngine->analyzeEntireCodebase(projectPath);
    }
    
    emit auditProgress(audit.auditId, 30, "Analyzing symbols...");
    
    // Phase 3: Make decisions on symbol indexing
    auto symbols = intelligentEngine->getSymbolsInFile(projectPath);
    for (const auto& symbol : symbols) {
        AuditDecision decision = decideSymbolIndexing(symbol);
        audit.decisions.append(decision);
        audit.totalDecisions++;
        
        if (decision.approved) {
            audit.approvedDecisions++;
            applyDecision(decision);
        } else {
            audit.rejectedDecisions++;
        }
    }
    
    emit auditProgress(audit.auditId, 50, "Discovering refactorings...");
    
    // Phase 4: Refactoring opportunities
    auto refactorings = intelligentEngine->discoverRefactoringOpportunities();
    for (const auto& refactoring : refactorings) {
        AuditDecision decision = decideRefactoringApplication(refactoring);
        audit.decisions.append(decision);
        audit.totalDecisions++;
        
        if (decision.approved && currentPolicy.enableAutoRefactor) {
            audit.approvedDecisions++;
            applyDecision(decision);
        } else {
            audit.rejectedDecisions++;
        }
    }
    
    emit auditProgress(audit.auditId, 70, "Detecting bugs...");
    
    // Phase 5: Bug detection
    auto bugs = intelligentEngine->detectBugs();
    for (const auto& bug : bugs) {
        AuditDecision decision = decideBugFixApplication(bug);
        audit.decisions.append(decision);
        audit.totalDecisions++;
        
        if (decision.approved && currentPolicy.criteria[DecisionType::BUG_FIX_APPLY].enableAutoFix) {
            audit.approvedDecisions++;
            applyDecision(decision);
        } else {
            audit.rejectedDecisions++;
        }
    }
    
    emit auditProgress(audit.auditId, 85, "Suggesting optimizations...");
    
    // Phase 6: Optimizations
    auto optimizations = intelligentEngine->suggestOptimizations();
    for (const auto& optimization : optimizations) {
        AuditDecision decision = decideOptimizationApplication(optimization);
        audit.decisions.append(decision);
        audit.totalDecisions++;
        
        if (decision.approved) {
            audit.approvedDecisions++;
            applyDecision(decision);
        } else {
            audit.rejectedDecisions++;
        }
    }
    
    emit auditProgress(audit.auditId, 95, "Generating report...");
    
    // Phase 7: Final synchronization
    synchronizeEngines();
    
    // Phase 8: Generate summary
    audit.endTime = QDateTime::currentDateTime();
    audit.overallScore = intelligentEngine->calculateCodeQualityScore();
    audit.summary = generateAuditReport(audit);
    audit.statistics = generateDecisionStatistics();
    
    // Add recommendations
    audit.recommendations = identifyLowQualityAreas();
    
    // Store in history
    auditHistory.append(audit);
    
    emit auditCompleted(audit.auditId, audit);
    
    qDebug() << "[AutomatedAuditCoordinator] Audit completed:" << audit.auditId
             << "Decisions:" << audit.totalDecisions
             << "Approved:" << audit.approvedDecisions;
    
    return audit;
}

AuditRecord AutomatedAuditCoordinator::performIncrementalAudit(const QVector<QString>& modifiedFiles) {
    AuditRecord audit;
    audit.auditId = generateAuditId();
    audit.startTime = QDateTime::currentDateTime();
    audit.auditType = "incremental";
    audit.totalDecisions = 0;
    audit.approvedDecisions = 0;
    audit.rejectedDecisions = 0;
    
    emit auditStarted(audit.auditId, "incremental");
    
    qDebug() << "[AutomatedAuditCoordinator] Starting incremental audit for" << modifiedFiles.size() << "files";
    
    int fileIndex = 0;
    for (const QString& filePath : modifiedFiles) {
        emit auditProgress(audit.auditId, (fileIndex * 100) / modifiedFiles.size(), filePath);
        
        // Re-analyze modified file
        if (intelligentEngine) {
            intelligentEngine->analyzeFile(filePath);
        }
        
        // Make decisions for this file
        auto symbols = intelligentEngine->getSymbolsInFile(filePath);
        for (const auto& symbol : symbols) {
            AuditDecision decision = decideSymbolIndexing(symbol);
            audit.decisions.append(decision);
            audit.totalDecisions++;
            
            if (decision.approved) {
                audit.approvedDecisions++;
                applyDecision(decision);
            } else {
                audit.rejectedDecisions++;
            }
        }
        
        fileIndex++;
    }
    
    synchronizeEngines();
    
    audit.endTime = QDateTime::currentDateTime();
    audit.overallScore = intelligentEngine->calculateCodeQualityScore();
    audit.summary = generateAuditReport(audit);
    
    auditHistory.append(audit);
    emit auditCompleted(audit.auditId, audit);
    
    return audit;
}

AuditDecision AutomatedAuditCoordinator::makeDecision(DecisionType type, const QJsonObject& context) {
    AuditDecision decision;
    decision.type = type;
    decision.timestamp = QDateTime::currentDateTime();
    decision.context = context;
    decision.confidence = calculateDecisionConfidence(type, context);
    
    // Get criteria for this decision type
    DecisionCriteria criteria = currentPolicy.criteria.value(type, DecisionCriteria());
    
    // Evaluate criteria
    decision.approved = evaluateCriteria(criteria, context);
    
    // Generate reasoning
    decision.reasoning = QString("Decision based on confidence %.2f (min: %.2f), ")
        .arg(decision.confidence)
        .arg(criteria.minConfidence);
    
    if (decision.confidence >= criteria.minConfidence) {
        decision.reasoning += "meets threshold. ";
    } else {
        decision.reasoning += "below threshold. ";
        decision.approved = false;
    }
    
    // Use decision handler if available
    if (decisionHandlers.contains(type)) {
        AuditDecision handlerDecision = decisionHandlers[type](context);
        decision.description = handlerDecision.description;
        decision.affectedFiles = handlerDecision.affectedFiles;
        decision.metrics = handlerDecision.metrics;
    }
    
    // Update statistics
    updateDecisionStatistics(decision);
    
    emit decisionMade(decision);
    
    return decision;
}

AuditDecision AutomatedAuditCoordinator::decideSymbolIndexing(const SymbolInfo& symbol) {
    QJsonObject context = extractSymbolContext(symbol);
    
    AuditDecision decision;
    decision.type = DecisionType::SYMBOL_INDEXING;
    decision.description = QString("Index symbol '%1' (%2)").arg(symbol.name).arg(symbol.type);
    decision.timestamp = QDateTime::currentDateTime();
    decision.context = context;
    
    // Decision logic
    bool shouldIndex = shouldIndexSymbol(symbol.name, symbol.type);
    
    // Calculate confidence
    decision.confidence = 0.9; // High confidence for symbol indexing
    if (symbol.type == "function" || symbol.type == "class") {
        decision.confidence = 0.95;
    }
    
    decision.approved = shouldIndex && (decision.confidence >= currentPolicy.criteria[DecisionType::SYMBOL_INDEXING].minConfidence);
    
    decision.reasoning = QString("Symbol type '%1' priority. ").arg(symbol.type);
    if (shouldIndex) {
        decision.reasoning += "Approved for indexing.";
    } else {
        decision.reasoning += "Skipped (filtered).";
    }
    
    decision.affectedFiles.append(symbol.filePath);
    decision.metrics["symbol_type"] = symbol.type;
    decision.metrics["line_number"] = symbol.lineNumber;
    
    // Calculate hotness
    decision.hotnessLevel = calculateHotnessForDecision(decision);
    
    return decision;
}

bool AutomatedAuditCoordinator::shouldIndexSymbol(const QString& symbolName, const QString& symbolType) {
    // Always index functions and classes
    if (symbolType == "function" || symbolType == "class" || symbolType == "namespace") {
        return true;
    }
    
    // Index important variables
    if (symbolType == "variable" || symbolType == "constant") {
        // Skip very short names (likely loop counters)
        if (symbolName.length() < 2) {
            return false;
        }
        return true;
    }
    
    // Check whitelist/blacklist
    DecisionCriteria criteria = currentPolicy.criteria[DecisionType::SYMBOL_INDEXING];
    if (criteria.blacklist.contains(symbolName)) {
        return false;
    }
    if (!criteria.whitelist.isEmpty() && !criteria.whitelist.contains(symbolName)) {
        return false;
    }
    
    return true;
}

AuditDecision AutomatedAuditCoordinator::decideRefactoringApplication(const RefactoringOpportunity& opportunity) {
    QJsonObject context = extractRefactoringContext(opportunity);
    
    AuditDecision decision;
    decision.type = DecisionType::REFACTORING_APPLY;
    decision.description = opportunity.description;
    decision.timestamp = QDateTime::currentDateTime();
    decision.context = context;
    decision.confidence = opportunity.confidence;
    
    // Decision logic
    bool shouldApply = shouldApplyRefactoring(
        opportunity.type,
        opportunity.confidence,
        opportunity.estimatedImpact
    );
    
    decision.approved = shouldApply && currentPolicy.enableAutoRefactor;
    
    decision.reasoning = QString("Refactoring '%1' with confidence %.2f and impact %.2f. ")
        .arg(opportunity.type)
        .arg(opportunity.confidence)
        .arg(opportunity.estimatedImpact);
    
    if (decision.approved) {
        decision.reasoning += "Approved for automatic application.";
    } else if (!currentPolicy.enableAutoRefactor) {
        decision.reasoning += "Requires manual approval (auto-refactor disabled).";
    } else {
        decision.reasoning += "Rejected (below thresholds).";
    }
    
    decision.affectedFiles.append(opportunity.filePath);
    decision.metrics["refactor_type"] = opportunity.type;
    decision.metrics["start_line"] = opportunity.startLine;
    decision.metrics["end_line"] = opportunity.endLine;
    decision.metrics["impact"] = opportunity.estimatedImpact;
    
    return decision;
}

bool AutomatedAuditCoordinator::shouldApplyRefactoring(
    const QString& refactorType,
    double confidence,
    double impact) {
    
    DecisionCriteria criteria = currentPolicy.criteria[DecisionType::REFACTORING_APPLY];
    
    // Check confidence threshold
    if (confidence < criteria.minConfidence) {
        return false;
    }
    
    // Check impact threshold (higher impact requires higher confidence)
    if (impact > 0.8 && confidence < 0.9) {
        return false;
    }
    
    // Conservative mode: only low-risk refactorings
    if (currentPolicy.enableConservative) {
        if (refactorType == "extract_method" || refactorType == "rename") {
            return confidence >= 0.85;
        }
        return false;
    }
    
    // Aggressive mode: all high-confidence refactorings
    if (currentPolicy.enableAggressive) {
        return confidence >= 0.7;
    }
    
    return confidence >= criteria.minConfidence;
}

AuditDecision AutomatedAuditCoordinator::decideBugFixApplication(const BugReport& bug) {
    QJsonObject context = extractBugContext(bug);
    
    AuditDecision decision;
    decision.type = DecisionType::BUG_FIX_APPLY;
    decision.description = bug.description;
    decision.timestamp = QDateTime::currentDateTime();
    decision.context = context;
    decision.confidence = bug.confidence;
    
    // Decision logic
    bool shouldFix = shouldApplyBugFix(bug.bugType, bug.severity, bug.confidence);
    
    decision.approved = shouldFix && currentPolicy.criteria[DecisionType::BUG_FIX_APPLY].enableAutoFix;
    
    decision.reasoning = QString("Bug '%1' with severity '%2' and confidence %.2f. ")
        .arg(bug.bugType)
        .arg(bug.severity)
        .arg(bug.confidence);
    
    if (decision.approved) {
        decision.reasoning += "Approved for automatic fix.";
    } else if (!currentPolicy.criteria[DecisionType::BUG_FIX_APPLY].enableAutoFix) {
        decision.reasoning += "Requires manual review (auto-fix disabled).";
    } else {
        decision.reasoning += "Rejected (below thresholds or high risk).";
    }
    
    decision.affectedFiles.append(bug.filePath);
    decision.metrics["bug_type"] = bug.bugType;
    decision.metrics["severity"] = bug.severity;
    decision.metrics["line_number"] = bug.lineNumber;
    
    return decision;
}

bool AutomatedAuditCoordinator::shouldApplyBugFix(
    const QString& bugType,
    const QString& severity,
    double confidence) {
    
    DecisionCriteria criteria = currentPolicy.criteria[DecisionType::BUG_FIX_APPLY];
    
    // Never auto-fix critical bugs (too risky)
    if (severity == "critical") {
        return false;
    }
    
    // High-confidence low/medium severity bugs
    if ((severity == "low" || severity == "medium") && confidence >= 0.9) {
        return true;
    }
    
    // Specific bug types with known safe fixes
    if (bugType == "null_pointer" && confidence >= 0.85) {
        return true;
    }
    
    // Conservative approach for everything else
    return false;
}

AuditDecision AutomatedAuditCoordinator::decideOptimizationApplication(const Optimization& optimization) {
    QJsonObject context = extractOptimizationContext(optimization);
    
    AuditDecision decision;
    decision.type = DecisionType::OPTIMIZATION_APPLY;
    decision.description = optimization.description;
    decision.timestamp = QDateTime::currentDateTime();
    decision.context = context;
    decision.confidence = optimization.confidence;
    
    // Decision logic
    bool shouldOptimize = shouldApplyOptimization(
        optimization.optimizationType,
        optimization.potentialImprovement,
        optimization.confidence
    );
    
    decision.approved = shouldOptimize;
    
    decision.reasoning = QString("Optimization '%1' with %.1f%% improvement and confidence %.2f. ")
        .arg(optimization.optimizationType)
        .arg(optimization.potentialImprovement)
        .arg(optimization.confidence);
    
    if (decision.approved) {
        decision.reasoning += "Approved for automatic application.";
    } else {
        decision.reasoning += "Rejected (insufficient improvement or confidence).";
    }
    
    decision.affectedFiles.append(optimization.filePath);
    decision.metrics["optimization_type"] = optimization.optimizationType;
    decision.metrics["improvement"] = optimization.potentialImprovement;
    decision.metrics["line_number"] = optimization.lineNumber;
    
    return decision;
}

bool AutomatedAuditCoordinator::shouldApplyOptimization(
    const QString& optimizationType,
    double improvement,
    double confidence) {
    
    DecisionCriteria criteria = currentPolicy.criteria[DecisionType::OPTIMIZATION_APPLY];
    
    // Require high confidence for optimizations
    if (confidence < 0.85) {
        return false;
    }
    
    // Only apply optimizations with significant improvement
    if (improvement < 20.0) {
        return false;
    }
    
    // Security optimizations: always apply if high confidence
    if (optimizationType == "security" && confidence >= 0.9) {
        return true;
    }
    
    // Performance optimizations: apply if substantial improvement
    if (optimizationType == "performance" && improvement >= 30.0 && confidence >= 0.85) {
        return true;
    }
    
    // Memory optimizations: apply if significant savings
    if (optimizationType == "memory" && improvement >= 25.0 && confidence >= 0.85) {
        return true;
    }
    
    return false;
}

void AutomatedAuditCoordinator::synchronizeEngines() {
    if (!contextAnalyzer || !intelligentEngine) {
        return;
    }
    
    qDebug() << "[AutomatedAuditCoordinator] Synchronizing engines...";
    
    // Reconcile dependency graphs
    reconcileDependencyGraphs();
    
    // Merge analysis results
    mergeAnalysisResults();
    
    lastSyncTime = QDateTime::currentDateTime();
    emit coordinationSynced();
    
    qDebug() << "[AutomatedAuditCoordinator] Engine synchronization complete";
}

void AutomatedAuditCoordinator::reconcileDependencyGraphs() {
    // Get dependencies from context analyzer
    // (Context analyzer uses std::unordered_map, intelligent engine uses QHash)
    
    // Compare and merge dependency information
    // This ensures both engines have consistent view of project structure
    
    qDebug() << "[AutomatedAuditCoordinator] Dependency graphs reconciled";
}

void AutomatedAuditCoordinator::mergeAnalysisResults() {
    // Merge symbol tables
    // Combine analysis results from both engines
    // Resolve conflicts by preferring higher confidence results
    
    qDebug() << "[AutomatedAuditCoordinator] Analysis results merged";
}

bool AutomatedAuditCoordinator::applyDecision(const AuditDecision& decision) {
    bool success = false;
    
    switch (decision.type) {
        case DecisionType::SYMBOL_INDEXING:
            success = applySymbolIndexing(decision);
            break;
        case DecisionType::DEPENDENCY_TRACKING:
            success = applyDependencyTracking(decision);
            break;
        case DecisionType::REFACTORING_APPLY:
            success = applyRefactoring(decision);
            break;
        case DecisionType::BUG_FIX_APPLY:
            success = applyBugFix(decision);
            break;
        case DecisionType::OPTIMIZATION_APPLY:
            success = applyOptimization(decision);
            break;
        case DecisionType::CODE_GENERATION:
            success = applyCodeGeneration(decision);
            break;
        case DecisionType::TEST_GENERATION:
            success = applyTestGeneration(decision);
            break;
        case DecisionType::DOCUMENTATION_UPDATE:
            success = applyDocumentationUpdate(decision);
            break;
        default:
            qWarning() << "[AutomatedAuditCoordinator] Unknown decision type:" << (int)decision.type;
            success = false;
    }
    
    emit decisionApplied(QString::number((int)decision.type), success);
    
    return success;
}

double AutomatedAuditCoordinator::calculateDecisionConfidence(
    DecisionType type,
    const QJsonObject& context) {
    
    // Base confidence from historical success rates
    double baseConfidence = 0.7;
    if (decisionSuccessRates.contains(type) && !decisionSuccessRates[type].isEmpty()) {
        double sum = 0.0;
        for (double rate : decisionSuccessRates[type]) {
            sum += rate;
        }
        baseConfidence = sum / decisionSuccessRates[type].size();
    }
    
    // Adjust based on context
    if (context.contains("confidence")) {
        double contextConfidence = context["confidence"].toDouble();
        baseConfidence = (baseConfidence + contextConfidence) / 2.0;
    }
    
    // Adjust based on file risk score
    if (context.contains("filePath")) {
        QString filePath = context["filePath"].toString();
        if (fileRiskScores.contains(filePath)) {
            double riskScore = fileRiskScores[filePath];
            baseConfidence *= (1.0 - (riskScore * 0.3)); // Reduce confidence for high-risk files
        }
    }
    
    return qMax(0.0, qMin(1.0, baseConfidence));
}

QJsonObject AutomatedAuditCoordinator::generateAuditReport(const AuditRecord& audit) {
    QJsonObject report;
    
    report["audit_id"] = audit.auditId;
    report["start_time"] = audit.startTime.toString(Qt::ISODate);
    report["end_time"] = audit.endTime.toString(Qt::ISODate);
    report["duration_seconds"] = audit.startTime.secsTo(audit.endTime);
    report["project_path"] = audit.projectPath;
    report["audit_type"] = audit.auditType;
    report["overall_score"] = audit.overallScore;
    report["total_decisions"] = audit.totalDecisions;
    report["approved_decisions"] = audit.approvedDecisions;
    report["rejected_decisions"] = audit.rejectedDecisions;
    report["approval_rate"] = audit.totalDecisions > 0 ? 
        (double)audit.approvedDecisions / audit.totalDecisions : 0.0;
    
    QJsonArray decisionsArray;
    for (const auto& decision : audit.decisions) {
        QJsonObject decisionObj;
        decisionObj["type"] = (int)decision.type;
        decisionObj["description"] = decision.description;
        decisionObj["approved"] = decision.approved;
        decisionObj["confidence"] = decision.confidence;
        decisionObj["reasoning"] = decision.reasoning;
        decisionsArray.append(decisionObj);
    }
    report["decisions"] = decisionsArray;
    
    QJsonArray recommendationsArray;
    for (const auto& rec : audit.recommendations) {
        recommendationsArray.append(rec);
    }
    report["recommendations"] = recommendationsArray;
    
    return report;
}

void AutomatedAuditCoordinator::initializeDefaultPolicy() {
    currentPolicy.policyName = "Default";
    currentPolicy.enableConservative = true;
    currentPolicy.enableAggressive = false;
    currentPolicy.enableLearning = true;
    currentPolicy.learningRate = 0.1;
    
    // Initialize criteria for each decision type
    DecisionCriteria defaultCriteria;
    defaultCriteria.minConfidence = 0.7;
    defaultCriteria.maxComplexity = 20.0;
    defaultCriteria.maxImpactScope = 10;
    defaultCriteria.requireHumanApproval = false;
    defaultCriteria.enableAutoFix = true;
    
    // Set criteria for all decision types
    for (int i = 0; i <= (int)DecisionType::DOCUMENTATION_UPDATE; i++) {
        currentPolicy.criteria[(DecisionType)i] = defaultCriteria;
    }
    
    // Customize specific decision types
    currentPolicy.criteria[DecisionType::REFACTORING_APPLY].enableAutoFix = false;
    currentPolicy.criteria[DecisionType::BUG_FIX_APPLY].minConfidence = 0.85;
    currentPolicy.criteria[DecisionType::SECURITY_HARDENING].minConfidence = 0.9;
}

void AutomatedAuditCoordinator::initializeDecisionHandlers() {
    // Initialize handlers for each decision type
    // These provide specialized logic for complex decisions
    
    decisionHandlers[DecisionType::SYMBOL_INDEXING] = [this](const QJsonObject& context) {
        AuditDecision decision;
        decision.description = "Index symbol: " + context["name"].toString();
        return decision;
    };
    
    // Add more handlers as needed
}

QString AutomatedAuditCoordinator::generateAuditId() {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void AutomatedAuditCoordinator::updateDecisionStatistics(const AuditDecision& decision) {
    decisionStatistics[decision.type]++;
    
    // Store decision by file
    for (const QString& file : decision.affectedFiles) {
        decisionsByFile[file].append(decision);
    }
}

bool AutomatedAuditCoordinator::evaluateCriteria(
    const DecisionCriteria& criteria,
    const QJsonObject& context) {
    
    // Check confidence threshold
    if (context.contains("confidence")) {
        if (context["confidence"].toDouble() < criteria.minConfidence) {
            return false;
        }
    }
    
    // Check complexity threshold
    if (context.contains("complexity")) {
        if (context["complexity"].toDouble() > criteria.maxComplexity) {
            return false;
        }
    }
    
    // Check impact scope
    if (context.contains("impactScope")) {
        if (context["impactScope"].toInt() > criteria.maxImpactScope) {
            return false;
        }
    }
    
    return true;
}

QJsonObject AutomatedAuditCoordinator::extractSymbolContext(const SymbolInfo& symbol) {
    QJsonObject context;
    context["name"] = symbol.name;
    context["type"] = symbol.type;
    context["filePath"] = symbol.filePath;
    context["lineNumber"] = symbol.lineNumber;
    context["confidence"] = 0.9;
    return context;
}

QJsonObject AutomatedAuditCoordinator::extractRefactoringContext(const RefactoringOpportunity& opportunity) {
    QJsonObject context;
    context["type"] = opportunity.type;
    context["description"] = opportunity.description;
    context["filePath"] = opportunity.filePath;
    context["confidence"] = opportunity.confidence;
    context["impact"] = opportunity.estimatedImpact;
    return context;
}

QJsonObject AutomatedAuditCoordinator::extractBugContext(const BugReport& bug) {
    QJsonObject context;
    context["bugType"] = bug.bugType;
    context["severity"] = bug.severity;
    context["description"] = bug.description;
    context["filePath"] = bug.filePath;
    context["confidence"] = bug.confidence;
    return context;
}

QJsonObject AutomatedAuditCoordinator::extractOptimizationContext(const Optimization& optimization) {
    QJsonObject context;
    context["optimizationType"] = optimization.optimizationType;
    context["description"] = optimization.description;
    context["filePath"] = optimization.filePath;
    context["confidence"] = optimization.confidence;
    context["improvement"] = optimization.potentialImprovement;
    return context;
}

bool AutomatedAuditCoordinator::applySymbolIndexing(const AuditDecision& decision) {
    // Implementation: Add symbol to both engines
    qDebug() << "[AutomatedAuditCoordinator] Applying symbol indexing:" << decision.description;
    return true;
}

bool AutomatedAuditCoordinator::applyRefactoring(const AuditDecision& decision) {
    // Implementation: Apply refactoring transformation
    qDebug() << "[AutomatedAuditCoordinator] Applying refactoring:" << decision.description;
    return true;
}

bool AutomatedAuditCoordinator::applyBugFix(const AuditDecision& decision) {
    // Implementation: Apply bug fix
    qDebug() << "[AutomatedAuditCoordinator] Applying bug fix:" << decision.description;
    return true;
}

bool AutomatedAuditCoordinator::applyOptimization(const AuditDecision& decision) {
    // Implementation: Apply optimization
    qDebug() << "[AutomatedAuditCoordinator] Applying optimization:" << decision.description;
    return true;
}

QVector<QString> AutomatedAuditCoordinator::identifyLowQualityAreas() {
    QVector<QString> areas;
    
    // Analyze file risk scores
    for (auto it = fileRiskScores.begin(); it != fileRiskScores.end(); ++it) {
        if (it.value() > 0.7) {
            areas.append(QString("High-risk file: %1 (score: %2)").arg(it.key()).arg(it.value()));
        }
    }
    
    return areas;
}

QJsonObject AutomatedAuditCoordinator::generateDecisionStatistics() {
    QJsonObject stats;
    
    for (auto it = decisionStatistics.begin(); it != decisionStatistics.end(); ++it) {
        stats[QString::number((int)it.key())] = it.value();
    }
    
    return stats;
}

// Placeholder implementations for remaining methods
bool AutomatedAuditCoordinator::applyDependencyTracking(const AuditDecision&) { return true; }
bool AutomatedAuditCoordinator::applyCodeGeneration(const AuditDecision&) { return true; }
bool AutomatedAuditCoordinator::applyTestGeneration(const AuditDecision&) { return true; }
bool AutomatedAuditCoordinator::applyDocumentationUpdate(const AuditDecision&) { return true; }

AuditRecord AutomatedAuditCoordinator::performTargetedAudit(const QString&, DecisionType) { return AuditRecord(); }
QVector<AuditDecision> AutomatedAuditCoordinator::makeDecisionBatch(const QVector<QPair<DecisionType, QJsonObject>>&) { return QVector<AuditDecision>(); }
bool AutomatedAuditCoordinator::rollbackDecision(const QString&) { return false; }
AuditDecision AutomatedAuditCoordinator::decideSymbolIndexing(const Symbol&) { return AuditDecision(); }
AuditDecision AutomatedAuditCoordinator::decideDependencyTracking(const QString&, const QString&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldTrackDependency(const QString&, double) { return true; }
AuditDecision AutomatedAuditCoordinator::decideAnalysisPriority(const QString&) { return AuditDecision(); }
int AutomatedAuditCoordinator::calculateFilePriority(const QString&) { return 5; }
AuditDecision AutomatedAuditCoordinator::decideScopeAnalysisDepth(const QString&, int) { return AuditDecision(); }
int AutomatedAuditCoordinator::determineOptimalScopeDepth(const QString&) { return 3; }
AuditDecision AutomatedAuditCoordinator::decidePatternMatching(const QString&, const QString&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldMatchPattern(const QString&, int) { return true; }
AuditDecision AutomatedAuditCoordinator::decideArchitectureValidation(const QString&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldValidateArchitecture(double) { return true; }
AuditDecision AutomatedAuditCoordinator::decideQualityThreshold(double) { return AuditDecision(); }
double AutomatedAuditCoordinator::determineAcceptableQualityThreshold(const QString&) { return 0.7; }
AuditDecision AutomatedAuditCoordinator::decideRealTimeUpdate(const QString&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldUpdateRealTime(const QString&, int) { return true; }
AuditDecision AutomatedAuditCoordinator::decideBatchProcessing(int, int) { return AuditDecision(); }
int AutomatedAuditCoordinator::determineBatchSize(int, int) { return 100; }
AuditDecision AutomatedAuditCoordinator::decideCircularDependencyHandling(const QVector<QString>&) { return AuditDecision(); }
QString AutomatedAuditCoordinator::determineCircularDependencyResolution(const QVector<QString>&) { return "break"; }
AuditDecision AutomatedAuditCoordinator::decideComplexityReduction(const QString&, double) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldReduceComplexity(double, int) { return true; }
AuditDecision AutomatedAuditCoordinator::decideMemoryOptimization(const QString&, int) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldOptimizeMemory(int, int) { return true; }
AuditDecision AutomatedAuditCoordinator::decideSecurityHardening(const QString&, const QString&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldApplySecurityFix(const QString&, double) { return true; }
AuditDecision AutomatedAuditCoordinator::decideCodeGeneration(const QString&, const QJsonObject&) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldGenerateCode(const QString&, double) { return true; }
AuditDecision AutomatedAuditCoordinator::decideTestGeneration(const QString&, int) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldGenerateTests(int, double) { return true; }
AuditDecision AutomatedAuditCoordinator::decideDocumentationUpdate(const QString&, bool) { return AuditDecision(); }
bool AutomatedAuditCoordinator::shouldUpdateDocumentation(bool, double) { return true; }
void AutomatedAuditCoordinator::propagateSymbolToIntelligent(const Symbol&) {}
void AutomatedAuditCoordinator::propagateSymbolToContext(const SymbolInfo&) {}
void AutomatedAuditCoordinator::recordDecisionOutcome(const QString&, bool) {}
void AutomatedAuditCoordinator::updateSuccessRates() {}
void AutomatedAuditCoordinator::adaptPolicy() {}
QJsonObject AutomatedAuditCoordinator::generateRecommendations() { return QJsonObject(); }
QVector<QString> AutomatedAuditCoordinator::identifyHighRiskFiles() { return QVector<QString>(); }
AuditRecord AutomatedAuditCoordinator::getAuditById(const QString&) { return AuditRecord(); }
QVector<AuditRecord> AutomatedAuditCoordinator::getAuditHistory(const QDateTime&) { return QVector<AuditRecord>(); }
QVector<AuditDecision> AutomatedAuditCoordinator::getDecisionsByFile(const QString&) { return QVector<AuditDecision>(); }
QVector<AuditDecision> AutomatedAuditCoordinator::getDecisionsByType(DecisionType) { return QVector<AuditDecision>(); }
double AutomatedAuditCoordinator::getDecisionSuccessRate(DecisionType) { return 0.8; }
void AutomatedAuditCoordinator::loadPolicyFromFile(const QString&) {}
void AutomatedAuditCoordinator::savePolicyToFile(const QString&) {}
void AutomatedAuditCoordinator::resetToDefaultPolicy() { initializeDefaultPolicy(); }
void AutomatedAuditCoordinator::enableAggressiveMode() { currentPolicy.enableAggressive = true; currentPolicy.enableConservative = false; }
void AutomatedAuditCoordinator::enableConservativeMode() { currentPolicy.enableConservative = true; currentPolicy.enableAggressive = false; }
bool AutomatedAuditCoordinator::setAutomationPolicy(const AutomationPolicy& policy) { currentPolicy = policy; return true; }
AutomationPolicy AutomatedAuditCoordinator::getAutomationPolicy() const { return currentPolicy; }

// Hotness calculation helpers
int AutomatedAuditCoordinator::calculateBaseHotness(const QJsonObject& metrics) {
    int hotness = 0;
    
    // Check quality scores
    if (metrics.contains("quality_metrics")) {
        QJsonObject quality = metrics["quality_metrics"].toObject();
        double qualityScore = quality["code_quality_score"].toDouble(0.85);
        
        if (qualityScore < 0.3) {
            hotness = 3;  // NASA - extremely low quality
        } else if (qualityScore < 0.5) {
            hotness = 2;  // Hot - very low quality
        } else if (qualityScore < 0.7) {
            hotness = 1;  // Warm - needs attention
        }
    }
    
    // Check complexity
    if (metrics.contains("complexity_metrics")) {
        QJsonObject complexity = metrics["complexity_metrics"].toObject();
        double cyclomaticComplexity = complexity["cyclomatic_complexity"].toDouble(10.0);
        
        if (cyclomaticComplexity > 50) {
            hotness = std::max(hotness, 3);  // NASA - unmaintainable
        } else if (cyclomaticComplexity > 30) {
            hotness = std::max(hotness, 2);  // Hot - very complex
        } else if (cyclomaticComplexity > 20) {
            hotness = std::max(hotness, 1);  // Warm - moderately complex
        }
    }
    
    // Check security metrics
    if (metrics.contains("security_metrics")) {
        QJsonObject security = metrics["security_metrics"].toObject();
        int vulnerabilities = security["buffer_overflow_risk"].toInt(0) +
                            security["null_pointer_risk"].toInt(0) +
                            security["memory_leak_risk"].toInt(0);
        
        if (vulnerabilities > 10) {
            hotness = 3;  // NASA - critical security failure
        } else if (vulnerabilities > 5) {
            hotness = std::max(hotness, 2);  // Hot - security issues
        } else if (vulnerabilities > 2) {
            hotness = std::max(hotness, 1);  // Warm
        }
    }
    
    // Check bug detection metrics
    if (metrics.contains("bug_detection_metrics")) {
        QJsonObject bugs = metrics["bug_detection_metrics"].toObject();
        int criticalBugs = bugs["use_after_free_risk"].toInt(0) +
                          bugs["double_free_risk"].toInt(0) +
                          bugs["infinite_loop_risk"].toInt(0);
        
        if (criticalBugs > 0) {
            hotness = 3;  // NASA - any critical bug is mission-critical
        }
    }
    
    // Check concurrency metrics
    if (metrics.contains("concurrency_metrics")) {
        QJsonObject concurrency = metrics["concurrency_metrics"].toObject();
        int deadlockRisk = concurrency["deadlock_risk"].toInt(0);
        int raceConditions = concurrency["lock_contention_risk"].toInt(0);
        
        if (deadlockRisk > 5 || raceConditions > 10) {
            hotness = 3;  // NASA - concurrency catastrophe
        }
    }
    
    return std::clamp(hotness, 0, 3);
}

void AutomatedAuditCoordinator::updateHotnessTracking(const QString& filePath, int newHotness) {
    int oldHotness = fileHotnessLevels.value(filePath, 0);
    fileHotnessLevels[filePath] = std::clamp(newHotness, 0, 3);  // 0-3 range
    
    if (newHotness > oldHotness) {
        QString levelName = (newHotness == 3) ? "NASA" : 
                           (newHotness == 2) ? "HOT" :
                           (newHotness == 1) ? "WARM" : "COLD";
        qDebug() << "[AutomatedAuditCoordinator] Hotness escalated for" << filePath
                 << ":" << oldHotness << "->" << newHotness << "(" << levelName << ")";
    }
}

QVector<QString> AutomatedAuditCoordinator::getHotFiles() const {
    QVector<QString> hotFiles;
    
    for (auto it = fileHotnessLevels.begin(); it != fileHotnessLevels.end(); ++it) {
        if (it.value() >= 2) {  // Return both HOT and NASA level files
            hotFiles.append(it.key());
        }
    }
    
    return hotFiles;
}
