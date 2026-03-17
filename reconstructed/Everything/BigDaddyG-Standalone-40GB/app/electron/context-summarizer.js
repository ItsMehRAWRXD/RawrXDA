/**
 * BigDaddyG IDE - Fast Context Summarizer
 * Efficiently summarizes chat history for better AI context management
 */

class ContextSummarizer {
  constructor() {
    this.summarizationCache = new Map();
    this.compressionRatio = 0.3; // Target 30% of original size
  }

  async summarize(messages, options = {}) {
    const {
      maxTokens = 2000,
      preserveRecent = 5,
      strategy = 'hierarchical' // 'hierarchical' | 'semantic' | 'hybrid'
    } = options;

    // Fast path: if messages are already small enough
    const totalTokens = this.estimateTokens(messages);
    if (totalTokens <= maxTokens) {
      return messages;
    }

    // Always preserve the most recent messages
    const recent = messages.slice(-preserveRecent);
    const toSummarize = messages.slice(0, -preserveRecent);

    // Choose summarization strategy
    let summarized;
    switch (strategy) {
      case 'hierarchical':
        summarized = await this.hierarchicalSummarize(toSummarize, maxTokens - this.estimateTokens(recent));
        break;
      case 'semantic':
        summarized = await this.semanticSummarize(toSummarize, maxTokens - this.estimateTokens(recent));
        break;
      case 'hybrid':
        summarized = await this.hybridSummarize(toSummarize, maxTokens - this.estimateTokens(recent));
        break;
    }

    return [...summarized, ...recent];
  }

  async hierarchicalSummarize(messages, targetTokens) {
    // Group messages into conversation chunks
    const chunks = this.groupByConversation(messages);

    // Summarize each chunk
    const summaries = await Promise.all(
      chunks.map(chunk => this.summarizeChunk(chunk))
    );

    // If still too long, recursively summarize
    const totalTokens = this.estimateTokens(summaries);
    if (totalTokens > targetTokens && summaries.length > 1) {
      return this.hierarchicalSummarize(summaries, targetTokens);
    }

    return summaries;
  }

  async semanticSummarize(messages, targetTokens) {
    // Extract key information points
    const keyPoints = messages.map(msg => this.extractKeyPoints(msg));

    // Remove redundancy
    const unique = this.deduplicatePoints(keyPoints.flat());

    // Rank by importance
    const ranked = this.rankByImportance(unique);

    // Take top N that fit in target tokens
    return this.fitToTokenLimit(ranked, targetTokens);
  }

  async hybridSummarize(messages, targetTokens) {
    // Combine both strategies for best results
    const hierarchical = await this.hierarchicalSummarize(messages, targetTokens);
    const semantic = await this.semanticSummarize(messages, targetTokens);

    // Merge and deduplicate
    return this.mergeStrategies(hierarchical, semantic, targetTokens);
  }

  groupByConversation(messages) {
    const chunks = [];
    let current = [];

    for (const msg of messages) {
      current.push(msg);

      // Start new chunk on topic shifts or every 10 messages
      if (current.length >= 10 || this.detectTopicShift(current)) {
        chunks.push(current);
        current = [];
      }
    }

    if (current.length > 0) chunks.push(current);
    return chunks;
  }

  async summarizeChunk(chunk) {
    const key = JSON.stringify(chunk);

    // Check cache
    if (this.summarizationCache.has(key)) {
      return this.summarizationCache.get(key);
    }

    // Extract essential info
    const summary = {
      role: 'assistant',
      content: `[Summary] ${this.extractEssence(chunk)}`,
      metadata: {
        originalCount: chunk.length,
        topics: this.extractTopics(chunk),
        timestamp: Date.now()
      }
    };

    this.summarizationCache.set(key, summary);
    return summary;
  }

  extractEssence(messages) {
    // Extract code blocks, commands, and key decisions
    const code = messages.filter(m => m.content.includes('```')).map(m => '```code```');
    const commands = messages.filter(m => m.content.match(/^\s*(npm|node|python|cargo|go)\s/));
    const decisions = messages.filter(m => m.content.match(/✅|✓|completed|finished|done/i));

    const parts = [];
    if (code.length > 0) parts.push(`${code.length} code blocks`);
    if (commands.length > 0) parts.push(`executed ${commands.length} commands`);
    if (decisions.length > 0) parts.push(`made ${decisions.length} decisions`);

    return parts.join(', ') || 'General discussion';
  }

  extractTopics(messages) {
    const topics = new Set();
    const keywords = ['security', 'performance', 'bug', 'feature', 'optimization', 'refactor', 'test'];

    messages.forEach(msg => {
      keywords.forEach(kw => {
        if (msg.content.toLowerCase().includes(kw)) topics.add(kw);
      });
    });

    return Array.from(topics);
  }

  extractKeyPoints(message) {
    const points = [];
    const lines = message.content.split('\n');

    for (const line of lines) {
      // Extract bullets, numbered lists, headers
      if (line.match(/^(\*|-|\d+\.|\#{1,3})\s/)) {
        points.push(line.trim());
      }
    }

    return points;
  }

  deduplicatePoints(points) {
    const seen = new Set();
    return points.filter(p => {
      const normalized = p.toLowerCase().replace(/[^a-z0-9]/g, '');
      if (seen.has(normalized)) return false;
      seen.add(normalized);
      return true;
    });
  }

  rankByImportance(points) {
    return points.sort((a, b) => {
      const scoreA = this.calculateImportance(a);
      const scoreB = this.calculateImportance(b);
      return scoreB - scoreA;
    });
  }

  calculateImportance(point) {
    let score = 0;

    // Higher weight for certain keywords
    if (point.match(/error|bug|fix|critical|security/i)) score += 10;
    if (point.match(/feature|implement|add/i)) score += 5;
    if (point.match(/optimize|performance/i)) score += 3;

    // Code blocks are important
    if (point.includes('```')) score += 8;

    return score;
  }

  fitToTokenLimit(items, limit) {
    const result = [];
    let tokens = 0;

    for (const item of items) {
      const itemTokens = this.estimateTokens([item]);
      if (tokens + itemTokens <= limit) {
        result.push(item);
        tokens += itemTokens;
      } else {
        break;
      }
    }

    return result;
  }

  mergeStrategies(strategyA, strategyB, targetTokens) {
    const combined = [...strategyA, ...strategyB];
    const unique = this.deduplicatePoints(combined.map(m => m.content));
    return this.fitToTokenLimit(unique.map(c => ({ role: 'assistant', content: c })), targetTokens);
  }

  detectTopicShift(messages) {
    if (messages.length < 2) return false;

    const last = messages[messages.length - 1].content.toLowerCase();
    const prev = messages[messages.length - 2].content.toLowerCase();

    // Simple topic shift detection
    const lastWords = new Set(last.split(/\s+/));
    const prevWords = new Set(prev.split(/\s+/));

    const overlap = [...lastWords].filter(w => prevWords.has(w)).length;
    const similarity = overlap / Math.max(lastWords.size, prevWords.size);

    return similarity < 0.2; // Less than 20% word overlap
  }

  estimateTokens(messages) {
    if (!Array.isArray(messages)) return 0;
    const text = messages.map(m => m.content || m).join(' ');
    return Math.ceil(text.length / 4); // Rough estimate: 1 token ≈ 4 chars
  }
}

// Export for browser context
if (typeof window !== 'undefined') {
  window.ContextSummarizer = ContextSummarizer;
} else if (typeof module !== 'undefined' && module.exports) {
  module.exports = ContextSummarizer;
}

