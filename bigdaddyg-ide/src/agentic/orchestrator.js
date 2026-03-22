/**
 * Reject path traversal and absolute paths (project-relative only).
 * Does not: normalize Unicode paths or resolve symlinks — caller should pass repo-relative segments.
 */
function assertSafeRelativePath(relPath, label = 'path') {
  if (relPath == null || relPath === '') return;
  const s = String(relPath).replace(/\\/g, '/');
  if (s.includes('..')) {
    throw new Error(`${label}: path traversal not allowed`);
  }
  if (s.startsWith('/') || /^[a-zA-Z]:\//.test(s)) {
    throw new Error(`${label}: absolute paths not allowed`);
  }
}

/**
 * Extracts `{ steps: [...] }` from model text. Does not: validate step semantics — unknown types still flow to `normalizeStep`.
 * @returns {{ steps: object[], parseIssue: string | null }}
 */
function parsePlanResponse(content) {
  const fallbackAsk = (issue) => ({
    steps: [
      {
        type: 'ask_ai',
        description: `Planner reply was not usable JSON (${issue}). Try a shorter goal, confirm Settings › AI and the active provider, then check the main-process log for the raw planner output.`,
        prompt: 'Explain briefly what went wrong and propose a smaller follow-up goal.',
        status: 'pending'
      }
    ],
    parseIssue: issue
  });

  if (!content || typeof content !== 'string') {
    return fallbackAsk('empty or non-string content');
  }

  const trimmed = content.trim();
  const jsonMatch = trimmed.match(/\{[\s\S]*\}/);
  if (!jsonMatch) {
    return fallbackAsk('no JSON object found');
  }

  try {
    const parsed = JSON.parse(jsonMatch[0]);
    if (!Array.isArray(parsed.steps)) {
      return fallbackAsk('missing or invalid "steps" array');
    }
    return {
      steps: parsed.steps.map((s) => ({
        type: typeof s.type === 'string' ? s.type : 'ask_ai',
        description: typeof s.description === 'string' ? s.description : '',
        path: typeof s.path === 'string' ? s.path : undefined,
        prompt: typeof s.prompt === 'string' ? s.prompt : undefined,
        content: typeof s.content === 'string' ? s.content : undefined,
        maxHits: typeof s.maxHits === 'number' && Number.isFinite(s.maxHits) ? s.maxHits : undefined,
        risk: typeof s.risk === 'string' ? s.risk : undefined,
        mode: typeof s.mode === 'string' ? s.mode : undefined,
        agents: Array.isArray(s.agents) ? s.agents : undefined,
        goals: Array.isArray(s.goals) ? s.goals : undefined,
        command: typeof s.command === 'string' ? s.command : undefined,
        timeoutMs: typeof s.timeoutMs === 'number' && Number.isFinite(s.timeoutMs) ? s.timeoutMs : undefined
      })),
      parseIssue: null
    };
  } catch (e) {
    const hint = e && e.message ? e.message : 'JSON.parse failed';
    return fallbackAsk(hint);
  }
}

const DEFAULT_POLICY = {
  /** Master switch — when false, startTask throws (renderer also gates IPC). */
  autonomousEnabled: true,
  /** Skip human gate for read_file / list_dir / search_repo */
  autoApproveReads: true,
  /** When true, write_file / append_file run without pause (dangerous) */
  autoApproveWrites: false,
  /** When true, every ask_ai step waits for explicit approval */
  requireAskAiApproval: false,
  /** When true, planner may emit run_terminal; runtime must implement runShellCommand */
  allowShell: false,
  /** When true, planner may emit task (swarm) steps */
  swarmingEnabled: false,
  /** When true, run_terminal steps skip the approval gate (dangerous) */
  autoApproveTerminal: false,
  /** When true, task / swarm steps require explicit approval */
  requireSubagentApproval: true,
  /** Bumps maxPlanTokens / askAiMaxTokens (applied in startTask) */
  agentMaxMode: false,
  /** Extra planner + reflection token headroom */
  agentDeepThinking: false,
  /** Truncate workspace tree in planner prompt */
  agentContextLimitChars: 48000,
  /** Cap combined stdout+stderr capture for run_terminal */
  shellMaxOutputChars: 65536,
  /** Extra tokens for plan JSON */
  maxPlanTokens: 8192,
  /** Default cap for ask_ai / fallback invoke in executeStep */
  askAiMaxTokens: 4096,
  /** Reflection pass size */
  reflectionMaxTokens: 1024,
  /** One final LLM pass: did we satisfy the goal? */
  enableReflection: true,
  /** Max plan steps (truncate if model returns huge list) */
  maxSteps: 24,
  /** Max nesting depth for `task` steps (sub-agents spawning sub-agents) */
  maxSubtaskDepth: 3,
  /** Max sub-goals per single `task` step */
  maxSubAgentsPerStep: 6,
  /** Max time to wait for each sub-agent run (ms) */
  subtaskTimeoutMs: 3_600_000
};

