"""
User Agent Management for Web Requests
Provides realistic user agents to avoid bot detection
"""

import random
from datetime import datetime
from typing import List, Dict

class UserAgentManager:
    """Manages user agents for web requests with rotation and weighting"""
    
    # Current browser versions (as of 2024)
    CHROME_VERSIONS = ["120.0.6099.129", "121.0.6167.85", "122.0.6261.69"]
    FIREFOX_VERSIONS = ["121.0", "122.0", "123.0"]
    EDGE_VERSIONS = ["120.0.2210.144", "121.0.2277.83", "122.0.2365.52"]
    SAFARI_VERSIONS = ["17.2.1", "17.3", "17.3.1"]
    
    # Desktop User Agents (most common)
    DESKTOP_AGENTS = [
        # Chrome on Windows (most common)
        *[f"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/{v} Safari/537.36" 
          for v in CHROME_VERSIONS],
        
        # Chrome on Mac
        *[f"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/{v} Safari/537.36"
          for v in CHROME_VERSIONS],
        
        # Chrome on Linux
        *[f"Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/{v} Safari/537.36"
          for v in CHROME_VERSIONS],
        
        # Firefox on Windows
        *[f"Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:{v.split('.')[0]}.0) Gecko/20100101 Firefox/{v}"
          for v in FIREFOX_VERSIONS],
        
        # Firefox on Mac
        *[f"Mozilla/5.0 (Macintosh; Intel Mac OS X 10.15) Gecko/20100101 Firefox/{v}"
          for v in FIREFOX_VERSIONS],
        
        # Edge on Windows
        *[f"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/{v.split('.')[0]}.0.0.0 Safari/537.36 Edg/{v}"
          for v in EDGE_VERSIONS],
        
        # Safari on Mac
        *[f"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/{v} Safari/605.1.15"
          for v in SAFARI_VERSIONS],
    ]
    
    # Mobile User Agents (less common for scanners, but good for diversity)
    MOBILE_AGENTS = [
        # Chrome on Android
        "Mozilla/5.0 (Linux; Android 13; SM-G991B) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Mobile Safari/537.36",
        "Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Mobile Safari/537.36",
        
        # Safari on iPhone
        "Mozilla/5.0 (iPhone; CPU iPhone OS 17_2_1 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1",
        "Mozilla/5.0 (iPhone; CPU iPhone OS 17_3 like Mac OS X) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.0 Mobile/15E148 Safari/604.1",
    ]
    
    # Bot/Crawler User Agents (use sparingly, some sites block these)
    BOT_AGENTS = [
        "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
        "Mozilla/5.0 (compatible; bingbot/2.0; +http://www.bing.com/bingbot.htm)",
        "Mozilla/5.0 (compatible; DuckDuckBot/1.0; +http://duckduckgo.com/duckduckbot.html)",
    ]
    
    # Headers that often accompany user agents
    COMMON_HEADERS = {
        "chrome": {
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.9",
            "Accept-Encoding": "gzip, deflate, br",
            "DNT": "1",
            "Upgrade-Insecure-Requests": "1",
            "Sec-Fetch-Dest": "document",
            "Sec-Fetch-Mode": "navigate",
            "Sec-Fetch-Site": "none",
            "Sec-Fetch-User": "?1",
            "Sec-Ch-Ua": '"Not_A Brand";v="8", "Chromium";v="121", "Google Chrome";v="121"',
            "Sec-Ch-Ua-Mobile": "?0",
            "Sec-Ch-Ua-Platform": '"Windows"',
        },
        "firefox": {
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.5",
            "Accept-Encoding": "gzip, deflate, br",
            "DNT": "1",
            "Upgrade-Insecure-Requests": "1",
            "Sec-Fetch-Dest": "document",
            "Sec-Fetch-Mode": "navigate",
            "Sec-Fetch-Site": "none",
            "Sec-Fetch-User": "?1",
        },
        "safari": {
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Language": "en-US,en;q=0.9",
            "Accept-Encoding": "gzip, deflate, br",
        }
    }
    
    def __init__(self, prefer_desktop: bool = True, include_mobile: bool = True):
        """
        Initialize User Agent Manager
        
        Args:
            prefer_desktop: Whether to prefer desktop user agents (default: True)
            include_mobile: Whether to include mobile user agents in rotation (default: True)
        """
        self.prefer_desktop = prefer_desktop
        self.include_mobile = include_mobile
        self._last_used_agents: List[str] = []
        self._max_history = 10
    
    def get_random_agent(self, avoid_repeats: bool = True) -> str:
        """
        Get a random user agent string
        
        Args:
            avoid_repeats: Try to avoid recently used agents (default: True)
            
        Returns:
            Random user agent string
        """
        # Build agent pool based on preferences
        agent_pool = self.DESKTOP_AGENTS.copy()
        
        if self.include_mobile:
            # Add mobile agents with lower weight
            agent_pool.extend(self.MOBILE_AGENTS * 2)  # Less weight than desktop
        
        # If prefer_desktop, add more desktop agents to increase probability
        if self.prefer_desktop:
            agent_pool.extend(self.DESKTOP_AGENTS)  # Double the desktop agents
        
        # Try to avoid recently used agents
        if avoid_repeats and self._last_used_agents:
            available_agents = [a for a in agent_pool if a not in self._last_used_agents]
            if available_agents:
                agent_pool = available_agents
        
        # Select random agent
        selected_agent = random.choice(agent_pool)
        
        # Update history
        self._last_used_agents.append(selected_agent)
        if len(self._last_used_agents) > self._max_history:
            self._last_used_agents.pop(0)
        
        return selected_agent
    
    def get_headers_for_agent(self, user_agent: str) -> Dict[str, str]:
        """
        Get appropriate headers for a given user agent
        
        Args:
            user_agent: The user agent string
            
        Returns:
            Dictionary of headers appropriate for the user agent
        """
        headers = {"User-Agent": user_agent}
        
        # Determine browser type and add appropriate headers
        if "Chrome" in user_agent and "Edg" not in user_agent:
            headers.update(self.COMMON_HEADERS["chrome"])
        elif "Firefox" in user_agent:
            headers.update(self.COMMON_HEADERS["firefox"])
        elif "Safari" in user_agent and "Chrome" not in user_agent:
            headers.update(self.COMMON_HEADERS["safari"])
        else:
            # Default headers
            headers.update({
                "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
                "Accept-Language": "en-US,en;q=0.9",
                "Accept-Encoding": "gzip, deflate",
            })
        
        return headers
    
    def get_random_with_headers(self) -> Dict[str, str]:
        """
        Get a random user agent with appropriate headers
        
        Returns:
            Dictionary containing full header set with user agent
        """
        agent = self.get_random_agent()
        return self.get_headers_for_agent(agent)
    
    @staticmethod
    def get_googlebot() -> Dict[str, str]:
        """Get Googlebot user agent with headers (use carefully)"""
        return {
            "User-Agent": "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
            "Accept": "text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8",
            "Accept-Encoding": "gzip, deflate",
            "From": "googlebot(at)googlebot.com",
        }
    
    @classmethod
    def get_simple_list(cls) -> List[str]:
        """Get a simple list of user agents (backward compatibility)"""
        return cls.DESKTOP_AGENTS + cls.MOBILE_AGENTS


# Example usage function
def get_random_headers() -> Dict[str, str]:
    """Quick function to get random headers"""
    manager = UserAgentManager()
    return manager.get_random_with_headers()