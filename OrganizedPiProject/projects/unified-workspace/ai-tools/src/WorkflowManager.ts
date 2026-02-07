import * as vscode from 'vscode';

export interface Todo {
  id: string;
  title: string;
  status: 'pending' | 'completed';
  assignedAgent?: string;
}

export interface WorkflowStep {
  id: string;
  name: string;
  type: 'code' | 'review' | 'test';
  todos: Todo[];
  completed: boolean;
}

export interface AgentWorkflow {
  agentId: string;
  steps: WorkflowStep[];
}

export class WorkflowManager {
  private workflows: Map<string, AgentWorkflow> = new Map();
  private todos: Map<string, Todo> = new Map();

  constructor(private context: vscode.ExtensionContext) {
    this.loadWorkflows();
  }

  createWorkflow(agentId: string): AgentWorkflow {
    const workflow: AgentWorkflow = { agentId, steps: [] };
    this.workflows.set(agentId, workflow);
    this.saveWorkflows();
    return workflow;
  }

  addTodo(agentId: string, stepId: string, title: string): void {
    const workflow = this.workflows.get(agentId);
    const step = workflow?.steps.find(s => s.id === stepId);
    if (step) {
      const todo: Todo = { id: Date.now().toString(), title, status: 'pending' };
      step.todos.push(todo);
      this.todos.set(todo.id, todo);
      this.saveWorkflows();
    }
  }

  completeTodo(todoId: string): void {
    const todo = this.todos.get(todoId);
    if (todo) {
      todo.status = 'completed';
      this.saveWorkflows();
    }
  }

  getWorkflow(agentId: string): AgentWorkflow | undefined {
    return this.workflows.get(agentId);
  }

  getPendingTodos(): Todo[] {
    return Array.from(this.todos.values()).filter(t => t.status === 'pending');
  }

  private loadWorkflows(): void {
    const stored = this.context.globalState.get<Record<string, AgentWorkflow>>('workflows', {});
    this.workflows = new Map(Object.entries(stored));
    this.todos.clear();
    for (const workflow of this.workflows.values()) {
      for (const step of workflow.steps) {
        for (const todo of step.todos) {
          this.todos.set(todo.id, todo);
        }
      }
    }
  }

  private saveWorkflows(): void {
    this.context.globalState.update('workflows', Object.fromEntries(this.workflows));
  }
}