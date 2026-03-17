/**
 * BigDaddyG IDE - Deep Research Engine
 * Multi-source research aggregation with parallel fetching
 */

class DeepResearchEngine {
  constructor() {
    this.sources = [
      { name: 'StackOverflow', url: 'https://api.stackexchange.com/2.3/search', enabled: true },
      { name: 'GitHub', url: 'https://api.github.com/search', enabled: true },
      { name: 'DevDocs', url: 'https://devdocs.io/search', enabled: true },
      { name: 'NPM', url: 'https://registry.npmjs.org/-/v1/search', enabled: true }
    ];
    this.cache = new Map();
    this.maxConcurrent = 5;
  }

  async research(query, options = {}) {
    const {
      depth = 'normal', // 'quick' | 'normal' | 'deep'
      sources = this.sources.filter(s => s.enabled),
      timeout = 10000,
      useCache = true
    } = options;

    console.log(`[Research] Starting ${depth} research: "${query}"`);
    const startTime = Date.now();

    // Check cache
    const cacheKey = `${query}:${depth}`;
    if (useCache && this.cache.has(cacheKey)) {
      console.log('[Research] Cache hit');
      return this.cache.get(cacheKey);
    }

    // Parallel search across sources
    const searches = sources.map(source =>
      this.searchSource(source, query, { depth, timeout })
    );

    const results = await Promise.allSettled(searches);

    // Aggregate results
    const aggregated = this.aggregateResults(results, query);

    // Rank by relevance
    const ranked = this.rankResults(aggregated, query);

    // Generate summary
    const summary = this.generateSummary(ranked, depth);

    const elapsed = Date.now() - startTime;
    console.log(`[Research] Completed in ${elapsed}ms, found ${ranked.length} results`);

    const output = { query, summary, results: ranked, sources: sources.length, elapsed };

    // Cache results
    this.cache.set(cacheKey, output);

    return output;
  }

  async searchSource(source, query, options) {
    try {
      switch (source.name) {
        case 'StackOverflow':
          return await this.searchStackOverflow(query, options);
        case 'GitHub':
          return await this.searchGitHub(query, options);
        case 'DevDocs':
          return await this.searchDevDocs(query, options);
        case 'NPM':
          return await this.searchNPM(query, options);
        default:
          return { source: source.name, results: [] };
      }
    } catch (error) {
      console.error(`[Research] ${source.name} error:`, error.message);
      return { source: source.name, results: [], error: error.message };
    }
  }

  async searchStackOverflow(query, options) {
    const params = new URLSearchParams({
      order: 'desc',
      sort: 'relevance',
      q: query,
      site: 'stackoverflow',
      pagesize: options.depth === 'quick' ? 5 : options.depth === 'deep' ? 20 : 10
    });

    const response = await fetch(`https://api.stackexchange.com/2.3/search?${params}`, {
      signal: AbortSignal.timeout(options.timeout)
    });

    const data = await response.json();

    return {
      source: 'StackOverflow',
      results: (data.items || []).map(item => ({
        title: item.title,
        url: item.link,
        score: item.score,
        answered: item.is_answered,
        tags: item.tags
      }))
    };
  }

  async searchGitHub(query, options) {
    const params = new URLSearchParams({
      q: query,
      sort: 'stars',
      order: 'desc',
      per_page: options.depth === 'quick' ? 5 : options.depth === 'deep' ? 20 : 10
    });

    const response = await fetch(`https://api.github.com/search/repositories?${params}`, {
      signal: AbortSignal.timeout(options.timeout),
      headers: { 'Accept': 'application/vnd.github.v3+json' }
    });

    const data = await response.json();

    return {
      source: 'GitHub',
      results: (data.items || []).map(item => ({
        title: item.full_name,
        description: item.description,
        url: item.html_url,
        stars: item.stargazers_count,
        language: item.language
      }))
    };
  }

  async searchDevDocs(query, options) {
    // DevDocs doesn't have a public API, simulate local docs search
    return {
      source: 'DevDocs',
      results: []
    };
  }

  async searchNPM(query, options) {
    const params = new URLSearchParams({
      text: query,
      size: options.depth === 'quick' ? 5 : options.depth === 'deep' ? 20 : 10
    });

    const response = await fetch(`https://registry.npmjs.org/-/v1/search?${params}`, {
      signal: AbortSignal.timeout(options.timeout)
    });

    const data = await response.json();

    return {
      source: 'NPM',
      results: (data.objects || []).map(obj => ({
        title: obj.package.name,
        description: obj.package.description,
        url: `https://www.npmjs.com/package/${obj.package.name}`,
        version: obj.package.version,
        score: obj.score.final
      }))
    };
  }

  aggregateResults(settledResults, query) {
    const aggregated = [];

    for (const result of settledResults) {
      if (result.status === 'fulfilled' && result.value.results) {
        aggregated.push(...result.value.results.map(r => ({
          ...r,
          source: result.value.source
        })));
      }
    }

    return aggregated;
  }

  rankResults(results, query) {
    const queryTerms = query.toLowerCase().split(/\s+/);

    return results.sort((a, b) => {
      const scoreA = this.calculateRelevance(a, queryTerms);
      const scoreB = this.calculateRelevance(b, queryTerms);
      return scoreB - scoreA;
    });
  }

  calculateRelevance(result, queryTerms) {
    let score = 0;
    const text = `${result.title} ${result.description || ''}`.toLowerCase();

    // Exact match bonus
    if (text.includes(queryTerms.join(' '))) score += 10;

    // Term matching
    queryTerms.forEach(term => {
      if (text.includes(term)) score += 2;
    });

    // Source-specific scoring
    if (result.score) score += result.score * 0.1;
    if (result.stars) score += Math.log(result.stars + 1);
    if (result.answered) score += 5;

    return score;
  }

  generateSummary(results, depth) {
    if (results.length === 0) {
      return 'No results found.';
    }

    const top = results.slice(0, depth === 'deep' ? 10 : 5);
    const sources = [...new Set(top.map(r => r.source))];

    return {
      total: results.length,
      topSources: sources,
      recommendations: top.map(r => ({
        title: r.title,
        url: r.url,
        source: r.source
      }))
    };
  }

  clearCache() {
    this.cache.clear();
  }
}

// Export for browser context
if (typeof window !== 'undefined') {
  window.DeepResearchEngine = DeepResearchEngine;
} else if (typeof module !== 'undefined' && module.exports) {
  module.exports = DeepResearchEngine;
}

