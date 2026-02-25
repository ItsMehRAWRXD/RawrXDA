/**
 * Autonomous Codebase Refactorer
 * Leverages BigDaddyG-Engine orchestration capabilities for intelligent code refactoring
 */

import { EventEmitter } from 'events';
import { Agent } from '../MultiAgentOrchestrator';
import { CodeGenieIntegration } from '../codegenie/CodeGenieIntegration';
import { LearningGenerator } from '../learning/LearningGenerator';
import { SystematicLearningApproach } from '../learning/SystematicLearningApproach';

// Core Refactoring Interfaces
export interface CodebaseAnalysis {
  id: string;
  timestamp: number;
  language: string;
  files: FileAnalysis[];
  patterns: CodePattern[];
  metrics: CodebaseMetrics;
  issues: CodeIssue[];
  recommendations: RefactoringRecommendation[];
  complexity: ComplexityAnalysis;
  dependencies: DependencyGraph;
}

export interface FileAnalysis {
  path: string;
  language: string;
  size: number;
  lines: number;
  complexity: number;
  patterns: string[];
  issues: CodeIssue[];
  metrics: FileMetrics;
  dependencies: string[];
  lastModified: number;
}

export interface CodePattern {
  id: string;
  type: 'structural' | 'behavioral' | 'creational' | 'performance' | 'security';
  name: string;
  description: string;
  frequency: number;
  confidence: number;
  locations: PatternLocation[];
  impact: 'low' | 'medium' | 'high' | 'critical';
  refactoring: RefactoringStrategy;
}

export interface PatternLocation {
  file: string;
  line: number;
  column: number;
  context: string;
  severity: 'info' | 'warning' | 'error' | 'critical';
}

export interface CodeIssue {
  id: string;
  type: 'bug' | 'vulnerability' | 'performance' | 'maintainability' | 'style';
  severity: 'low' | 'medium' | 'high' | 'critical';
  message: string;
  file: string;
  line: number;
  column: number;
  fix: RefactoringAction;
  confidence: number;
  impact: ImpactAnalysis;
}

export interface RefactoringRecommendation {
  id: string;
  priority: number;
  type: 'optimization' | 'security' | 'maintainability' | 'performance' | 'architecture';
  title: string;
  description: string;
  benefits: string[];
  risks: string[];
  effort: 'low' | 'medium' | 'high';
  impact: ImpactAnalysis;
  actions: RefactoringAction[];
  prerequisites: string[];
  rollback: RollbackPlan;
}

export interface RefactoringAction {
  id: string;
  type: 'rename' | 'extract' | 'inline' | 'move' | 'split' | 'merge' | 'optimize' | 'secure';
  target: string;
  operation: string;
  parameters: Record<string, any>;
  validation: ValidationRule[];
  safety: SafetyCheck[];
}

export interface RefactoringStrategy {
  id: string;
  name: string;
  description: string;
  pattern: string;
  actions: RefactoringAction[];
  conditions: Condition[];
  priority: number;
  confidence: number;
  learning: LearningContext;
}

export interface CodebaseMetrics {
  totalFiles: number;
  totalLines: number;
  totalComplexity: number;
  averageComplexity: number;
  cyclomaticComplexity: number;
  maintainabilityIndex: number;
  technicalDebt: number;
  codeCoverage: number;
  testCoverage: number;
  documentationCoverage: number;
}

export interface FileMetrics {
  lines: number;
  complexity: number;
  maintainability: number;
  readability: number;
  testability: number;
  performance: number;
  security: number;
}

export interface ComplexityAnalysis {
  cyclomatic: number;
  cognitive: number;
  structural: number;
  temporal: number;
  spatial: number;
  overall: number;
  hotspots: ComplexityHotspot[];
}

export interface ComplexityHotspot {
  file: string;
  function: string;
  complexity: number;
  impact: number;
  refactoring: RefactoringStrategy;
}

export interface DependencyGraph {
  nodes: DependencyNode[];
  edges: DependencyEdge[];
  cycles: DependencyCycle[];
  metrics: DependencyMetrics;
}

export interface DependencyNode {
  id: string;
  name: string;
  type: 'file' | 'module' | 'class' | 'function';
  path: string;
  size: number;
  complexity: number;
  dependencies: string[];
  dependents: string[];
}

export interface DependencyEdge {
  from: string;
  to: string;
  type: 'import' | 'extend' | 'implement' | 'call' | 'use';
  strength: number;
  critical: boolean;
}

