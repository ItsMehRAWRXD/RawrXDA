"""
Web Search Tool using DuckDuckGo
"""

import requests
import json
import time
from typing import List, Dict, Any, Optional
import logging
from urllib.parse import quote_plus

from .base_tool import NetworkTool, tool_registry

logger = logging.getLogger(__name__)

class DuckDuckGoSearchTool(NetworkTool):
    """Tool for web search using DuckDuckGo API"""
    
    def __init__(self):
        super().__init__(
            name="web_search",
            description="Search the web for information using DuckDuckGo",
            rate_limit=20  # 20 requests per minute
        )
        self.base_url = "https://api.duckduckgo.com/"
        self.instant_answer_url = "https://api.duckduckgo.com/"
        self.search_url = "https://html.duckduckgo.com/html/"
    
    def validate_parameters(self, **kwargs) -> bool:
        """Validate search parameters"""
        return 'query' in kwargs and isinstance(kwargs['query'], str) and len(kwargs['query'].strip()) > 0
    
    def execute(self, query: str, max_results: int = 5, search_type: str = "web", **kwargs) -> str:
        """Execute web search"""
        try:
            if not self.check_rate_limit():
                return "Rate limit exceeded. Please wait before making another search request."
            
            self.record_request()
            
            if search_type == "instant":
                return self._search_instant_answer(query)
            else:
                return self._search_web(query, max_results)
                
        except Exception as e:
            logger.error(f"Web search error: {str(e)}")
            return f"Search error: {str(e)}"
    
    def _search_instant_answer(self, query: str) -> str:
        """Search for instant answers"""
        try:
            params = {
                'q': query,
                'format': 'json',
                'no_html': '1',
                'skip_disambig': '1'
            }
            
            response = requests.get(self.instant_answer_url, params=params, timeout=10)
            response.raise_for_status()
            
            data = response.json()
            
            results = []
            
            # Check for abstract (instant answer)
            if data.get('Abstract'):
                results.append(f"Instant Answer: {data['Abstract']}")
                if data.get('AbstractURL'):
                    results.append(f"Source: {data['AbstractURL']}")
            
            # Check for definition
            if data.get('Definition'):
                results.append(f"Definition: {data['Definition']}")
                if data.get('DefinitionURL'):
                    results.append(f"Source: {data['DefinitionURL']}")
            
            # Check for related topics
            if data.get('RelatedTopics'):
                results.append("Related Topics:")
                for topic in data['RelatedTopics'][:3]:
                    if isinstance(topic, dict) and topic.get('Text'):
                        results.append(f"- {topic['Text']}")
            
            return "\n".join(results) if results else f"No instant answer found for: '{query}'"
            
        except Exception as e:
            logger.error(f"Instant answer search error: {str(e)}")
            return f"Instant answer search failed: {str(e)}"
    
    def _search_web(self, query: str, max_results: int) -> str:
        """Search the web (simplified implementation)"""
        try:
            # This is a simplified implementation
            # In a real scenario, you'd use a proper web scraping library or API
            
            # For demonstration, we'll simulate web search results
            # In practice, you'd use libraries like requests-html, BeautifulSoup, or selenium
            
            results = []
            results.append(f"Web search results for: '{query}'")
            results.append("=" * 50)
            
            # Simulate search results (in real implementation, these would come from actual web scraping)
            simulated_results = [
                {
                    "title": f"Result 1: {query} - Comprehensive Guide",
                    "url": f"https://example1.com/{query.replace(' ', '-')}",
                    "snippet": f"This is a comprehensive guide about {query}. It covers all the important aspects and provides detailed information."
                },
                {
                    "title": f"Result 2: Understanding {query}",
                    "url": f"https://example2.com/understanding-{query.replace(' ', '-')}",
                    "snippet": f"Learn everything you need to know about {query}. This resource provides practical examples and real-world applications."
                },
                {
                    "title": f"Result 3: {query} Best Practices",
                    "url": f"https://example3.com/{query.replace(' ', '-')}-best-practices",
                    "snippet": f"Discover the best practices for working with {query}. Expert tips and recommendations for optimal results."
                }
            ]
            
            for i, result in enumerate(simulated_results[:max_results], 1):
                results.append(f"\n{i}. {result['title']}")
                results.append(f"   URL: {result['url']}")
                results.append(f"   {result['snippet']}")
            
            results.append(f"\nNote: This is a simulated search. In a real implementation, these would be actual web search results.")
            
            return "\n".join(results)
            
        except Exception as e:
            logger.error(f"Web search error: {str(e)}")
            return f"Web search failed: {str(e)}"

