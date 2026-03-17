/**
 * RawrXD Agentic IDE - Agent Orchestrator
 * Hybrid Planner/Coder/Reflector workflow for Win32
 * Integrates with OpenAI client for gpt-5.2-pro reasoning
 */

const OpenAIClient = require('../modules/openai-client');
const { EventEmitter } = require('events');
const fs = require('fs');
const path = require('path');

class AgentOrchestrator extends EventEmitter {
  constructor(config = {}) {
    super();
    this.openai = new OpenAIClient(config.openai || {});
    this.maxIterations = config.maxIterations || 10;
    this.agentTimeout = config.agentTimeout || 120000;
    this.enableFallback = config.enableFallback !== false;
    this.logPath = config.logPath || path.join(process.env.APPDATA || '.', 'RawrXD/logs');

    // Forward OpenAI events
    this.openai.on('debug', msg => this.emit('debug', msg));
    this.openai.on('info', msg => this.emit('info', msg));
    this.openai.on('warn', msg => this.emit('warn', msg));
    this.openai.on('error', msg => this.emit('error', msg));
  }

  /**
   * Execute full agentic workflow: Plan → Code → Verify
   * @param {string} objective - High-level goal
   * @param {Object} context - Task context (language, patterns, constraints)
   * @returns {Promise<Object>} Execution result with plan, code, and verification
   */
  async executeAgenticWorkflow(objective, context = {}) {
    const workflowId = this.generateId();
    const startTime = Date.now();

    this.emit('info', `[${workflowId}] Starting agentic workflow: ${objective}`);

    try {
      // Step 1: Planning
      this.emit('info', `[${workflowId}] Phase 1/3: Planning`);
      const plan = await this.openai.plan(objective, context.contextBlocks || []);

      if (!plan.steps || plan.steps.length === 0) {
        throw new Error('Planner failed to generate valid steps');
      }

      this.emit('info', `[${workflowId}] Plan generated: ${plan.steps.length} steps`);

      // Step 2: Code Generation with iterative refinement
      this.emit('info', `[${workflowId}] Phase 2/3: Code Generation`);
      let code = await this.generateCodeForPlan(plan, context, workflowId);

      // Step 3: Verification & Reflection
      this.emit('info', `[${workflowId}] Phase 3/3: Verification`);
      const verification = await this.verifyCode(code, plan, context, workflowId);

      // If verification failed and fallback enabled, attempt refinement
      if (!verification.passed && this.enableFallback && verification.improvements.length > 0) {
        this.emit('warn', `[${workflowId}] Verification failed, attempting refinement`);
        code = await this.refineCode(code, verification.improvements, context, workflowId);
        const secondVerification = await this.verifyCode(code, plan, context, workflowId);
        verification.refinementAttempts = 1;
        verification.finalPassed = secondVerification.passed;
      }

      const duration = Date.now() - startTime;
      const result = {
        workflowId,
        objective,
        status: verification.passed ? 'success' : 'partial',
        plan,
        code,
        verification,
        duration,
        timestamp: new Date().toISOString()
      };

      this.emit('info', `[${workflowId}] Workflow completed in ${duration}ms (${result.status})`);
      this.saveWorkflowLog(result);

      return result;
    } catch (error) {
      this.emit('error', `[${workflowId}] Workflow failed: ${error.message}`);
      this.saveWorkflowLog({
        workflowId,
        objective,
        status: 'failed',
        error: error.message,
        duration: Date.now() - startTime,
        timestamp: new Date().toISOString()
      });
      throw error;
    }
  }

