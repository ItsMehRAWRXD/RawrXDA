const OrchestratorModule = require('./orchestrator');
const AgentOrchestrator = OrchestratorModule;
const { stepRiskTier } = OrchestratorModule;

/**
 * Unified planning path: risk-tagged steps, optional whole-plan review,
 * optional single gate for all disk mutations, rollback snapshots before writes/appends.
 *
 * M01 — Adds plan/mutation batch gates on top of `AgentOrchestrator`. Does not: introduce new step types beyond the base orchestrator.
 * M06 — Rollback restores captured snapshots only; does not recover files deleted outside the agent runtime.
 */
const PLANNING_POLICY_DEFAULTS = {
  /** Pause after JSON plan for Approve / Deny before any step runs */
  requirePlanApproval: false,
  /** One approval for every write_file / append_file in the plan (when not auto-approving writes) */
  batchApproveMutations: false,
  /** Snapshot files before mutate steps (rollback in UI) */
  enableRollbackSnapshots: true
};

/**
 * Planning orchestrator with optional plan approval, mutation batch approval, and rollback stack.
 * Does not: change JSON plan grammar — still `{ "steps": [...] }` as parsed by `parsePlanResponse` in `orchestrator.js`.
 */
class AgenticPlanningOrchestrator extends AgentOrchestrator {
  startTask(goal, policy = {}) {
    const merged = { ...PLANNING_POLICY_DEFAULTS, ...policy };
    return super.startTask(goal, merged);
  }

  async afterPlanReady(task, planSteps, activeProvider) {
    void activeProvider;
    this.attachPlanRiskDigest(task, planSteps);

    if (task.policy.requirePlanApproval) {
      const ok = await this.waitForUserDecision(
        task,
        {
          kind: 'plan_review',
          stepType: 'plan_review',
          summary: this.buildPlanReviewSummary(task, planSteps),
          planStepsDigest: this.digestPlanSteps(planSteps)
        },
        'plan review: approve multi-step run'
      );
      task.pendingApproval = null;
      if (!ok) {
        task.cancelled = true;
        task.status = 'cancelled';
        this.appendLog(task, 'user rejected plan');
        return;
      }
    }

    if (task.policy.batchApproveMutations) {
      const normalized = planSteps.map((raw) => this.normalizeStep(raw));
      const mut = normalized.filter((s) => {
        const tier = stepRiskTier(s.type);
        if (tier === 'mutate') return !task.policy.autoApproveWrites;
        if (tier === 'exec') return !task.policy.autoApproveTerminal;
        return false;
      });
      if (mut.length > 0) {
        const ok = await this.waitForUserDecision(
          task,
          {
            kind: 'mutation_batch',
            stepType: 'mutation_batch',
            summary: this.buildMutationBatchSummary(mut),
            mutationBatch: mut.map((s) => ({
              type: s.type,
              path: s.path,
              description: (s.description || s.prompt || '').slice(0, 400),
              riskLevel: this.normalizeRiskLevel(s.risk, s.type)
            }))
          },
          `mutation batch: ${mut.length} write/append step(s) queued`
        );
        task.pendingApproval = null;
        if (!ok) {
          task.cancelled = true;
          task.status = 'cancelled';
          this.appendLog(task, 'user rejected mutation batch');
          return;
        }
        task.mutationBatchApproved = true;
      }
    }
  }

  attachPlanRiskDigest(task, planSteps) {
    if (!task.plan || !Array.isArray(task.plan.steps)) return;
    task.plan.riskDigest = planSteps.map((raw, i) => {
      const s = this.normalizeStep(raw);
      return {
        index: i,
        type: s.type,
        path: s.path,
        tier: stepRiskTier(s.type),
        level: this.normalizeRiskLevel(s.risk, s.type)
      };
    });
  }

  digestPlanSteps(planSteps) {
    return planSteps.map((raw) => {
      const s = this.normalizeStep(raw);
      return {
        type: s.type,
        path: s.path,
        description: (s.description || s.prompt || '').slice(0, 500),
        tier: stepRiskTier(s.type),
        riskLevel: this.normalizeRiskLevel(s.risk, s.type)
      };
    });
  }

  buildPlanReviewSummary(task, planSteps) {
    const lines = [
      `Goal: ${task.goal}`,
      `Steps: ${planSteps.length}`,
      '---',
      ...this.digestPlanSteps(planSteps).map(
        (d, i) =>
          `${i + 1}. [${d.riskLevel} / ${d.tier}] ${d.type} ${d.path || ''} — ${d.description.slice(0, 100)}`
      )
    ];
    return lines.join('\n');
  }

  buildMutationBatchSummary(mutSteps) {
    const lines = ['Queued disk mutations (project-relative):', '---'];
    mutSteps.forEach((s, i) => {
      lines.push(`${i + 1}. ${s.type} → ${s.path || '(missing path)'}`);
    });
    return lines.join('\n');
  }

  async executeStep(step, providerId) {
    const task = this._currentTaskForStep;
    if (
      task &&
      task.policy.enableRollbackSnapshots !== false &&
      (step.type === 'write_file' || step.type === 'append_file')
    ) {
      await this.captureRollbackSnapshot(task, step.path, step.type);
    }
    return super.executeStep(step, providerId);
  }

  async captureRollbackSnapshot(task, relPath, op) {
    if (!relPath || !task.rollbackStack) return;
    let previousContent = null;
    let existed = false;
    try {
      previousContent = await this.runtime.readFile(relPath);
      existed = true;
    } catch {
      existed = false;
      previousContent = null;
    }
    task.rollbackStack.push({
      path: relPath,
      previousContent,
      existed,
      op
    });
  }

  /**
   * Restore files from newest snapshot backwards (LIFO).
   * @param {string} taskId
   * Does not: undo in-memory editor buffers — disk paths only.
   */
  async rollbackTaskMutations(taskId) {
    const task = this.activeTasks.get(taskId);
    if (!task?.rollbackStack?.length) {
      return {
        ok: false,
        error: 'No rollback snapshots for this task — enable rollback snapshots and run mutate steps first.'
      };
    }
    const stack = [...task.rollbackStack];
    task.rollbackStack = [];
    let restored = 0;
    for (let i = stack.length - 1; i >= 0; i -= 1) {
      const e = stack[i];
      const { path: relPath, previousContent, existed } = e;
      try {
        if (!existed) {
          if (typeof this.runtime.deleteFile !== 'function') {
            throw new Error('Runtime missing deleteFile');
          }
          await this.runtime.deleteFile(relPath);
        } else {
          await this.runtime.writeFile(relPath, previousContent);
        }
        restored += 1;
      } catch (err) {
        task.rollbackStack = stack.slice(0, i + 1).concat(task.rollbackStack);
        return {
          ok: false,
          error: `${err.message} — partial rollback: ${restored} file(s) restored; see main log for path details.`,
          restored
        };
      }
    }
    this.appendLog(task, `rollback: restored ${restored} snapshot(s)`);
    return { ok: true, restored };
  }
}

module.exports = AgenticPlanningOrchestrator;
