import { WorkflowManager, Todo, WorkflowStep } from './WorkflowManager';
import { createProvider, setProvider } from './ai/provider';
import { WebSearchAgent } from './WebSearchAgent';

export class AgentOrchestrator {
  private webSearch = new WebSearchAgent();
  
  constructor(private workflowManager: WorkflowManager) {}

  async initializeAgent(agentId: string, provider: string): Promise<void> {
    setProvider(provider);
    const workflow = this.workflowManager.createWorkflow(agentId);
    
    // Default workflow steps
    const steps: Omit<WorkflowStep, 'id' | 'todos' | 'completed'>[] = [
      { name: 'Code Generation', type: 'code' },
      { name: 'Code Review', type: 'review' },
      { name: 'Testing', type: 'test' }
    ];

    steps.forEach(step => {
      const stepId = Date.now().toString() + Math.random();
      workflow.steps.push({
        ...step,
        id: stepId,
        todos: [],
        completed: false
      });
    });
  }

  async executeWorkflow(agentId: string, task: string): Promise<void> {
    const workflow = this.workflowManager.getWorkflow(agentId);
    if (!workflow) return;

    const provider = createProvider();
    
    for (const step of workflow.steps) {
      if (!step.completed) {
        const prompt = `Execute ${step.name} for task: ${task}`;
        const result = await provider.respond({
          messages: [{ role: 'user', content: prompt }]
        });
        
        // Create todo based on AI response
        this.workflowManager.addTodo(agentId, step.id, `Complete: ${result.substring(0, 50)}...`);
        step.completed = true;
      }
    }
  }

  async discoverNewAgents(): Promise<string[]> {
    return await this.webSearch.findAgents('AI development tools agents');
  }

  async addAgentFromSearch(agentName: string, capabilities: string[]): Promise<void> {
    const agentId = agentName.toLowerCase().replace(/\s+/g, '-');
    await this.initializeAgent(agentId, 'echo');
    
    const workflow = this.workflowManager.getWorkflow(agentId);
    if (workflow && workflow.steps[0]) {
      capabilities.forEach(cap => {
        this.workflowManager.addTodo(agentId, workflow.steps[0].id, `Implement: ${cap}`);
      });
    }
  }

  getAgentStatus(agentId: string): { totalTodos: number; completedTodos: number } {
    const workflow = this.workflowManager.getWorkflow(agentId);
    if (!workflow) return { totalTodos: 0, completedTodos: 0 };

    const allTodos = workflow.steps.flatMap(s => s.todos);
    return {
      totalTodos: allTodos.length,
      completedTodos: allTodos.filter(t => t.status === 'completed').length
    };
  }
}