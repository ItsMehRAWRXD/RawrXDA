#!/usr/bin/env python3
"""
Analytics Integration Example for BigDaddyG Agent Orchestrator
Shows how to integrate custom analytics tracking with your AI agents
"""

import requests
import json
import time
import uuid
from datetime import datetime
from typing import Dict, Any, Optional


class AnalyticsTracker:
    """Privacy-focused analytics tracker for AI agent interactions"""

    def __init__(self, api_endpoint: str = "http://localhost:3003/analytics"):
        self.api_endpoint = api_endpoint
        self.session_id = str(uuid.uuid4())
        self.user_id = f"user_{int(time.time())}_{uuid.uuid4().hex[:8]}"

    def track_event(self, event_type: str, properties: Dict[str, Any] = None):
        """Track a custom analytics event"""
        event = {
            "event": event_type,
            "properties": {
                "sessionId": self.session_id,
                "userId": self.user_id,
                "timestamp": datetime.now().isoformat(),
                "url": "agent_platform",  # Since we're in a backend context
                **(properties or {})
            },
            "clientTimestamp": int(time.time() * 1000)
        }

        try:
            response = requests.post(
                self.api_endpoint,
                json={"events": [event]},
                timeout=5
            )
            if response.status_code == 200:
                print(f"✅ Tracked event: {event_type}")
            else:
                print(f"❌ Failed to track event: {response.status_code}")
        except Exception as e:
            print(f"❌ Analytics error: {e}")

    def track_query(self, query: str, agent_name: str, metadata: Dict[str, Any] = None):
        """Track when a user submits a query"""
        # Categorize query for insights (privacy-preserving)
        query_category = self._categorize_query(query)

        properties = {
            "agentName": agent_name,
            "queryLength": len(query),
            "queryType": query_category,
            "hasCode": "```" in query or "`" in query,
            "hasUrls": "http" in query.lower(),
            "wordCount": len(query.split()),
            **(metadata or {})
        }

        self.track_event("user_query", properties)

    def track_response(self, response: str, query: str, agent_name: str,
                      response_time: float, metadata: Dict[str, Any] = None):
        """Track when an agent provides a response"""
        query_category = self._categorize_query(query)

        properties = {
            "agentName": agent_name,
            "responseLength": len(response),
            "responseTime": response_time,
            "wordCount": len(response.split()),
            "hasCode": "```" in response or "`" in response,
            "queryCategory": query_category,
            "success": True,
            **(metadata or {})
        }

        self.track_event("agent_response", properties)

    def track_error(self, error: Exception, query: str, agent_name: str,
                   context: Dict[str, Any] = None):
        """Track when an agent encounters an error"""
        properties = {
            "agentName": agent_name,
            "errorMessage": str(error),
            "errorType": self._categorize_error(error),
            "queryLength": len(query),
            "queryType": self._categorize_query(query),
            **(context or {})
        }

        self.track_event("agent_error", properties)

    def track_agent_interaction(self, agent_name: str, action: str,
                               details: Dict[str, Any] = None):
        """Track general agent interactions"""
        properties = {
            "agentName": agent_name,
            "action": action,  # 'started', 'completed', 'failed', etc.
            **(details or {})
        }

        self.track_event("agent_interaction", properties)

    def _categorize_query(self, query: str) -> str:
        """Categorize queries for analytics insights (privacy-preserving)"""
        query_lower = query.lower()

        # Pattern matching for query categorization
        if any(word in query_lower for word in ['help', 'how', 'what', 'why', 'when', 'where']):
            return 'informational'
        elif any(word in query_lower for word in ['create', 'make', 'build', 'generate', 'write']):
            return 'creative'
        elif any(word in query_lower for word in ['fix', 'debug', 'error', 'issue', 'problem']):
            return 'troubleshooting'
        elif any(word in query_lower for word in ['explain', 'describe', 'tell me']):
            return 'explanatory'
        elif any(word in query_lower for word in ['code', 'programming', 'function', 'class', 'script']):
            return 'technical'
        else:
            return 'other'

    def _categorize_error(self, error: Exception) -> str:
        """Categorize errors for debugging insights"""
        error_msg = str(error).lower()

        if 'network' in error_msg or 'connection' in error_msg:
            return 'network'
        elif 'timeout' in error_msg:
            return 'timeout'
        elif 'parse' in error_msg or 'syntax' in error_msg:
            return 'parsing'
        elif 'permission' in error_msg or 'auth' in error_msg:
            return 'authorization'
        else:
            return 'unknown'


class AnalyticsAwareAgent:
    """Example agent wrapper that includes analytics tracking"""

    def __init__(self, agent_name: str, agent_function):
        self.agent_name = agent_name
        self.agent_function = agent_function
        self.analytics = AnalyticsTracker()
        self.analytics.track_agent_interaction(agent_name, 'initialized')

    def process_request(self, query: str, session_id: str = None) -> str:
        """Process a user request with analytics tracking"""
        start_time = time.time()

        try:
            # Track the incoming query
            self.analytics.track_query(query, self.agent_name)

            # Process the request (this would call your actual agent)
            response = self.agent_function(query)

            # Calculate response time
            response_time = time.time() - start_time

            # Track successful response
            self.analytics.track_response(
                response, query, self.agent_name, response_time
            )

            self.analytics.track_agent_interaction(
                self.agent_name, 'completed',
                {'responseTime': response_time}
            )

            return response

        except Exception as e:
            # Calculate response time even for errors
            response_time = time.time() - start_time

            # Track the error
            self.analytics.track_error(e, query, self.agent_name, {
                'responseTime': response_time
            })

            self.analytics.track_agent_interaction(
                self.agent_name, 'failed',
                {'error': str(e), 'responseTime': response_time}
            )

            # Re-raise the exception
            raise e


# Example usage with your BigDaddyG Agent Orchestrator
def example_bigdaddyg_agent(query: str) -> str:
    """Example agent function - replace with your actual agent logic"""
    # Simulate agent processing
    time.sleep(0.5)  # Simulate processing time

    if "hello" in query.lower():
        return "Hello! I'm BigDaddyG, your AI assistant. How can I help you today?"
    elif "analytics" in query.lower():
        return "I can help you track user interactions and improve your AI agents based on real usage data!"
    else:
        return f"I received your query: '{query}'. This is a demo response from BigDaddyG."


def demonstrate_analytics_integration():
    """Demonstrate how analytics integration works"""

    # Create an analytics-aware version of your agent
    agent = AnalyticsAwareAgent("BigDaddyG", example_bigdaddyg_agent)

    # Simulate some user interactions
    test_queries = [
        "Hello, can you help me?",
        "How do I integrate analytics with my AI agents?",
        "I need help with debugging my code",
        "Can you explain how machine learning works?"
    ]

    for i, query in enumerate(test_queries, 1):
        print(f"\n--- Query {i} ---")
        print(f"User: {query}")

        try:
            response = agent.process_request(query, f"session_{i}")
            print(f"Agent: {response[:100]}...")  # Show first 100 chars
        except Exception as e:
            print(f"Error: {e}")

        # Small delay between queries
        time.sleep(1)

    print("
✅ Analytics demonstration complete!"    print("📊 Check your analytics dashboard at: http://localhost:3003/analytics/summary")


if __name__ == "__main__":
    print("🤖 BigDaddyG Agent Analytics Integration Demo")
    print("=" * 50)

    demonstrate_analytics_integration()

    print("\n📈 To see analytics insights:")
    print("1. Start the analytics API server: node analytics_api_server.js")
    print("2. Open the dashboard: analytics_dashboard.html")
    print("3. View collected data at: http://localhost:3003/analytics/summary")