function stepRiskTier(type) {
  const t = (type || '').toLowerCase();
  if (t === 'write_file' || t === 'append_file') return 'mutate';
  if (t === 'run_terminal' || t === 'run_shell') return 'exec';
  if (t === 'read_file' || t === 'list_dir' || t === 'search_repo') return 'read';
  if (t === 'task' || t === 'launch_agents') return 'cognitive';
  return 'cognitive';
}

/** @param {string} type */
function stepRiskLevelFromType(type) {
  const tier = stepRiskTier(type);
  if (tier === 'mutate' || tier === 'exec') return 'high';
  if (tier === 'read') return 'low';
  return 'medium';
}

/**
 * Plan → execute loop with optional human approval gates (main process).
 * Does not: change renderer IdeFeaturesContext or write `approval_policy.json` — policy is merged per-task from the caller.
 */
class AgentOrchestrator {
  /** @param {object} aiManager - AIProviderManager instance */
  constructor(aiManager, runtime = {}) {
    this.aiManager = aiManager;
    this.runtime = runtime;
    this.activeTasks = new Map();
    this.taskHistory = [];
  }

  /**
   * @param {string} goal
   * @param {Partial<typeof DEFAULT_POLICY>} [policy]
   */
  async startTask(goal, policy = {}) {
    const normalizedGoal = typeof goal === 'string' ? goal.trim() : '';
    if (!normalizedGoal) {
      throw new Error('Goal is required');
    }

    const taskId = this.generateTaskId();
    const mergedPolicy = { ...DEFAULT_POLICY, ...policy };
    if (mergedPolicy.autonomousEnabled === false) {
      throw new Error('Autonomous agent is disabled (autonomousEnabled=false)');
    }

    let maxPlan = Number(mergedPolicy.maxPlanTokens);
    if (!Number.isFinite(maxPlan) || maxPlan < 1024) maxPlan = DEFAULT_POLICY.maxPlanTokens;
    if (mergedPolicy.agentMaxMode) maxPlan = Math.min(32768, Math.floor(maxPlan * 2));
    if (mergedPolicy.agentDeepThinking) maxPlan = Math.min(32768, maxPlan + 4096);
    mergedPolicy.maxPlanTokens = maxPlan;

    let askTok = Number(mergedPolicy.askAiMaxTokens);
    if (!Number.isFinite(askTok) || askTok < 512) askTok = DEFAULT_POLICY.askAiMaxTokens;
    if (mergedPolicy.agentMaxMode) askTok = Math.min(16384, Math.max(askTok, 8192));
    if (mergedPolicy.agentDeepThinking) askTok = Math.min(16384, askTok + 4096);
    mergedPolicy.askAiMaxTokens = askTok;

    let refTok = Number(mergedPolicy.reflectionMaxTokens);
    if (!Number.isFinite(refTok) || refTok < 256) refTok = DEFAULT_POLICY.reflectionMaxTokens;
    if (mergedPolicy.agentDeepThinking) refTok = Math.min(4096, refTok + 1024);
    mergedPolicy.reflectionMaxTokens = refTok;

    let ctxLim = Number(mergedPolicy.agentContextLimitChars);
    if (!Number.isFinite(ctxLim)) ctxLim = DEFAULT_POLICY.agentContextLimitChars;
    mergedPolicy.agentContextLimitChars = Math.min(200000, Math.max(2000, Math.floor(ctxLim)));

    const task = {
      id: taskId,
      goal: normalizedGoal,
      policy: mergedPolicy,
      status: 'planning',
      /** E04 Omega SDLC phase for structured logs: plan → execute → verify → ship */
      omegaPhase: 'plan',
      steps: [],
      plan: null,
      createdAt: new Date(),
      updatedAt: new Date(),
      log: [],
      /** @type {((ok: boolean) => void) | null} */
      _approveResolve: null,
      cancelled: false,
      /** When true, all disk mutations in the plan were pre-approved in one batch (AgenticPlanningOrchestrator). */
      mutationBatchApproved: false,
      /** Snapshots for rollback (newest last). */
      rollbackStack: [],
      /** Nesting depth for swarm sub-tasks (root = 0). */
      executionDepth: 0
    };

    this.activeTasks.set(taskId, task);
    this.taskHistory.push(task);

    this.executeTask(taskId).catch((err) => {
      // eslint-disable-next-line no-console
      console.error('Task execution error:', err);
      const t = this.activeTasks.get(taskId);
      if (t) {
        t.status = 'failed';
        t.error = err.message;
        t.updatedAt = new Date();
        this.appendLog(t, `fatal: ${err.message}`);
      }
    });

    return taskId;
  }