class WikipediaSearchTool(NetworkTool):
    """Tool for searching Wikipedia"""
    
    def __init__(self):
        super().__init__(
            name="wikipedia_search",
            description="Search Wikipedia for information",
            rate_limit=30
        )
        self.base_url = "https://en.wikipedia.org/api/rest_v1/page/summary/"
    
    def validate_parameters(self, **kwargs) -> bool:
        """Validate search parameters"""
        return 'query' in kwargs and isinstance(kwargs['query'], str) and len(kwargs['query'].strip()) > 0
    
    def execute(self, query: str, **kwargs) -> str:
        """Execute Wikipedia search"""
        try:
            if not self.check_rate_limit():
                return "Rate limit exceeded. Please wait before making another search request."
            
            self.record_request()
            
            # Clean query for Wikipedia URL
            clean_query = query.replace(' ', '_').title()
            url = f"{self.base_url}{clean_query}"
            
            response = requests.get(url, timeout=10)
            
            if response.status_code == 200:
                data = response.json()
                
                results = []
                results.append(f"Wikipedia: {data.get('title', query)}")
                results.append("=" * 50)
                
                if data.get('extract'):
                    results.append(data['extract'])
                
                if data.get('content_urls', {}).get('desktop', {}).get('page'):
                    results.append(f"\nFull article: {data['content_urls']['desktop']['page']}")
                
                return "\n".join(results)
            
            elif response.status_code == 404:
                return f"No Wikipedia article found for: '{query}'"
            else:
                return f"Wikipedia search failed with status: {response.status_code}"
                
        except Exception as e:
            logger.error(f"Wikipedia search error: {str(e)}")
            return f"Wikipedia search failed: {str(e)}"

class NewsSearchTool(NetworkTool):
    """Tool for searching news articles"""
    
    def __init__(self):
        super().__init__(
            name="news_search",
            description="Search for recent news articles",
            rate_limit=15
        )
    
    def validate_parameters(self, **kwargs) -> bool:
        """Validate search parameters"""
        return 'query' in kwargs and isinstance(kwargs['query'], str) and len(kwargs['query'].strip()) > 0
    
    def execute(self, query: str, max_results: int = 5, **kwargs) -> str:
        """Execute news search"""
        try:
            if not self.check_rate_limit():
                return "Rate limit exceeded. Please wait before making another search request."
            
            self.record_request()
            
            # This is a simplified implementation
            # In a real scenario, you'd use a news API like NewsAPI, Google News API, etc.
            
            results = []
            results.append(f"News search results for: '{query}'")
            results.append("=" * 50)
            
            # Simulate news results
            simulated_news = [
                {
                    "title": f"Breaking: {query} Makes Headlines",
                    "source": "Tech News",
                    "date": "2024-01-15",
                    "snippet": f"Recent developments in {query} have captured attention across the industry."
                },
                {
                    "title": f"Analysis: The Impact of {query}",
                    "source": "Business Weekly",
                    "date": "2024-01-14",
                    "snippet": f"Experts analyze the growing influence of {query} on modern business practices."
                },
                {
                    "title": f"Research: New Study on {query}",
                    "source": "Science Daily",
                    "date": "2024-01-13",
                    "snippet": f"A new research study provides insights into {query} and its applications."
                }
            ]
            
            for i, article in enumerate(simulated_news[:max_results], 1):
                results.append(f"\n{i}. {article['title']}")
                results.append(f"   Source: {article['source']} ({article['date']})")
                results.append(f"   {article['snippet']}")
            
            results.append(f"\nNote: This is a simulated news search. In a real implementation, these would be actual news articles.")
            
            return "\n".join(results)
            
        except Exception as e:
            logger.error(f"News search error: {str(e)}")
            return f"News search failed: {str(e)}"

# Register tools
tool_registry.register_tool(DuckDuckGoSearchTool(), "search")
tool_registry.register_tool(WikipediaSearchTool(), "search")
tool_registry.register_tool(NewsSearchTool(), "search")
