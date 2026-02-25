// BigDaddyGEngine/copilot/ContextTracker.ts
// Context tracking for session state and personalization

export interface ContextEntry {
  task: string;
  agentId: string;
  output: any;
  timestamp: number;
}

export class ContextTracker {
  public history: ContextEntry[] = [];
  public recentAgent: string | null = null;

  /**
   * Update context with task execution
   */
  update(task: any, agentId: string, output: any): void {
    const entry: ContextEntry = {
      task: task.text || task,
      agentId,
      output,
      timestamp: Date.now()
    };

    this.history.push(entry);
    this.recentAgent = agentId;

    // Keep history manageable (last 100 entries)
    if (this.history.length > 100) {
      this.history = this.history.slice(-100);
    }

    console.log(`📝 Context updated: ${agentId} executed task`);
  }

  /**
   * Get last n entries
   */
  getLast(n: number = 5): ContextEntry[] {
    return this.history.slice(-n);
  }

  /**
   * Summarize context history
   */
  summarize(): string {
    if (this.history.length === 0) {
      return 'No context history available';
    }

    const entries = this.getLast(10);
    return entries.map(entry => 
      `${entry.agentId}: ${entry.task}`
    ).join('\n');
  }

  /**
   * Get recent agents
   */
  getRecentAgents(): string[] {
    const uniqueAgents = new Set<string>();
    this.getLast(20).forEach(entry => {
      uniqueAgents.add(entry.agentId);
    });
    return Array.from(uniqueAgents);
  }

  /**
   * Get context by agent
   */
  getByAgent(agentId: string): ContextEntry[] {
    return this.history.filter(entry => entry.agentId === agentId);
  }

  /**
   * Get context within time range
   */
  getByTimeRange(startTime: number, endTime: number): ContextEntry[] {
    return this.history.filter(entry => 
      entry.timestamp >= startTime && entry.timestamp <= endTime
    );
  }

  /**
   * Find similar tasks in history
   */
  findSimilar(taskText: string, threshold: number = 0.5): ContextEntry[] {
    return this.history.filter(entry => {
      const similarity = this.calculateSimilarity(entry.task, taskText);
      return similarity >= threshold;
    });
  }

  /**
   * Calculate simple string similarity (Jaccard similarity)
   */
  private calculateSimilarity(str1: string, str2: string): number {
    const words1 = new Set(str1.toLowerCase().split(/\s+/));
    const words2 = new Set(str2.toLowerCase().split(/\s+/));
    
    const intersection = [...words1].filter(x => words2.has(x)).length;
    const union = words1.size + words2.size - intersection;
    
    return intersection / union;
  }

  /**
   * Clear context history
   */
  clear(): void {
    this.history = [];
    this.recentAgent = null;
    console.log('🗑️ Context history cleared');
  }

  /**
   * Get statistics
   */
  getStats(): {
    totalEntries: number;
    uniqueAgents: number;
    timeSpan: number;
    mostUsedAgent: string | null;
  } {
    const uniqueAgents = new Set(this.history.map(e => e.agentId));
    const agentCounts = new Map<string, number>();
    
    this.history.forEach(entry => {
      agentCounts.set(entry.agentId, (agentCounts.get(entry.agentId) || 0) + 1);
    });

    const mostUsedAgent = agentCounts.size > 0
      ? Array.from(agentCounts.entries()).reduce((a, b) => a[1] > b[1] ? a : b)[0]
      : null;

    const timeSpan = this.history.length > 0
      ? this.history[this.history.length - 1].timestamp - this.history[0].timestamp
      : 0;

    return {
      totalEntries: this.history.length,
      uniqueAgents: uniqueAgents.size,
      timeSpan,
      mostUsedAgent
    };
  }
}