  /**
   * Register a nested swarm task (shared policy, deeper executionDepth). Caller must await `executeTask(childId)`.
   */
  forkChildTask(goal, parentTask) {
    const normalizedGoal = typeof goal === 'string' ? goal.trim() : '';
    if (!normalizedGoal) {
      throw new Error('Sub-agent goal is required');
    }
    const parentDepth = parentTask.executionDepth || 0;
    const nextDepth = parentDepth + 1;
    if (nextDepth > parentTask.policy.maxSubtaskDepth) {
      throw new Error(
        `Sub-agent depth ${nextDepth} exceeds maxSubtaskDepth (${parentTask.policy.maxSubtaskDepth})`
      );
    }
    const taskId = this.generateTaskId();
    const task = {
      id: taskId,
      goal: normalizedGoal,
      policy: { ...parentTask.policy },
      status: 'planning',
      omegaPhase: 'plan',
      steps: [],
      plan: null,
      createdAt: new Date(),
      updatedAt: new Date(),
      log: [],
      _approveResolve: null,
      cancelled: false,
      mutationBatchApproved: false,
      rollbackStack: [],
      executionDepth: nextDepth
    };
    this.activeTasks.set(taskId, task);
    this.taskHistory.push(task);
    return taskId;
  }

  filterDisallowedPlanSteps(task, planSteps) {
    return planSteps.filter((raw) => {
      const t = typeof raw.type === 'string' ? raw.type.trim().toLowerCase() : '';
      const nt = t === 'launch_agents' ? 'task' : t;
      if ((nt === 'run_terminal' || nt === 'run_shell') && !task.policy.allowShell) {
        this.appendLog(
          task,
          '[plan-filter] removed run_terminal — enable “Agent terminal” in Settings › Copilot'
        );
        return false;
      }
      if (nt === 'task' && !task.policy.swarmingEnabled) {
        this.appendLog(
          task,
          '[plan-filter] removed task/swarm step — enable “Swarming” in Settings › Copilot'
        );
        return false;
      }
      return true;
    });
  }

  appendLog(task, line) {
    const entry = `[${new Date().toISOString()}] ${line}`;
    if (!task.log) task.log = [];
    task.log.push(entry);
    if (task.log.length > 400) task.log.splice(0, task.log.length - 400);
  }

  /**
   * User approves or denies the current pending mutating step.
   */
  respondApproval(taskId, approved) {
    const task = this.activeTasks.get(taskId);
    if (!task || typeof task._approveResolve !== 'function') {
      return { ok: false, error: 'No pending approval for this task' };
    }
    const resolve = task._approveResolve;
    task._approveResolve = null;
    task.status = 'executing';
    task.updatedAt = new Date();
    resolve(Boolean(approved));
    return { ok: true };
  }

  cancelTask(taskId) {
    const task = this.activeTasks.get(taskId);
    if (!task) return { ok: false, error: 'Unknown task id — it may have finished or never started.' };
    task.cancelled = true;
    if (typeof task._approveResolve === 'function') {
      task._approveResolve(false);
      task._approveResolve = null;
    }
    task.status = 'cancelled';
    task.updatedAt = new Date();
    this.appendLog(task, 'cancelled by user');
    return { ok: true };
  }

  /**
   * Poll until the task reaches a terminal status or optional cancel predicate fires.
   * @param {string} taskId
   * @param {{ isCancelled?: () => boolean, pollMs?: number, timeoutMs?: number }} [opts]
   */
  waitForTaskCompletion(taskId, opts = {}) {
    const pollMs = typeof opts.pollMs === 'number' && opts.pollMs >= 50 ? opts.pollMs : 250;
    const timeoutMs =
      typeof opts.timeoutMs === 'number' && opts.timeoutMs >= 1000 ? opts.timeoutMs : 3_600_000;
    const isCancelled = typeof opts.isCancelled === 'function' ? opts.isCancelled : null;
    const terminal = new Set([
      'completed',
      'completed_with_errors',
      'completed_with_skips',
      'failed',
      'cancelled'
    ]);
    const started = Date.now();
    return new Promise((resolve, reject) => {
      const tick = () => {
        if (isCancelled && isCancelled()) {
          this.cancelTask(taskId);
          resolve({
            taskId,
            status: 'cancelled',
            reason: 'parent_cancelled'
          });
          return;
        }
        if (Date.now() - started > timeoutMs) {
          reject(new Error(`Sub-agent task ${taskId} timed out after ${timeoutMs}ms`));
          return;
        }
        const t = this.activeTasks.get(taskId);
        if (!t) {
          resolve({ taskId, status: 'missing' });
          return;
        }
        if (terminal.has(t.status)) {
          resolve({ taskId, status: t.status, error: t.error || null });
          return;
        }
        setTimeout(tick, pollMs);
      };
      tick();
    });
  }

