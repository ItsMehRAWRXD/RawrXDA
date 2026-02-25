// BigDaddyGEngine/ui/search/Indexer.ts
// Full-Text Search Indexer

export interface IndexEntry {
  file: string;
  positions: Map<string, number[]>; // term -> positions
  termCount: number;
  lastModified: number;
}

export interface SearchResult {
  file: string;
  score: number;
  matches: Match[];
}

export interface Match {
  term: string;
  positions: number[];
  context: string;
}

export class SearchIndexer {
  private index: Map<string, IndexEntry> = new Map();
  private stopWords: Set<string> = new Set([
    'the', 'a', 'an', 'and', 'or', 'but', 'in', 'on', 'at', 'to', 'for', 'of', 'with', 'by'
  ]);

  /**
   * Index a file
   */
  async indexFile(file: string, content: string): Promise<void> {
    const terms = this.tokenize(content);
    const positions = new Map<string, number[]>();

    terms.forEach((term, position) => {
      if (!positions.has(term)) {
        positions.set(term, []);
      }
      positions.get(term)!.push(position);
    });

    const entry: IndexEntry = {
      file,
      positions,
      termCount: terms.length,
      lastModified: Date.now()
    };

    this.index.set(file, entry);
  }

  /**
   * Remove a file from the index
   */
  removeFile(file: string): void {
    this.index.delete(file);
  }

  /**
   * Update index when file changes
   */
  async updateFile(file: string, content: string): Promise<void> {
    await this.indexFile(file, content);
  }

  /**
   * Search across indexed files
   */
  search(query: string, options: {
    maxResults?: number;
    minScore?: number;
    fuzzy?: boolean;
  } = {}): SearchResult[] {
    const terms = this.tokenize(query.toLowerCase());
    const results = new Map<string, { score: number; matches: Match[] }>();

    // Score each file
    this.index.forEach((entry, file) => {
      const matches: Match[] = [];
      let score = 0;

      terms.forEach(term => {
        const positions = entry.positions.get(term);
        if (positions) {
          // Calculate term frequency score
          const frequency = positions.length / entry.termCount;
          score += frequency * 100;

          matches.push({
            term,
            positions,
            context: `...context around term...`
          });
        }
      });

      if (matches.length > 0) {
        results.set(file, { score, matches });
      }
    });

    // Sort by score and apply options
    const sortedResults: SearchResult[] = Array.from(results.entries())
      .map(([file, data]) => ({
        file,
        score: data.score,
        matches: data.matches
      }))
      .sort((a, b) => b.score - a.score)
      .filter(result => result.score >= (options.minScore || 0))
      .slice(0, options.maxResults || 100);

    return sortedResults;
  }

  /**
   * Get index statistics
   */
  getStats(): {
    filesIndexed: number;
    totalTerms: number;
    avgTermsPerFile: number;
  } {
    let totalTerms = 0;
    this.index.forEach(entry => {
      totalTerms += entry.termCount;
    });

    return {
      filesIndexed: this.index.size,
      totalTerms,
      avgTermsPerFile: this.index.size > 0 ? totalTerms / this.index.size : 0
    };
  }

  /**
   * Clear the entire index
   */
  clear(): void {
    this.index.clear();
  }

  /**
   * Tokenize text into terms
   */
  private tokenize(text: string): string[] {
    return text
      .toLowerCase()
      .replace(/[^\w\s]/g, ' ')
      .split(/\s+/)
      .filter(term => term.length > 2 && !this.stopWords.has(term));
  }
}

export class IncrementalIndexer extends SearchIndexer {
  private indexQueue: Array<{ file: string; content: string }> = [];
  private isIndexing: boolean = false;

  /**
   * Queue a file for indexing
   */
  queueFile(file: string, content: string): void {
    this.indexQueue.push({ file, content });
    if (!this.isIndexing) {
      this.processQueue();
    }
  }

  /**
   * Process indexing queue
   */
  private async processQueue(): Promise<void> {
    this.isIndexing = true;

    while (this.indexQueue.length > 0) {
      const item = this.indexQueue.shift()!;
      await this.indexFile(item.file, item.content);
    }

    this.isIndexing = false;
  }

  /**
   * Get queue status
   */
  getQueueStatus(): { queued: number; indexing: boolean } {
    return {
      queued: this.indexQueue.length,
      indexing: this.isIndexing
    };
  }
}

export function getGlobalSearchIndexer(): SearchIndexer {
  return globalSearchIndexer;
}

export function getGlobalIncrementalIndexer(): IncrementalIndexer {
  return globalIncrementalIndexer;
}

const globalSearchIndexer = new SearchIndexer();
const globalIncrementalIndexer = new IncrementalIndexer();