export interface DependencyCycle {
  nodes: string[];
  strength: number;
  impact: number;
  breaking: RefactoringAction[];
}

export interface DependencyMetrics {
  totalNodes: number;
  totalEdges: number;
  cycles: number;
  maxDepth: number;
  averageDepth: number;
  coupling: number;
  cohesion: number;
}

export interface ImpactAnalysis {
  files: string[];
  functions: string[];
  classes: string[];
  modules: string[];
  tests: string[];
  documentation: string[];
  breaking: boolean;
  risk: 'low' | 'medium' | 'high' | 'critical';
}

export interface ValidationRule {
  type: 'syntax' | 'semantic' | 'type' | 'style' | 'security' | 'performance';
  rule: string;
  message: string;
  severity: 'error' | 'warning' | 'info';
}

export interface SafetyCheck {
  type: 'backup' | 'test' | 'validation' | 'rollback' | 'approval';
  description: string;
  required: boolean;
  automated: boolean;
}

export interface RollbackPlan {
  id: string;
  description: string;
  steps: RollbackStep[];
  validation: ValidationRule[];
  automated: boolean;
}

export interface RollbackStep {
  action: string;
  target: string;
  parameters: Record<string, any>;
  validation: ValidationRule[];
}

export interface Condition {
  type: 'file' | 'function' | 'class' | 'pattern' | 'metric' | 'dependency';
  operator: 'equals' | 'contains' | 'matches' | 'greater' | 'less' | 'exists';
  value: any;
  description: string;
}

export interface LearningContext {
  domain: string;
  environment: string;
  constraints: string[];
  goals: string[];
  userPreferences: Record<string, any>;
}

// Autonomous Codebase Refactorer Class
export class AutonomousCodebaseRefactorer extends EventEmitter {
  private codeGenie: CodeGenieIntegration;
  private learningGenerator: LearningGenerator;
  private systematicApproach: SystematicLearningApproach;
  private analysisCache: Map<string, CodebaseAnalysis> = new Map();
  private refactoringHistory: RefactoringHistory[] = [];
  private safetyChecks: SafetyCheck[] = [];
  private rollbackPlans: Map<string, RollbackPlan> = new Map();

  constructor() {
    super();
    this.codeGenie = new CodeGenieIntegration();
    this.learningGenerator = new LearningGenerator();
    this.systematicApproach = new SystematicLearningApproach();
    this.initializeSafetyChecks();
    this.setupLearningIntegration();
  }

  private initializeSafetyChecks() {
    this.safetyChecks = [
      {
        type: 'backup',
        description: 'Create backup of original files',
        required: true,
        automated: true
      },
      {
        type: 'test',
        description: 'Run test suite before refactoring',
        required: true,
        automated: true
      },
      {
        type: 'validation',
        description: 'Validate syntax and semantics',
        required: true,
        automated: true
      },
      {
        type: 'rollback',
        description: 'Prepare rollback plan',
        required: true,
        automated: true
      },
      {
        type: 'approval',
        description: 'Get user approval for critical changes',
        required: false,
        automated: false
      }
    ];
  }

  private setupLearningIntegration() {
    this.learningGenerator.on('patternEvolved', (pattern) => {
      this.emit('refactoringPatternEvolved', pattern);
    });

    this.systematicApproach.on('learningComplete', (data) => {
      this.emit('refactoringLearningComplete', data);
    });
  }

  // Core Analysis Methods
  public async analyzeCodebase(
    rootPath: string,
    options: AnalysisOptions = {}
  ): Promise<CodebaseAnalysis> {
    console.log('🔍 Analyzing codebase:', rootPath);

    try {
      const analysisId = this.generateAnalysisId();
      const startTime = Date.now();

      // Discover files
      const files = await this.discoverFiles(rootPath, options);
      
      // Analyze each file
      const fileAnalyses = await this.analyzeFiles(files, options);
      
      // Detect patterns
      const patterns = await this.detectPatterns(fileAnalyses, options);
      
      // Calculate metrics
      const metrics = await this.calculateMetrics(fileAnalyses, options);
      
      // Identify issues
      const issues = await this.identifyIssues(fileAnalyses, patterns, options);
      
      // Generate recommendations
      const recommendations = await this.generateRecommendations(
        fileAnalyses, patterns, issues, metrics, options
      );
      
      // Analyze complexity
      const complexity = await this.analyzeComplexity(fileAnalyses, options);
      
      // Build dependency graph
      const dependencies = await this.buildDependencyGraph(fileAnalyses, options);

      const analysis: CodebaseAnalysis = {
        id: analysisId,
        timestamp: startTime,
        language: options.language || 'typescript',
        files: fileAnalyses,
        patterns,
        metrics,
        issues,
        recommendations,
        complexity,
        dependencies
      };

      this.analysisCache.set(analysisId, analysis);
      this.emit('analysisComplete', analysis);

      return analysis;

    } catch (error) {
      console.error('❌ Codebase analysis failed:', error);
      throw error;
    }
  }