  /**
   * @param {object} task
   * @param {object} fields — include `kind` for UI (`step` | `plan_review` | `mutation_batch`)
   * @param {string} [logLine]
   */
  waitForUserDecision(task, fields, logLine) {
    return new Promise((resolve) => {
      task._approveResolve = resolve;
      task.status = 'awaiting_approval';
      task.pendingApproval = {
        ...fields,
        requestedAt: new Date().toISOString()
      };
      task.updatedAt = new Date();
      this.appendLog(task, logLine || `approval required: ${fields.stepType || fields.kind || '?'}`);
    });
  }

  waitForApproval(task, stepRecord, summary) {
    return this.waitForUserDecision(
      task,
      {
        kind: 'step',
        stepType: stepRecord.type,
        path: stepRecord.path,
        description: stepRecord.description,
        summary
      },
      `approval required: ${stepRecord.type} ${stepRecord.path || ''}`
    );
  }

  /**
   * @param {object} step
   * @param {object} policy
   * @param {object} [task] — honors `mutationBatchApproved` for mutate tier
   */
  stepNeedsApproval(step, policy, task = null) {
    const tier = stepRiskTier(step.type);
    if (tier === 'mutate' && task && task.mutationBatchApproved) {
      return false;
    }
    if (tier === 'exec' && task && task.mutationBatchApproved) {
      return false;
    }
    if (tier === 'read') return !policy.autoApproveReads;
    if (tier === 'mutate') return !policy.autoApproveWrites;
    if (tier === 'exec') return !policy.autoApproveTerminal;
    const st = (step.type || '').toLowerCase();
    if (tier === 'cognitive' && st === 'ask_ai' && policy.requireAskAiApproval) {
      return true;
    }
    if (tier === 'cognitive' && (st === 'task' || st === 'launch_agents') && policy.requireSubagentApproval) {
      return true;
    }
    return false;
  }

  /** Hook for plan-level gates (extended by AgenticPlanningOrchestrator). */
  async afterPlanReady(task, planSteps, activeProvider) {
    void task;
    void planSteps;
    void activeProvider;
  }

  /** Prefer planner-supplied risk; fall back to step type. */
  normalizeRiskLevel(modelRisk, stepType) {
    const r = (modelRisk || '').toLowerCase();
    if (r === 'low' || r === 'medium' || r === 'high') {
      return r;
    }
    return stepRiskLevelFromType(stepType);
  }

