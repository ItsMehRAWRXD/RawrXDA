import * as https from 'https';

export interface SearchResult {
  title: string;
  url: string;
  snippet: string;
}

export class WebSearchAgent {
  async search(query: string): Promise<SearchResult[]> {
    // Simple DuckDuckGo instant answer API
    const url = `https://api.duckduckgo.com/?q=${encodeURIComponent(query)}&format=json&no_html=1`;
    
    return new Promise((resolve, reject) => {
      https.get(url, res => {
        let data = '';
        res.on('data', chunk => data += chunk);
        res.on('end', () => {
          try {
            const result = JSON.parse(data);
            const results: SearchResult[] = [];
            
            if (result.RelatedTopics) {
              result.RelatedTopics.slice(0, 5).forEach((topic: any) => {
                if (topic.Text && topic.FirstURL) {
                  results.push({
                    title: topic.Text.split(' - ')[0] || 'Result',
                    url: topic.FirstURL,
                    snippet: topic.Text
                  });
                }
              });
            }
            
            resolve(results);
          } catch (e) {
            resolve([]);
          }
        });
      }).on('error', () => resolve([]));
    });
  }

  async findAgents(query: string = 'AI agents tools'): Promise<string[]> {
    const results = await this.search(query);
    return results.map(r => r.title).filter(title => 
      title.toLowerCase().includes('agent') || 
      title.toLowerCase().includes('ai') ||
      title.toLowerCase().includes('tool')
    );
  }
}