  public async refactorCodebase(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    options: RefactoringOptions = {}
  ): Promise<RefactoringResult> {
    console.log('🔧 Refactoring codebase with strategy:', strategy.name);

    const refactoringId = this.generateRefactoringId();
    try {
      const startTime = Date.now();

      // Pre-refactoring safety checks
      await this.performSafetyChecks(analysis, strategy, options);
      
      // Create rollback plan
      const rollbackPlan = await this.createRollbackPlan(analysis, strategy, options);
      this.rollbackPlans.set(refactoringId, rollbackPlan);
      
      // Execute refactoring actions
      const results = await this.executeRefactoringActions(
        analysis, strategy, options
      );
      
      // Validate results
      const validation = await this.validateRefactoring(results, options);
      
      // Learn from refactoring
      await this.learnFromRefactoring(analysis, strategy, results, options);

      const result: RefactoringResult = {
        id: refactoringId,
        strategy: strategy.id,
        analysis: analysis.id,
        startTime,
        endTime: Date.now(),
        actions: results,
        validation,
        rollbackPlan,
        success: validation.success,
        metrics: this.calculateRefactoringMetrics(results),
        learning: await this.extractLearningInsights(results)
      };

      this.refactoringHistory.push({
        id: refactoringId,
        timestamp: startTime,
        strategy: strategy.name,
        analysis: analysis.id,
        result,
        rollbackPlan
      });

      this.emit('refactoringComplete', result);

      return result;

    } catch (error) {
      console.error('❌ Refactoring failed:', error);
      
      // Attempt rollback if available
      const rollbackPlan = this.rollbackPlans.get(refactoringId);
      if (rollbackPlan) {
        await this.executeRollback(rollbackPlan, options);
      }
      
      throw error;
    }
  }

  public async suggestRefactoring(
    analysis: CodebaseAnalysis,
    options: SuggestionOptions = {}
  ): Promise<RefactoringRecommendation[]> {
    console.log('💡 Generating refactoring suggestions');

    try {
      const suggestions: RefactoringRecommendation[] = [];

      // Pattern-based suggestions
      const patternSuggestions = await this.generatePatternBasedSuggestions(
        analysis.patterns, options
      );
      suggestions.push(...patternSuggestions);

      // Issue-based suggestions
      const issueSuggestions = await this.generateIssueBasedSuggestions(
        analysis.issues, options
      );
      suggestions.push(...issueSuggestions);

      // Metric-based suggestions
      const metricSuggestions = await this.generateMetricBasedSuggestions(
        analysis.metrics, options
      );
      suggestions.push(...metricSuggestions);

      // Complexity-based suggestions
      const complexitySuggestions = await this.generateComplexityBasedSuggestions(
        analysis.complexity, options
      );
      suggestions.push(...complexitySuggestions);

      // Dependency-based suggestions
      const dependencySuggestions = await this.generateDependencyBasedSuggestions(
        analysis.dependencies, options
      );
      suggestions.push(...dependencySuggestions);

      // Sort by priority and impact
      suggestions.sort((a, b) => {
        const priorityDiff = b.priority - a.priority;
        if (priorityDiff !== 0) return priorityDiff;
        return b.impact.risk.localeCompare(a.impact.risk);
      });

      this.emit('suggestionsGenerated', suggestions);

      return suggestions;

    } catch (error) {
      console.error('❌ Suggestion generation failed:', error);
      throw error;
    }
  }