  async executeTask(taskId) {
    const task = this.activeTasks.get(taskId);
    if (!task) return;

    try {
      if (task.cancelled) return;

      task.status = 'planning';
      task.omegaPhase = 'plan';
      task.updatedAt = new Date();
      this.appendLog(task, '[omega:plan] planning with workspace context…');

      const activeProvider = this.aiManager.getActiveProviderId
        ? this.aiManager.getActiveProviderId()
        : 'bigdaddyg';

      let workspaceSummary = '';
      if (typeof this.runtime.summarizeWorkspace === 'function') {
        try {
          workspaceSummary = await this.runtime.summarizeWorkspace();
        } catch (e) {
          workspaceSummary = `(workspace scan failed: ${e.message})`;
        }
      }

      const ctxLimit = task.policy.agentContextLimitChars || 48000;
      if (workspaceSummary.length > ctxLimit) {
        this.appendLog(
          task,
          `[context] workspace summary truncated to ${ctxLimit} chars (Settings › Copilot › Context limit)`
        );
        workspaceSummary = `${workspaceSummary.slice(0, ctxLimit)}\n… [truncated — increase context limit in Settings]`;
      }

      const planPrompt = this.buildPlanPrompt(
        task.goal,
        this.runtime?.getProjectRoot?.(),
        workspaceSummary,
        task.policy
      );
      const response = await this.aiManager.invoke(activeProvider, planPrompt, {
        maxTokens: task.policy.maxPlanTokens,
        temperature: 0.15
      });
      const parsedPlan = parsePlanResponse(response.content);
      if (parsedPlan.parseIssue) {
        this.appendLog(
          task,
          `[plan-parse] ${parsedPlan.parseIssue} — expect a single JSON object from buildPlanPrompt (steps array).`
        );
      }
      let planSteps = this.filterDisallowedPlanSteps(task, parsedPlan.steps || []);
      const plan = { steps: planSteps };
      if (planSteps.length > task.policy.maxSteps) {
        this.appendLog(task, `truncating plan from ${planSteps.length} to ${task.policy.maxSteps} steps`);
        planSteps = planSteps.slice(0, task.policy.maxSteps);
        plan.steps = planSteps;
      }
      task.plan = plan;
      task.steps.push({
        type: 'plan',
        content: JSON.stringify(plan, null, 2),
        status: 'completed',
        risk: 'cognitive'
      });

      await this.afterPlanReady(task, planSteps, activeProvider);
      task.pendingApproval = null;
      if (task.cancelled) {
        task.updatedAt = new Date();
        return;
      }

      task.status = 'executing';
      task.omegaPhase = 'execute';
      task.updatedAt = new Date();
      this.appendLog(task, `[omega:execute] running ${planSteps.length} steps`);

      for (const rawStep of planSteps) {
        if (task.cancelled) {
          this.appendLog(task, 'aborted (cancelled)');
          break;
        }

        const step = this.normalizeStep(rawStep);
        const stepRecord = {
          type: step.type,
          description: step.description || step.prompt || '',
          path: step.path,
          status: 'running',
          risk: stepRiskTier(step.type),
          riskLevel: this.normalizeRiskLevel(step.risk, step.type),
          startedAt: new Date()
        };
        task.steps.push(stepRecord);
        task.updatedAt = new Date();

        try {
          if (this.stepNeedsApproval(step, task.policy, task)) {
            stepRecord.status = 'pending_approval';
            const summary = this.buildApprovalSummary(step);
            const ok = await this.waitForApproval(task, stepRecord, summary);
            task.pendingApproval = null;
            if (!ok) {
              stepRecord.status = 'skipped';
              stepRecord.error = 'User denied approval';
              stepRecord.completedAt = new Date();
              this.appendLog(task, `skipped after denial: ${step.type}`);
              continue;
            }
            stepRecord.status = 'running';
          }

          this._currentTaskForStep = task;
          let result;
          try {
            result = await this.executeStep(step, activeProvider);
          } finally {
            this._currentTaskForStep = null;
          }
          stepRecord.status = 'completed';
          stepRecord.result = result;
          stepRecord.completedAt = new Date();
          this.appendLog(task, `ok: ${step.type} ${step.path || ''}`);
        } catch (err) {
          stepRecord.status = 'failed';
          stepRecord.error = err.message;
          stepRecord.completedAt = new Date();
          this.appendLog(task, `fail: ${step.type} — ${err.message}`);
          // E05: real mutate failure stops the plan (rollback stack already captured per step in planning orchestrator)
          if (stepRiskTier(step.type) === 'mutate') {
            task.omegaPhase = 'failed';
            this.appendLog(task, '[omega:failed] halted plan after disk mutation failure');
            break;
          }
        }
      }

      if (task.cancelled) {
        task.status = 'cancelled';
      } else if (task.policy.enableReflection && !task.cancelled) {
        task.omegaPhase = 'verify';
        this.appendLog(task, '[omega:verify] reflection pass');
        await this.runReflection(task, activeProvider);
        if (task.policy.agentDeepThinking && task.policy.enableReflection && !task.cancelled) {
          this.appendLog(task, '[omega:verify] deep reflection pass');
          await this.runReflectionDeep(task, activeProvider);
        }
      }

      const failed = task.steps.some((s) => s.status === 'failed');
      const skipped = task.steps.some((s) => s.status === 'skipped');
      if (task.status !== 'cancelled') {
        if (failed) task.status = 'completed_with_errors';
        else if (skipped) task.status = 'completed_with_skips';
        else task.status = 'completed';
        if (task.omegaPhase !== 'failed') {
          task.omegaPhase = 'ship';
          this.appendLog(task, `[omega:ship] task ${task.status}`);
        }
      }
    } catch (error) {
      task.status = 'failed';
      task.error = error.message;
      this.appendLog(task, `plan/execute error: ${error.message}`);
    }

    task.updatedAt = new Date();
    task.pendingApproval = null;
  }

