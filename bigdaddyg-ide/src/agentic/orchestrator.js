function parsePlanResponse(content) {
  const fallback = {
    steps: [{ type: 'ask_ai', description: 'Manual review required', status: 'pending' }]
  };

  if (!content || typeof content !== 'string') {
    return fallback;
  }

  const trimmed = content.trim();
  const jsonMatch = trimmed.match(/\{[\s\S]*\}/);
  if (!jsonMatch) {
    return fallback;
  }

  try {
    const parsed = JSON.parse(jsonMatch[0]);
    if (!Array.isArray(parsed.steps)) {
      return fallback;
    }
    return {
      steps: parsed.steps.map((s) => ({
        type: typeof s.type === 'string' ? s.type : 'ask_ai',
        description: typeof s.description === 'string' ? s.description : '',
        path: typeof s.path === 'string' ? s.path : undefined,
        prompt: typeof s.prompt === 'string' ? s.prompt : undefined,
        content: typeof s.content === 'string' ? s.content : undefined
      }))
    };
  } catch {
    return fallback;
  }
}

class AgentOrchestrator {
  constructor(aiManager, runtime = {}) {
    this.aiManager = aiManager;
    this.runtime = runtime;
    this.activeTasks = new Map();
    this.taskHistory = [];
  }

  async startTask(goal) {
    const normalizedGoal = typeof goal === 'string' ? goal.trim() : '';
    if (!normalizedGoal) {
      throw new Error('Goal is required');
    }

    const taskId = this.generateTaskId();
    const task = {
      id: taskId,
      goal: normalizedGoal,
      status: 'planning',
      steps: [],
      plan: null,
      createdAt: new Date(),
      updatedAt: new Date()
    };

    this.activeTasks.set(taskId, task);
    this.taskHistory.push(task);

    this.executeTask(taskId).catch((err) => {
      console.error('Task execution error:', err);
      const t = this.activeTasks.get(taskId);
      if (t) {
        t.status = 'failed';
        t.error = err.message;
        t.updatedAt = new Date();
      }
    });

    return taskId;
  }

  async executeTask(taskId) {
    const task = this.activeTasks.get(taskId);
    if (!task) return;

    try {
      task.status = 'planning';
      task.updatedAt = new Date();

      const activeProvider = this.aiManager.getActiveProviderId
        ? this.aiManager.getActiveProviderId()
        : 'bigdaddyg';
      const planPrompt = this.buildPlanPrompt(task.goal, this.runtime?.getProjectRoot?.());
      const response = await this.aiManager.invoke(activeProvider, planPrompt);
      const plan = parsePlanResponse(response.content);
      task.plan = plan;
      task.steps.push({ type: 'plan', content: JSON.stringify(plan, null, 2), status: 'completed' });

      task.status = 'executing';
      task.updatedAt = new Date();

      for (const rawStep of plan.steps || []) {
        const step = this.normalizeStep(rawStep);
        const stepRecord = {
          type: step.type,
          description: step.description || step.prompt || '',
          path: step.path,
          status: 'running',
          startedAt: new Date()
        };
        task.steps.push(stepRecord);
        task.updatedAt = new Date();

        try {
          const result = await this.executeStep(step, activeProvider);
          stepRecord.status = 'completed';
          stepRecord.result = result;
          stepRecord.completedAt = new Date();
        } catch (err) {
          stepRecord.status = 'failed';
          stepRecord.error = err.message;
          stepRecord.completedAt = new Date();
        }
      }

      task.status = task.steps.some((s) => s.status === 'failed') ? 'completed_with_errors' : 'completed';
    } catch (error) {
      task.status = 'failed';
      task.error = error.message;
    }

    task.updatedAt = new Date();
  }

  normalizeStep(step) {
    if (!step || typeof step !== 'object') {
      return { type: 'ask_ai', description: String(step || ''), prompt: String(step || '') };
    }
    return {
      type: typeof step.type === 'string' ? step.type.trim().toLowerCase() : 'ask_ai',
      description: step.description || '',
      path: step.path,
      prompt: step.prompt || step.description,
      content: step.content
    };
  }

  async executeStep(step, providerId) {
    if (step.type === 'read_file') {
      this.requireRuntime('readFile');
      if (!step.path) {
        throw new Error('read_file step requires "path"');
      }
      const content = await this.runtime.readFile(step.path);
      return { path: step.path, bytes: content.length, preview: String(content).slice(0, 500) };
    }

    if (step.type === 'write_file') {
      this.requireRuntime('writeFile');
      if (!step.path) {
        throw new Error('write_file step requires "path"');
      }
      const content = typeof step.content === 'string'
        ? step.content
        : await this.generateFileContent(step, providerId);
      await this.runtime.writeFile(step.path, content);
      return { path: step.path, bytes: content.length };
    }

    if (step.type === 'append_file') {
      this.requireRuntime('appendFile');
      if (!step.path) {
        throw new Error('append_file step requires "path"');
      }
      const content = typeof step.content === 'string'
        ? step.content
        : await this.generateFileContent(step, providerId);
      await this.runtime.appendFile(step.path, content);
      return { path: step.path, bytes: content.length };
    }

    if (step.type === 'list_dir') {
      this.requireRuntime('readDir');
      const dirPath = step.path || '.';
      const entries = await this.runtime.readDir(dirPath);
      return { path: dirPath, entries };
    }

    const result = await this.aiManager.invoke(
      providerId,
      step.prompt || step.description || 'Continue task execution.',
      {}
    );
    return result.content;
  }

  async generateFileContent(step, providerId) {
    const prompt = `Generate file content for path "${step.path}".
Task description: ${step.description || ''}
Return file content only, no markdown fences.`;
    const result = await this.aiManager.invoke(providerId, prompt, {});
    return (result.content || '').trim();
  }

  requireRuntime(methodName) {
    if (typeof this.runtime[methodName] !== 'function') {
      throw new Error(`Runtime missing required method: ${methodName}`);
    }
  }

  buildPlanPrompt(goal, projectRoot) {
    return `Create a deterministic JSON plan to complete this goal:
${goal}

Project root: ${projectRoot || '(not opened)'}

Return ONLY a JSON object with:
{
  "steps": [
    {
      "type": "ask_ai|read_file|write_file|append_file|list_dir",
      "description": "short description",
      "path": "relative/path/if needed",
      "prompt": "for ask_ai",
      "content": "optional file content"
    }
  ]
}`;
  }

  getTaskStatus(taskId) {
    return this.activeTasks.get(taskId) || null;
  }

  generateTaskId() {
    return `task_${Date.now()}_${Math.random().toString(36).slice(2, 11)}`;
  }
}

module.exports = AgentOrchestrator;