  // Autonomous Refactoring Orchestration
  public async orchestrateAutonomousRefactoring(
    rootPath: string,
    goals: string[],
    constraints: string[] = [],
    options: OrchestrationOptions = {}
  ): Promise<AutonomousRefactoringResult> {
    console.log('🤖 Starting autonomous refactoring orchestration');

    try {
      const orchestrationId = this.generateOrchestrationId();
      const startTime = Date.now();

      // Analyze codebase
      const analysis = await this.analyzeCodebase(rootPath, options.analysis);
      
      // Generate suggestions
      const suggestions = await this.suggestRefactoring(analysis, options.suggestions);
      
      // Select optimal strategy
      const strategy = await this.selectOptimalStrategy(
        analysis, suggestions, goals, constraints, options
      );
      
      // Execute refactoring
      const result = await this.refactorCodebase(analysis, strategy, options.refactoring);
      
      // Validate and optimize
      const optimization = await this.optimizeRefactoring(result, options);
      
      // Generate report
      const report = await this.generateRefactoringReport(
        analysis, strategy, result, optimization, options
      );

      const autonomousResult: AutonomousRefactoringResult = {
        id: orchestrationId,
        startTime,
        endTime: Date.now(),
        analysis,
        suggestions,
        strategy,
        result,
        optimization,
        report,
        success: result.success && optimization.success,
        metrics: this.calculateAutonomousMetrics(analysis, result, optimization)
      };

      this.emit('autonomousRefactoringComplete', autonomousResult);

      return autonomousResult;

    } catch (error) {
      console.error('❌ Autonomous refactoring orchestration failed:', error);
      throw error;
    }
  }

  // Helper Methods
  private async discoverFiles(
    rootPath: string,
    options: AnalysisOptions
  ): Promise<string[]> {
    // Implementation for file discovery
    return [];
  }

  private async analyzeFiles(
    files: string[],
    options: AnalysisOptions
  ): Promise<FileAnalysis[]> {
    // Implementation for file analysis
    return [];
  }

  private async detectPatterns(
    fileAnalyses: FileAnalysis[],
    options: AnalysisOptions
  ): Promise<CodePattern[]> {
    // Implementation for pattern detection
    return [];
  }

  private async calculateMetrics(
    fileAnalyses: FileAnalysis[],
    options: AnalysisOptions
  ): Promise<CodebaseMetrics> {
    // Implementation for metrics calculation
    return {
      totalFiles: 0,
      totalLines: 0,
      totalComplexity: 0,
      averageComplexity: 0,
      cyclomaticComplexity: 0,
      maintainabilityIndex: 0,
      technicalDebt: 0,
      codeCoverage: 0,
      testCoverage: 0,
      documentationCoverage: 0
    };
  }

  private async identifyIssues(
    fileAnalyses: FileAnalysis[],
    patterns: CodePattern[],
    options: AnalysisOptions
  ): Promise<CodeIssue[]> {
    // Implementation for issue identification
    return [];
  }

  private async generateRecommendations(
    fileAnalyses: FileAnalysis[],
    patterns: CodePattern[],
    issues: CodeIssue[],
    metrics: CodebaseMetrics,
    options: AnalysisOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for recommendation generation
    return [];
  }

  private async analyzeComplexity(
    fileAnalyses: FileAnalysis[],
    options: AnalysisOptions
  ): Promise<ComplexityAnalysis> {
    // Implementation for complexity analysis
    return {
      cyclomatic: 0,
      cognitive: 0,
      structural: 0,
      temporal: 0,
      spatial: 0,
      overall: 0,
      hotspots: []
    };
  }

  private async buildDependencyGraph(
    fileAnalyses: FileAnalysis[],
    options: AnalysisOptions
  ): Promise<DependencyGraph> {
    // Implementation for dependency graph building
    return {
      nodes: [],
      edges: [],
      cycles: [],
      metrics: {
        totalNodes: 0,
        totalEdges: 0,
        cycles: 0,
        maxDepth: 0,
        averageDepth: 0,
        coupling: 0,
        cohesion: 0
      }
    };
  }

  private async performSafetyChecks(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    options: RefactoringOptions
  ): Promise<void> {
    // Implementation for safety checks
  }

  private async createRollbackPlan(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    options: RefactoringOptions
  ): Promise<RollbackPlan> {
    // Implementation for rollback plan creation
    return {
      id: this.generateId(),
      description: 'Rollback plan',
      steps: [],
      validation: [],
      automated: true
    };
  }

  private async executeRefactoringActions(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    options: RefactoringOptions
  ): Promise<RefactoringActionResult[]> {
    // Implementation for action execution
    return [];
  }

  private async validateRefactoring(
    results: RefactoringActionResult[],
    options: RefactoringOptions
  ): Promise<RefactoringValidation> {
    // Implementation for validation
    return {
      success: true,
      errors: [],
      warnings: [],
      metrics: {}
    };
  }

