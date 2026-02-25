function parsePlanResponse(content) {
  if (!content || typeof content !== 'string') {
    return { steps: [{ type: 'manual', description: 'Manual review required', status: 'pending' }] };
  }
  const trimmed = content.trim();
  const jsonMatch = trimmed.match(/\{[\s\S]*\}/);
  if (jsonMatch) {
    try {
      const parsed = JSON.parse(jsonMatch[0]);
      if (parsed.steps && Array.isArray(parsed.steps)) {
        return parsed;
      }
      return { steps: [{ type: 'step', description: trimmed.slice(0, 200), status: 'pending' }] };
    } catch {
      // fall through to fallback
    }
  }
  return {
    steps: [{ type: 'manual', description: 'Manual review required', status: 'pending' }]
  };
}

class AgentOrchestrator {
  constructor(aiManager) {
    this.aiManager = aiManager;
    this.activeTasks = new Map();
    this.taskHistory = [];
  }

  async startTask(goal) {
    const taskId = this.generateTaskId();
    const task = {
      id: taskId,
      goal,
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

      const planPrompt = `Create a detailed development plan for: ${task.goal}

Break this down into specific, actionable steps. Return a JSON object with a "steps" array. Each step: { "type": "step", "description": "what to do" }.
Example: { "steps": [ { "type": "step", "description": "Create file X" }, { "type": "step", "description": "Add tests" } ] }
Return ONLY the JSON object, no markdown or explanation.`;

      const response = await this.aiManager.invoke('bigdaddyg', planPrompt);
      const plan = parsePlanResponse(response.content);
      task.plan = plan;
      task.steps.push({ type: 'plan', content: JSON.stringify(plan, null, 2), status: 'completed' });

      task.status = 'executing';
      task.updatedAt = new Date();

      const steps = plan.steps || [];
      for (const step of steps) {
        const stepRecord = {
          type: step.type || 'step',
          description: step.description || String(step),
          status: 'running',
          startedAt: new Date()
        };
        task.steps.push(stepRecord);

        try {
          const result = await this.aiManager.invoke('bigdaddyg',
            `Execute this development step: ${stepRecord.description}`
          );
          stepRecord.status = 'completed';
          stepRecord.result = result.content;
          stepRecord.completedAt = new Date();
        } catch (err) {
          stepRecord.status = 'failed';
          stepRecord.error = err.message;
          stepRecord.completedAt = new Date();
        }
        task.updatedAt = new Date();
      }

      task.status = 'completed';
    } catch (error) {
      task.status = 'failed';
      task.error = error.message;
    }

    task.updatedAt = new Date();
  }

  getTaskStatus(taskId) {
    return this.activeTasks.get(taskId) || null;
  }

  generateTaskId() {
    return `task_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  }
}

module.exports = AgentOrchestrator;