  buildApprovalSummary(step) {
    const lines = [`Action: ${step.type}`, `Path: ${step.path || '(n/a)'}`];
    const st = (step.type || '').toLowerCase();
    if (st === 'ask_ai') {
      const pr = step.prompt || step.description || '';
      lines.push(`Prompt preview (${pr.length} chars):`, pr.slice(0, 1200) + (pr.length > 1200 ? '…' : ''));
      return lines.join('\n');
    }
    if (st === 'run_terminal') {
      const c = step.command || step.prompt || step.description || '';
      lines.push(`Shell command (${String(c).length} chars):`, String(c).slice(0, 1200) + (String(c).length > 1200 ? '…' : ''));
      return lines.join('\n');
    }
    if (st === 'task' || st === 'launch_agents') {
      const mode = step.taskMode || step.mode || 'sequential';
      const agents = Array.isArray(step.agents) ? step.agents : [];
      lines.push(`Mode: ${mode}`, `Sub-agents: ${agents.length}`);
      agents.slice(0, 12).forEach((a, i) => {
        const g = typeof a === 'string' ? a : a && a.goal;
        lines.push(`  ${i + 1}. ${String(g || '').slice(0, 200)}`);
      });
      if (agents.length > 12) lines.push(`  … +${agents.length - 12} more`);
      return lines.join('\n');
    }
    if (step.content != null) {
      const c = String(step.content);
      lines.push(`Content preview (${c.length} chars):`, c.slice(0, 800) + (c.length > 800 ? '…' : ''));
    } else if (st === 'write_file' || st === 'append_file') {
      lines.push('Content will be generated by the model if not provided in the plan.');
    }
    return lines.join('\n');
  }
  async runReflection(task, providerId) {
    const stepRecord = {
      type: 'reflect',
      description: 'Verify goal satisfaction',
      status: 'running',
      risk: 'cognitive',
      startedAt: new Date()
    };
    task.steps.push(stepRecord);
    task.updatedAt = new Date();

    try {
      const recap = task.steps
        .filter((s) => s.result || s.error)
        .slice(-12)
        .map((s) => `- ${s.type} ${s.status} ${s.error || ''}`)
        .join('\n');
      const prompt = `You are a senior principal engineer reviewing an autonomous IDE run.

Original goal:
${task.goal}

Step outcomes (recent):
${recap}

Reply in 2–4 sentences: was the goal likely satisfied, what is still missing, and the single highest-leverage next action if any.`;
      const refTok = Math.min(4096, Number(task.policy.reflectionMaxTokens) || 1024);
      const result = await this.aiManager.invoke(providerId, prompt, {
        maxTokens: refTok,
        temperature: 0.2
      });
      stepRecord.status = 'completed';
      stepRecord.result = result.content;
      stepRecord.completedAt = new Date();
      this.appendLog(task, 'reflection complete');
    } catch (e) {
      stepRecord.status = 'failed';
      stepRecord.error = e.message;
      stepRecord.completedAt = new Date();
    }
  }

  /** Second critique pass when `agentDeepThinking` is enabled (real extra LLM call). */
  async runReflectionDeep(task, providerId) {
    const stepRecord = {
      type: 'reflect_deep',
      description: 'Deep critique — risks and gaps',
      status: 'running',
      risk: 'cognitive',
      startedAt: new Date()
    };
    task.steps.push(stepRecord);
    task.updatedAt = new Date();
    try {
      const recap = task.steps
        .filter((s) => s.type === 'reflect' && s.result)
        .map((s) => String(s.result).slice(0, 600))
        .join('\n---\n');
      const prompt = `You are auditing an autonomous IDE run for missed risks and edge cases.

Original goal:
${task.goal}

Prior reflection (if any):
${recap || '(none)'}

Reply in 2–3 sentences: what was overlooked, and the single best follow-up action.`;
      const maxTok = Math.min(4096, (Number(task.policy.reflectionMaxTokens) || 1024) + 512);
      const result = await this.aiManager.invoke(providerId, prompt, {
        maxTokens: maxTok,
        temperature: 0.25
      });
      stepRecord.status = 'completed';
      stepRecord.result = result.content;
      stepRecord.completedAt = new Date();
      this.appendLog(task, 'deep reflection complete');
    } catch (e) {
      stepRecord.status = 'failed';
      stepRecord.error = e.message;
      stepRecord.completedAt = new Date();
    }
  }

  normalizeStep(step) {
    if (!step || typeof step !== 'object') {
      return { type: 'ask_ai', description: String(step || ''), prompt: String(step || '') };
    }
    let rawType = typeof step.type === 'string' ? step.type.trim().toLowerCase() : 'ask_ai';
    if (rawType === 'launch_agents') rawType = 'task';
    if (rawType === 'run_shell') rawType = 'run_terminal';
    const base = {
      type: rawType,
      description: step.description || '',
      path: step.path,
      prompt: step.prompt || step.description,
      content: step.content,
      maxHits: typeof step.maxHits === 'number' && Number.isFinite(step.maxHits) ? step.maxHits : undefined,
      risk: typeof step.risk === 'string' ? step.risk : undefined,
      command: typeof step.command === 'string' ? step.command : undefined,
      timeoutMs: typeof step.timeoutMs === 'number' && Number.isFinite(step.timeoutMs) ? step.timeoutMs : undefined
    };
    if (base.type === 'run_terminal') {
      const cmd =
        (base.command && String(base.command).trim()) ||
        String(base.prompt || base.description || '').trim();
      base.command = cmd;
      base.path = cmd ? cmd.slice(0, 160) : undefined;
    }
    if (base.type === 'task') {
      let agents = step.agents;
      if (!agents && Array.isArray(step.goals)) {
        agents = step.goals.map((g) => (typeof g === 'string' ? { goal: g } : g));
      }
      base.agents = Array.isArray(agents) ? agents : [];
      const m = (step.mode || step.taskMode || 'sequential').toString().toLowerCase();
      base.taskMode = m === 'parallel' ? 'parallel' : 'sequential';
    }
    return base;
  }