  private async learnFromRefactoring(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    results: RefactoringActionResult[],
    options: RefactoringOptions
  ): Promise<void> {
    // Implementation for learning
  }

  private async executeRollback(
    rollbackPlan: RollbackPlan,
    options: RefactoringOptions
  ): Promise<void> {
    // Implementation for rollback execution
  }

  private async generatePatternBasedSuggestions(
    patterns: CodePattern[],
    options: SuggestionOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for pattern-based suggestions
    return [];
  }

  private async generateIssueBasedSuggestions(
    issues: CodeIssue[],
    options: SuggestionOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for issue-based suggestions
    return [];
  }

  private async generateMetricBasedSuggestions(
    metrics: CodebaseMetrics,
    options: SuggestionOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for metric-based suggestions
    return [];
  }

  private async generateComplexityBasedSuggestions(
    complexity: ComplexityAnalysis,
    options: SuggestionOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for complexity-based suggestions
    return [];
  }

  private async generateDependencyBasedSuggestions(
    dependencies: DependencyGraph,
    options: SuggestionOptions
  ): Promise<RefactoringRecommendation[]> {
    // Implementation for dependency-based suggestions
    return [];
  }

  private async selectOptimalStrategy(
    analysis: CodebaseAnalysis,
    suggestions: RefactoringRecommendation[],
    goals: string[],
    constraints: string[],
    options: OrchestrationOptions
  ): Promise<RefactoringStrategy> {
    // Implementation for strategy selection
    return {
      id: this.generateId(),
      name: 'Default Strategy',
      description: 'Default refactoring strategy',
      pattern: 'default',
      actions: [],
      conditions: [],
      priority: 1,
      confidence: 0.8,
      learning: {
        domain: 'refactoring',
        environment: 'development',
        constraints: [],
        goals: [],
        userPreferences: {}
      }
    };
  }

  private async optimizeRefactoring(
    result: RefactoringResult,
    options: OrchestrationOptions
  ): Promise<RefactoringOptimization> {
    // Implementation for optimization
    return {
      success: true,
      improvements: [],
      metrics: {},
      recommendations: []
    };
  }

  private async generateRefactoringReport(
    analysis: CodebaseAnalysis,
    strategy: RefactoringStrategy,
    result: RefactoringResult,
    optimization: RefactoringOptimization,
    options: OrchestrationOptions
  ): Promise<RefactoringReport> {
    // Implementation for report generation
    return {
      id: this.generateId(),
      timestamp: Date.now(),
      analysis: analysis.id,
      strategy: strategy.id,
      result: result.id,
      optimization: optimization,
      summary: 'Refactoring report',
      details: {},
      metrics: {},
      recommendations: []
    };
  }

  private calculateRefactoringMetrics(results: RefactoringActionResult[]): RefactoringMetrics {
    // Implementation for metrics calculation
    return {
      actionsExecuted: results.length,
      successRate: 1.0,
      timeSaved: 0,
      complexityReduced: 0,
      maintainabilityImproved: 0,
      performanceImproved: 0,
      securityImproved: 0
    };
  }

  private async extractLearningInsights(results: RefactoringActionResult[]): Promise<LearningInsights> {
    // Implementation for learning insights extraction
    return {
      patterns: [],
      strategies: [],
      improvements: [],
      recommendations: []
    };
  }

  private calculateAutonomousMetrics(
    analysis: CodebaseAnalysis,
    result: RefactoringResult,
    optimization: RefactoringOptimization
  ): AutonomousMetrics {
    // Implementation for autonomous metrics calculation
    return {
      analysisTime: 0,
      refactoringTime: 0,
      optimizationTime: 0,
      totalTime: 0,
      efficiency: 0,
      accuracy: 0,
      impact: 0,
      satisfaction: 0
    };
  }