  /**
   * Generate code iteratively from plan
   */
  async generateCodeForPlan(plan, context, workflowId) {
    let code = '';
    let iteration = 0;

    for (const step of plan.steps.slice(0, Math.min(plan.steps.length, this.maxIterations))) {
      iteration++;
      this.emit('debug', `[${workflowId}] Generating code for step ${iteration}/${plan.steps.length}: ${step.action}`);

      try {
        const spec = `
Step ${iteration}: ${step.action}
Reasoning: ${step.reasoning}
Context: ${JSON.stringify(context, null, 2)}
${code.length > 0 ? `\nExisting code:\n\`\`\`\n${code}\n\`\`\`` : ''}
        `;

        const stepCode = await this.openai.generateCode(
          spec,
          context.language || 'typescript',
          context.examples || []
        );

        code += '\n\n' + stepCode;

        this.emit('debug', `[${workflowId}] Step ${iteration} code generated (${stepCode.length} chars)`);
      } catch (error) {
        this.emit('warn', `[${workflowId}] Step ${iteration} generation failed: ${error.message}`);
        if (!this.enableFallback) throw error;
      }
    }

    return code;
  }

  /**
   * Verify generated code
   */
  async verifyCode(code, plan, context, workflowId) {
    const criteria = `
- Implements all steps from the plan
- Uses proper error handling and logging
- Follows ${context.language || 'typescript'} best practices
- Is production-ready and maintainable
- Includes appropriate comments
    `;

    try {
      const reflection = await this.openai.reflect(code, criteria);
      this.emit('debug', `[${workflowId}] Verification score: ${reflection.score || 'unknown'}`);
      return {
        passed: reflection.passed !== false && (reflection.score || 0) > 0.7,
        ...reflection
      };
    } catch (error) {
      this.emit('warn', `[${workflowId}] Verification failed: ${error.message}`);
      return { passed: false, error: error.message, improvements: [] };
    }
  }

  /**
   * Refine code based on verification feedback
   */
  async refineCode(code, improvements, context, workflowId) {
    const refinementPrompt = `
Current code has the following issues:
${improvements.map((i, idx) => `${idx + 1}. ${i}`).join('\n')}

Please fix these issues while maintaining all original functionality.
Language: ${context.language || 'typescript'}
    `;

    try {
      const refined = await this.openai.generateCode(refinementPrompt, context.language || 'typescript', [code]);
      this.emit('debug', `[${workflowId}] Code refined`);
      return refined;
    } catch (error) {
      this.emit('warn', `[${workflowId}] Refinement failed: ${error.message}`);
      return code; // Return original if refinement fails
    }
  }

  /**
   * Batch execute multiple objectives
   */
  async executeBatch(objectives, context = {}) {
    this.emit('info', `Starting batch execution: ${objectives.length} objectives`);
    const results = [];

    for (const objective of objectives) {
      try {
        const result = await this.executeAgenticWorkflow(objective, context);
        results.push({ objective, ...result });
      } catch (error) {
        results.push({ objective, status: 'failed', error: error.message });
      }
    }

    return results;
  }

  /**
   * Stream-based execution for real-time UI updates
   */
  async *executeAgenticWorkflowStreaming(objective, context = {}) {
    const workflowId = this.generateId();
    this.emit('info', `[${workflowId}] Starting streaming workflow`);

    try {
      yield { stage: 'planning', status: 'in-progress', workflowId };
      const plan = await this.openai.plan(objective, context.contextBlocks || []);
      yield { stage: 'planning', status: 'complete', plan, workflowId };

      yield { stage: 'coding', status: 'in-progress', workflowId };
      const code = await this.generateCodeForPlan(plan, context, workflowId);
      yield { stage: 'coding', status: 'complete', code, workflowId };

      yield { stage: 'verification', status: 'in-progress', workflowId };
      const verification = await this.verifyCode(code, plan, context, workflowId);
      yield { stage: 'verification', status: 'complete', verification, workflowId };

      yield {
        stage: 'complete',
        status: 'success',
        result: { plan, code, verification, workflowId }
      };
    } catch (error) {
      yield { stage: 'error', status: 'failed', error: error.message, workflowId };
    }
  }

  /**
   * Generate unique ID for tracking
   */
  generateId() {
    return `agent_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }

  /**
   * Save workflow execution log
   */
  saveWorkflowLog(result) {
    try {
      if (!fs.existsSync(this.logPath)) {
        fs.mkdirSync(this.logPath, { recursive: true });
      }
      const logFile = path.join(this.logPath, `workflow_${Date.now()}.json`);
      fs.writeFileSync(logFile, JSON.stringify(result, null, 2));
      this.emit('debug', `Workflow log saved to ${logFile}`);
    } catch (error) {
      this.emit('error', `Failed to save workflow log: ${error.message}`);
    }
  }
}

module.exports = AgentOrchestrator;