  async executeStep(step, providerId) {
    if (step.type === 'read_file') {
      this.requireRuntime('readFile');
      if (!step.path) {
        throw new Error('read_file step requires "path"');
      }
      assertSafeRelativePath(step.path, 'read_file');
      const content = await this.runtime.readFile(step.path);
      return { path: step.path, bytes: content.length, preview: String(content).slice(0, 500) };
    }

    if (step.type === 'search_repo') {
      this.requireRuntime('searchWorkspace');
      const query = (step.prompt || step.description || '').trim();
      if (query.length < 2) {
        throw new Error('search_repo requires prompt or description (search string, min 2 chars)');
      }
      const maxHits = Math.min(Math.max(1, step.maxHits || 40), 80);
      return await this.runtime.searchWorkspace(query, { maxHits });
    }

    if (step.type === 'write_file') {
      this.requireRuntime('writeFile');
      if (!step.path) {
        throw new Error('write_file step requires "path"');
      }
      assertSafeRelativePath(step.path, 'write_file');
      const content =
        typeof step.content === 'string' ? step.content : await this.generateFileContent(step, providerId);
      await this.runtime.writeFile(step.path, content);
      return { path: step.path, bytes: content.length };
    }

    if (step.type === 'append_file') {
      this.requireRuntime('appendFile');
      if (!step.path) {
        throw new Error('append_file step requires "path"');
      }
      assertSafeRelativePath(step.path, 'append_file');
      const content =
        typeof step.content === 'string' ? step.content : await this.generateFileContent(step, providerId);
      await this.runtime.appendFile(step.path, content);
      return { path: step.path, bytes: content.length };
    }

    if (step.type === 'list_dir') {
      this.requireRuntime('readDir');
      const dirPath = step.path || '.';
      assertSafeRelativePath(dirPath, 'list_dir');
      const entries = await this.runtime.readDir(dirPath);
      return { path: dirPath, entries };
    }

    if (step.type === 'run_terminal') {
      const parent = this._currentTaskForStep;
      if (!parent || !parent.policy.allowShell) {
        throw new Error('run_terminal is disabled — enable Agent terminal in Settings › Copilot');
      }
      this.requireRuntime('runShellCommand');
      const cmd = String(step.command || step.prompt || '').trim();
      if (!cmd) {
        throw new Error('run_terminal requires non-empty "command"');
      }
      const maxOut = Number(parent.policy.shellMaxOutputChars) || 65536;
      const res = await this.runtime.runShellCommand({
        command: cmd,
        timeoutMs: step.timeoutMs,
        maxOutputChars: maxOut
      });
      if (res.exitCode !== 0) {
        const tail = `${res.stderr || ''}\n${res.stdout || ''}`.trim().slice(0, 800);
        throw new Error(`run_terminal exit ${res.exitCode}${tail ? `: ${tail}` : ''}`);
      }
      return res;
    }

    if (step.type === 'task') {
      const parent = this._currentTaskForStep;
      if (!parent || !parent.policy.swarmingEnabled) {
        throw new Error('task/swarm steps are disabled — enable Swarming in Settings › Copilot');
      }
      const agents = step.agents || [];
      if (agents.length === 0) {
        throw new Error('task step requires agents or goals array');
      }
      if (agents.length > parent.policy.maxSubAgentsPerStep) {
        throw new Error(`task step: at most ${parent.policy.maxSubAgentsPerStep} sub-agents`);
      }
      const mode = step.taskMode === 'parallel' ? 'parallel' : 'sequential';
      const runOne = async (a) => {
        const g = typeof a === 'string' ? a : a && a.goal;
        const goalStr = String(g || '').trim();
        if (!goalStr) {
          throw new Error('sub-agent empty goal');
        }
        const childId = this.forkChildTask(goalStr, parent);
        await this.executeTask(childId);
        const child = this.activeTasks.get(childId);
        return {
          taskId: childId,
          status: child ? child.status : 'missing',
          error: child && child.error ? child.error : null
        };
      };
      let subResults;
      if (mode === 'parallel') {
        subResults = await Promise.all(agents.map((a) => runOne(a)));
      } else {
        subResults = [];
        for (const a of agents) {
          subResults.push(await runOne(a));
        }
      }
      return { mode, subTasks: subResults };
    }

    const askCap = this._currentTaskForStep?.policy?.askAiMaxTokens || 4096;
    const result = await this.aiManager.invoke(
      providerId,
      step.prompt || step.description || 'Continue task execution.',
      { maxTokens: askCap, temperature: 0.25 }
    );
    return result.content;
  }