  private generateAnalysisId(): string {
    return `analysis_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private generateRefactoringId(): string {
    return `refactor_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private generateOrchestrationId(): string {
    return `orchestration_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  private generateId(): string {
    return `refactorer_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  // Public API Methods
  public getAnalysis(id: string): CodebaseAnalysis | undefined {
    return this.analysisCache.get(id);
  }

  public getRefactoringHistory(): RefactoringHistory[] {
    return [...this.refactoringHistory];
  }

  public getRollbackPlan(id: string): RollbackPlan | undefined {
    return this.rollbackPlans.get(id);
  }

  public async rollbackRefactoring(id: string): Promise<RollbackResult> {
    const rollbackPlan = this.rollbackPlans.get(id);
    if (!rollbackPlan) {
      throw new Error(`Rollback plan not found for refactoring: ${id}`);
    }

    try {
      await this.executeRollback(rollbackPlan, {});
      return {
        success: true,
        message: 'Rollback completed successfully',
        timestamp: Date.now()
      };
    } catch (error) {
      return {
        success: false,
        message: `Rollback failed: ${error}`,
        timestamp: Date.now()
      };
    }
  }
}

// Supporting Interfaces
export interface AnalysisOptions {
  language?: string;
  includeTests?: boolean;
  includeDocumentation?: boolean;
  maxDepth?: number;
  excludePatterns?: string[];
  includePatterns?: string[];
}

export interface RefactoringOptions {
  dryRun?: boolean;
  backup?: boolean;
  validate?: boolean;
  rollback?: boolean;
  approval?: boolean;
  maxChanges?: number;
  excludeFiles?: string[];
  includeFiles?: string[];
}

export interface SuggestionOptions {
  maxSuggestions?: number;
  minPriority?: number;
  includeTypes?: string[];
  excludeTypes?: string[];
  focusAreas?: string[];
}

export interface OrchestrationOptions {
  analysis?: AnalysisOptions;
  suggestions?: SuggestionOptions;
  refactoring?: RefactoringOptions;
  learning?: boolean;
  optimization?: boolean;
  reporting?: boolean;
}

export interface RefactoringResult {
  id: string;
  strategy: string;
  analysis: string;
  startTime: number;
  endTime: number;
  actions: RefactoringActionResult[];
  validation: RefactoringValidation;
  rollbackPlan: RollbackPlan;
  success: boolean;
  metrics: RefactoringMetrics;
  learning: LearningInsights;
}

export interface RefactoringActionResult {
  id: string;
  action: string;
  target: string;
  success: boolean;
  changes: RefactoringChange[];
  validation: RefactoringValidation;
  metrics: RefactoringMetrics;
}

export interface RefactoringChange {
  file: string;
  line: number;
  column: number;
  type: 'add' | 'remove' | 'modify' | 'move' | 'rename';
  oldValue: string;
  newValue: string;
  description: string;
}

export interface RefactoringValidation {
  success: boolean;
  errors: ValidationError[];
  warnings: ValidationWarning[];
  metrics: Record<string, number>;
}

export interface ValidationError {
  type: string;
  message: string;
  file: string;
  line: number;
  column: number;
  severity: 'error' | 'critical';
}

export interface ValidationWarning {
  type: string;
  message: string;
  file: string;
  line: number;
  column: number;
  severity: 'warning' | 'info';
}

export interface RefactoringMetrics {
  actionsExecuted: number;
  successRate: number;
  timeSaved: number;
  complexityReduced: number;
  maintainabilityImproved: number;
  performanceImproved: number;
  securityImproved: number;
}

export interface LearningInsights {
  patterns: string[];
  strategies: string[];
  improvements: string[];
  recommendations: string[];
}

export interface RefactoringHistory {
  id: string;
  timestamp: number;
  strategy: string;
  analysis: string;
  result: RefactoringResult;
  rollbackPlan: RollbackPlan;
}

export interface AutonomousRefactoringResult {
  id: string;
  startTime: number;
  endTime: number;
  analysis: CodebaseAnalysis;
  suggestions: RefactoringRecommendation[];
  strategy: RefactoringStrategy;
  result: RefactoringResult;
  optimization: RefactoringOptimization;
  report: RefactoringReport;
  success: boolean;
  metrics: AutonomousMetrics;
}

export interface RefactoringOptimization {
  success: boolean;
  improvements: string[];
  metrics: Record<string, number>;
  recommendations: string[];
}

export interface RefactoringReport {
  id: string;
  timestamp: number;
  analysis: string;
  strategy: string;
  result: string;
  optimization: RefactoringOptimization;
  summary: string;
  details: Record<string, any>;
  metrics: Record<string, number>;
  recommendations: string[];
}

export interface AutonomousMetrics {
  analysisTime: number;
  refactoringTime: number;
  optimizationTime: number;
  totalTime: number;
  efficiency: number;
  accuracy: number;
  impact: number;
  satisfaction: number;
}

export interface RollbackResult {
  success: boolean;
  message: string;
  timestamp: number;
}

// Export singleton instance
export const autonomousRefactorer = new AutonomousCodebaseRefactorer();
