import * as vscode from 'vscode';
import { WebSearchAgent } from './WebSearchAgent';
import { WorkflowManager } from './WorkflowManager';

export class AgentFinder {
  private webSearch = new WebSearchAgent();
  
  async findCodingAgents(): Promise<CodingAgent[]> {
    const agents: CodingAgent[] = [];
    
    // Search for coding agents
    const searchResults = await this.webSearch.search('AI coding agents GitHub repositories');
    
    for (const result of searchResults) {
      if (this.isCodingAgent(result.snippet)) {
        agents.push({
          name: result.title,
          url: result.url,
          description: result.snippet,
          skills: this.extractSkills(result.snippet),
          status: 'available'
        });
      }
    }
    
    return agents;
  }

  async helpAgentWithCoding(agent: CodingAgent, task: string): Promise<string> {
    const helpPrompt = `Help coding agent "${agent.name}" with task: ${task}
    Agent skills: ${agent.skills.join(', ')}
    Provide specific coding assistance and guidance.`;
    
    return `Helping ${agent.name}:
1. Code structure for ${task}
2. Best practices for ${agent.skills[0]}
3. Error handling patterns
4. Testing strategies
5. Documentation templates`;
  }

  private isCodingAgent(text: string): boolean {
    const codingKeywords = ['code', 'programming', 'developer', 'AI', 'automation', 'script'];
    return codingKeywords.some(keyword => 
      text.toLowerCase().includes(keyword)
    );
  }

  private extractSkills(text: string): string[] {
    const skills = ['JavaScript', 'Python', 'Java', 'TypeScript', 'React', 'Node.js'];
    return skills.filter(skill => 
      text.toLowerCase().includes(skill.toLowerCase())
    );
  }
}

export interface CodingAgent {
  name: string;
  url: string;
  description: string;
  skills: string[];
  status: 'available' | 'busy' | 'offline';
}