  async generateFileContent(step, providerId) {
    const prompt = `Generate file content for path "${step.path}".
Task description: ${step.description || ''}
Return file content only, no markdown fences.`;
    const cap = this._currentTaskForStep?.policy?.askAiMaxTokens || 8192;
    const result = await this.aiManager.invoke(providerId, prompt, { maxTokens: cap, temperature: 0.2 });
    return (result.content || '').trim();
  }

  requireRuntime(methodName) {
    if (typeof this.runtime[methodName] !== 'function') {
      throw new Error(
        `Runtime missing required method: ${methodName} — wire ${methodName} on the orchestrator runtime bridge in main.`
      );
    }
  }

  buildPlanPrompt(goal, projectRoot, workspaceSummary, policy) {
    const pol = policy || DEFAULT_POLICY;
    const shellLine = pol.allowShell
      ? `- run_terminal: single-line shell command in "command" (cwd = project root). Optional "timeoutMs" (5000–600000). No newlines or null bytes in "command". Use only when necessary (build, test, format).`
      : '- run_terminal is **disabled** for this run — do not emit run_terminal or run_shell steps.';
    const swarmLine = pol.swarmingEnabled
      ? `- task: sub-goals as nested agent runs. Use "taskMode": "parallel" | "sequential", and "agents": [ { "goal": "..." } ] or "goals": [ "..." ]. Max ${pol.maxSubAgentsPerStep} agents per step; nesting depth max ${pol.maxSubtaskDepth}.`
      : '- task / swarm is **disabled** — do not emit task or launch_agents steps.';
    const typeList = [
      'ask_ai',
      'read_file',
      'write_file',
      'append_file',
      'list_dir',
      'search_repo',
      ...(pol.allowShell ? ['run_terminal'] : []),
      ...(pol.swarmingEnabled ? ['task'] : [])
    ].join(', ');
    const modeHints = [];
    if (pol.agentMaxMode) modeHints.push('Planner hint: user enabled **max mode** — prefer fewer, higher-impact steps.');
    if (pol.agentDeepThinking) {
      modeHints.push('Planner hint: user enabled **deep thinking** — explore with read/search before writes.');
    }
    const modeBlock = modeHints.length ? `${modeHints.join('\n')}\n` : '';
    return `You are the planning brain of an autonomous IDE agent (Cursor-style). Produce a JSON plan ONLY.

User goal:
${goal}

Project root: ${projectRoot || '(not opened — prefer ask_ai-only steps)'}
Workspace index (partial, respect paths):
${workspaceSummary || '(unavailable)'}

${modeBlock}Rules:
- Use relative paths from project root. Never use .. or absolute paths outside the project.
- Prefer search_repo or list_dir / read_file before write_file when exploring.
- write_file may omit "content" if the editor should generate body later.
- step types allowed for this run: ${typeList}
- optional per-step "risk": "low" | "medium" | "high" (disk writes and shell should be high)
- search_repo: use "prompt" or "description" for the literal substring to find; optional "maxHits" (default 40).
${shellLine}
${swarmLine}
- Do not invent network/package steps that are not expressible as the allowed step types above.

Return ONLY a JSON object with a "steps" array. Each step object must include "type" (one of the allowed types for this run) and fields required for that type (e.g. run_terminal needs "command"; task needs "agents" or "goals" and optional "taskMode").
Example shape:
{
  "steps": [
    {
      "type": "read_file",
      "description": "short description",
      "path": "relative/path",
      "risk": "low"
    }
  ]
}`;
  }

  /**
   * Lightweight snapshots for UI task picker (active + recent history).
   */
  listTasksSnapshot() {
    const slim = (t) => ({
      id: t.id,
      goal: t.goal,
      status: t.status,
      omegaPhase: t.omegaPhase,
      updatedAt: t.updatedAt instanceof Date ? t.updatedAt.toISOString() : t.updatedAt,
      stepCount: Array.isArray(t.steps) ? t.steps.length : 0,
      hasPendingApproval: Boolean(t.pendingApproval),
      rollbackCount: Array.isArray(t.rollbackStack) ? t.rollbackStack.length : 0
    });
    const active = Array.from(this.activeTasks.values()).map(slim);
    const history = this.taskHistory
      .slice(-30)
      .reverse()
      .map(slim)
      .filter((x, i, arr) => arr.findIndex((y) => y.id === x.id) === i);
    return { active, history };
  }

  getTaskStatus(taskId) {
    return this.activeTasks.get(taskId) || null;
  }

  generateTaskId() {
    return `task_${Date.now()}_${Math.random().toString(36).slice(2, 11)}`;
  }
}

module.exports = AgentOrchestrator;
module.exports.stepRiskTier = stepRiskTier;
module.exports.stepRiskLevelFromType = stepRiskLevelFromType;
