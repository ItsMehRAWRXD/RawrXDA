#!/usr/bin/env python3
"""
Cursor IDE Integration System
Unlimited AI access configuration
"""

import os
import sys
import json
import time
import threading
import requests
from typing import Dict, List, Any, Optional
import subprocess
import webbrowser

class CursorAIConfig:
    """Cursor IDE integration system for unlimited AI access"""
    
    def __init__(self):
        self.initialized = False
        self.cursor_config = {}
        self.ai_services = {}
        self.api_keys = {}
        self.connection_status = {}
        
        # AI service configurations
        self.ai_services = {
            'openai': {
                'name': 'OpenAI GPT',
                'api_url': 'https://api.openai.com/v1/chat/completions',
                'models': ['gpt-4', 'gpt-3.5-turbo', 'gpt-4-turbo'],
                'status': 'inactive'
            },
            'anthropic': {
                'name': 'Anthropic Claude',
                'api_url': 'https://api.anthropic.com/v1/messages',
                'models': ['claude-3-opus', 'claude-3-sonnet', 'claude-3-haiku'],
                'status': 'inactive'
            },
            'google': {
                'name': 'Google Gemini',
                'api_url': 'https://generativelanguage.googleapis.com/v1beta/models',
                'models': ['gemini-pro', 'gemini-pro-vision'],
                'status': 'inactive'
            },
            'cohere': {
                'name': 'Cohere',
                'api_url': 'https://api.cohere.ai/v1/chat',
                'models': ['command', 'command-light'],
                'status': 'inactive'
            },
            'huggingface': {
                'name': 'Hugging Face',
                'api_url': 'https://api-inference.huggingface.co/models',
                'models': ['microsoft/DialoGPT-medium', 'facebook/blenderbot-400M-distill'],
                'status': 'inactive'
            }
        }
        
        print("Cursor AI Integration System created")
    
    def initialize(self):
        """Initialize the Cursor AI integration"""
        try:
            # Load configuration
            self.load_configuration()
            
            # Test AI service connections
            self.test_ai_connections()
            
            # Start monitoring thread
            self.start_monitoring()
            
            self.initialized = True
            print("Cursor AI Integration System initialized successfully")
            return True
            
        except Exception as e:
            print(f"Failed to initialize Cursor AI Integration: {e}")
            return False
    
    def load_configuration(self):
        """Load Cursor AI configuration"""
        try:
            config_path = os.path.join(os.getcwd(), 'cursor_ai_config.json')
            
            if os.path.exists(config_path):
                with open(config_path, 'r') as f:
                    self.cursor_config = json.load(f)
                
                # Load API keys
                if 'api_keys' in self.cursor_config:
                    self.api_keys = self.cursor_config['api_keys']
                
                print("Cursor AI configuration loaded")
            else:
                # Create default configuration
                self.create_default_config()
                print("Default Cursor AI configuration created")
                
        except Exception as e:
            print(f"Failed to load configuration: {e}")
            self.create_default_config()
    
    def create_default_config(self):
        """Create default Cursor AI configuration"""
        self.cursor_config = {
            'version': '1.0.0',
            'ai_services': self.ai_services,
            'api_keys': {},
            'settings': {
                'default_model': 'gpt-4',
                'max_tokens': 4000,
                'temperature': 0.7,
                'timeout': 30,
                'retry_attempts': 3,
                'auto_switch': True,
                'fallback_service': 'openai'
            },
            'usage_limits': {
                'daily_requests': 1000,
                'monthly_requests': 30000,
                'cost_limit': 100.0
            },
            'monitoring': {
                'enabled': True,
                'log_level': 'info',
                'save_responses': True
            }
        }
        
        self.save_configuration()
    
    def save_configuration(self):
        """Save Cursor AI configuration"""
        try:
            config_path = os.path.join(os.getcwd(), 'cursor_ai_config.json')
            
            with open(config_path, 'w') as f:
                json.dump(self.cursor_config, f, indent=2)
            
            print("Cursor AI configuration saved")
            
        except Exception as e:
            print(f"Failed to save configuration: {e}")
    
    def add_api_key(self, service_name: str, api_key: str):
        """Add API key for AI service"""
        try:
            if service_name in self.ai_services:
                self.api_keys[service_name] = api_key
                self.cursor_config['api_keys'][service_name] = api_key
                self.save_configuration()
                
                # Test connection
                self.test_service_connection(service_name)
                
                print(f"API key added for {service_name}")
                return True
            else:
                print(f"Unknown service: {service_name}")
                return False
                
        except Exception as e:
            print(f"Failed to add API key: {e}")
            return False
    
    def test_ai_connections(self):
        """Test connections to all AI services"""
        for service_name in self.ai_services:
            self.test_service_connection(service_name)
    
    def test_service_connection(self, service_name: str):
        """Test connection to specific AI service"""
        try:
            if service_name not in self.api_keys:
                self.connection_status[service_name] = 'no_api_key'
                return False
            
            api_key = self.api_keys[service_name]
            service_config = self.ai_services[service_name]
            
            # Test connection based on service
            if service_name == 'openai':
                success = self.test_openai_connection(api_key)
            elif service_name == 'anthropic':
                success = self.test_anthropic_connection(api_key)
            elif service_name == 'google':
                success = self.test_google_connection(api_key)
            elif service_name == 'cohere':
                success = self.test_cohere_connection(api_key)
            elif service_name == 'huggingface':
                success = self.test_huggingface_connection(api_key)
            else:
                success = False
            
            self.connection_status[service_name] = 'connected' if success else 'failed'
            self.ai_services[service_name]['status'] = 'active' if success else 'inactive'
            
            return success
            
        except Exception as e:
            print(f"Failed to test {service_name} connection: {e}")
            self.connection_status[service_name] = 'error'
            return False
    
    def test_openai_connection(self, api_key: str) -> bool:
        """Test OpenAI connection"""
        try:
            headers = {
                'Authorization': f'Bearer {api_key}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': 'gpt-3.5-turbo',
                'messages': [{'role': 'user', 'content': 'Hello'}],
                'max_tokens': 10
            }
            
            response = requests.post(
                'https://api.openai.com/v1/chat/completions',
                headers=headers,
                json=data,
                timeout=10
            )
            
            return response.status_code == 200
            
        except Exception as e:
            print(f"OpenAI connection test failed: {e}")
            return False
    
    def test_anthropic_connection(self, api_key: str) -> bool:
        """Test Anthropic connection"""
        try:
            headers = {
                'x-api-key': api_key,
                'Content-Type': 'application/json',
                'anthropic-version': '2023-06-01'
            }
            
            data = {
                'model': 'claude-3-haiku-20240307',
                'max_tokens': 10,
                'messages': [{'role': 'user', 'content': 'Hello'}]
            }
            
            response = requests.post(
                'https://api.anthropic.com/v1/messages',
                headers=headers,
                json=data,
                timeout=10
            )
            
            return response.status_code == 200
            
        except Exception as e:
            print(f"Anthropic connection test failed: {e}")
            return False
    
    def test_google_connection(self, api_key: str) -> bool:
        """Test Google connection"""
        try:
            url = f'https://generativelanguage.googleapis.com/v1beta/models?key={api_key}'
            
            response = requests.get(url, timeout=10)
            
            return response.status_code == 200
            
        except Exception as e:
            print(f"Google connection test failed: {e}")
            return False
    
    def test_cohere_connection(self, api_key: str) -> bool:
        """Test Cohere connection"""
        try:
            headers = {
                'Authorization': f'Bearer {api_key}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': 'command-light',
                'message': 'Hello',
                'max_tokens': 10
            }
            
            response = requests.post(
                'https://api.cohere.ai/v1/chat',
                headers=headers,
                json=data,
                timeout=10
            )
            
            return response.status_code == 200
            
        except Exception as e:
            print(f"Cohere connection test failed: {e}")
            return False
    
    def test_huggingface_connection(self, api_key: str) -> bool:
        """Test Hugging Face connection"""
        try:
            headers = {
                'Authorization': f'Bearer {api_key}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'inputs': 'Hello, how are you?'
            }
            
            response = requests.post(
                'https://api-inference.huggingface.co/models/microsoft/DialoGPT-medium',
                headers=headers,
                json=data,
                timeout=10
            )
            
            return response.status_code == 200
            
        except Exception as e:
            print(f"Hugging Face connection test failed: {e}")
            return False
    
    def start_monitoring(self):
        """Start monitoring AI service health"""
        monitoring_thread = threading.Thread(target=self.monitor_ai_services)
        monitoring_thread.daemon = True
        monitoring_thread.start()
        
        print("AI service monitoring started")
    
    def monitor_ai_services(self):
        """Monitor AI service health"""
        while True:
            try:
                # Test connections periodically
                for service_name in self.ai_services:
                    if service_name in self.api_keys:
                        self.test_service_connection(service_name)
                
                time.sleep(300)  # Check every 5 minutes
                
            except Exception as e:
                print(f"AI service monitoring error: {e}")
                time.sleep(300)
    
    def get_ai_response(self, prompt: str, service_name: Optional[str] = None, model: Optional[str] = None):
        """Get AI response from specified service"""
        try:
            # Determine which service to use
            if service_name is None:
                service_name = self.get_best_available_service()
            
            if service_name is None:
                return {"error": "No AI services available"}
            
            # Get model
            if model is None:
                model = self.cursor_config['settings']['default_model']
            
            # Get response based on service
            if service_name == 'openai':
                return self.get_openai_response(prompt, model)
            elif service_name == 'anthropic':
                return self.get_anthropic_response(prompt, model)
            elif service_name == 'google':
                return self.get_google_response(prompt, model)
            elif service_name == 'cohere':
                return self.get_cohere_response(prompt, model)
            elif service_name == 'huggingface':
                return self.get_huggingface_response(prompt, model)
            else:
                return {"error": f"Unknown service: {service_name}"}
                
        except Exception as e:
            print(f"Failed to get AI response: {e}")
            return {"error": str(e)}
    
    def get_best_available_service(self) -> Optional[str]:
        """Get the best available AI service"""
        # Check for connected services
        connected_services = []
        for service_name, status in self.connection_status.items():
            if status == 'connected':
                connected_services.append(service_name)
        
        if not connected_services:
            return None
        
        # Return the first connected service (could be improved with load balancing)
        return connected_services[0]
    
    def get_openai_response(self, prompt: str, model: str) -> Dict[str, Any]:
        """Get response from OpenAI"""
        try:
            headers = {
                'Authorization': f'Bearer {self.api_keys["openai"]}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'messages': [{'role': 'user', 'content': prompt}],
                'max_tokens': self.cursor_config['settings']['max_tokens'],
                'temperature': self.cursor_config['settings']['temperature']
            }
            
            response = requests.post(
                'https://api.openai.com/v1/chat/completions',
                headers=headers,
                json=data,
                timeout=self.cursor_config['settings']['timeout']
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'openai',
                    'model': model,
                    'response': result['choices'][0]['message']['content'],
                    'usage': result.get('usage', {}),
                    'success': True
                }
            else:
                return {
                    'service': 'openai',
                    'error': f"API error: {response.status_code}",
                    'success': False
                }
                
        except Exception as e:
            return {
                'service': 'openai',
                'error': str(e),
                'success': False
            }
    
    def get_anthropic_response(self, prompt: str, model: str) -> Dict[str, Any]:
        """Get response from Anthropic"""
        try:
            headers = {
                'x-api-key': self.api_keys["anthropic"],
                'Content-Type': 'application/json',
                'anthropic-version': '2023-06-01'
            }
            
            data = {
                'model': model,
                'max_tokens': self.cursor_config['settings']['max_tokens'],
                'messages': [{'role': 'user', 'content': prompt}]
            }
            
            response = requests.post(
                'https://api.anthropic.com/v1/messages',
                headers=headers,
                json=data,
                timeout=self.cursor_config['settings']['timeout']
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'anthropic',
                    'model': model,
                    'response': result['content'][0]['text'],
                    'usage': result.get('usage', {}),
                    'success': True
                }
            else:
                return {
                    'service': 'anthropic',
                    'error': f"API error: {response.status_code}",
                    'success': False
                }
                
        except Exception as e:
            return {
                'service': 'anthropic',
                'error': str(e),
                'success': False
            }
    
    def get_google_response(self, prompt: str, model: str) -> Dict[str, Any]:
        """Get response from Google"""
        try:
            url = f'https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent?key={self.api_keys["google"]}'
            
            data = {
                'contents': [{'parts': [{'text': prompt}]}],
                'generationConfig': {
                    'maxOutputTokens': self.cursor_config['settings']['max_tokens'],
                    'temperature': self.cursor_config['settings']['temperature']
                }
            }
            
            response = requests.post(
                url,
                json=data,
                timeout=self.cursor_config['settings']['timeout']
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'google',
                    'model': model,
                    'response': result['candidates'][0]['content']['parts'][0]['text'],
                    'usage': result.get('usageMetadata', {}),
                    'success': True
                }
            else:
                return {
                    'service': 'google',
                    'error': f"API error: {response.status_code}",
                    'success': False
                }
                
        except Exception as e:
            return {
                'service': 'google',
                'error': str(e),
                'success': False
            }
    
    def get_cohere_response(self, prompt: str, model: str) -> Dict[str, Any]:
        """Get response from Cohere"""
        try:
            headers = {
                'Authorization': f'Bearer {self.api_keys["cohere"]}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'model': model,
                'message': prompt,
                'max_tokens': self.cursor_config['settings']['max_tokens'],
                'temperature': self.cursor_config['settings']['temperature']
            }
            
            response = requests.post(
                'https://api.cohere.ai/v1/chat',
                headers=headers,
                json=data,
                timeout=self.cursor_config['settings']['timeout']
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'cohere',
                    'model': model,
                    'response': result['text'],
                    'usage': result.get('meta', {}),
                    'success': True
                }
            else:
                return {
                    'service': 'cohere',
                    'error': f"API error: {response.status_code}",
                    'success': False
                }
                
        except Exception as e:
            return {
                'service': 'cohere',
                'error': str(e),
                'success': False
            }
    
    def get_huggingface_response(self, prompt: str, model: str) -> Dict[str, Any]:
        """Get response from Hugging Face"""
        try:
            headers = {
                'Authorization': f'Bearer {self.api_keys["huggingface"]}',
                'Content-Type': 'application/json'
            }
            
            data = {
                'inputs': prompt,
                'parameters': {
                    'max_length': self.cursor_config['settings']['max_tokens'],
                    'temperature': self.cursor_config['settings']['temperature']
                }
            }
            
            response = requests.post(
                f'https://api-inference.huggingface.co/models/{model}',
                headers=headers,
                json=data,
                timeout=self.cursor_config['settings']['timeout']
            )
            
            if response.status_code == 200:
                result = response.json()
                return {
                    'service': 'huggingface',
                    'model': model,
                    'response': result[0]['generated_text'],
                    'usage': {},
                    'success': True
                }
            else:
                return {
                    'service': 'huggingface',
                    'error': f"API error: {response.status_code}",
                    'success': False
                }
                
        except Exception as e:
            return {
                'service': 'huggingface',
                'error': str(e),
                'success': False
            }
    
    def get_service_status(self) -> Dict[str, Any]:
        """Get status of all AI services"""
        status = {
            'services': {},
            'total_services': len(self.ai_services),
            'connected_services': 0,
            'available_models': []
        }
        
        for service_name, service_config in self.ai_services.items():
            service_status = {
                'name': service_config['name'],
                'status': service_config['status'],
                'connection_status': self.connection_status.get(service_name, 'unknown'),
                'models': service_config['models'],
                'has_api_key': service_name in self.api_keys
            }
            
            status['services'][service_name] = service_status
            
            if service_status['connection_status'] == 'connected':
                status['connected_services'] += 1
                status['available_models'].extend(service_config['models'])
        
        return status
    
    def get_usage_statistics(self) -> Dict[str, Any]:
        """Get usage statistics"""
        # This would typically be stored in a database
        # For now, return mock data
        return {
            'total_requests': 0,
            'successful_requests': 0,
            'failed_requests': 0,
            'total_tokens': 0,
            'total_cost': 0.0,
            'requests_by_service': {},
            'requests_by_model': {},
            'daily_usage': [],
            'monthly_usage': []
        }
    
    def open_cursor_website(self):
        """Open Cursor website"""
        try:
            webbrowser.open('https://cursor.sh')
            return True
        except Exception as e:
            print(f"Failed to open Cursor website: {e}")
            return False
    
    def get_cursor_download_link(self) -> str:
        """Get Cursor download link"""
        return "https://cursor.sh/download"
    
    def install_cursor_extension(self):
        """Install Cursor extension"""
        try:
            # This would typically install a VS Code extension
            # For now, just return success
            print("Cursor extension installation simulated")
            return True
        except Exception as e:
            print(f"Failed to install Cursor extension: {e}")
            return False
    
    def shutdown(self):
        """Shutdown the Cursor AI integration"""
        try:
            self.initialized = False
            print("Cursor AI Integration System shutdown complete")
        except Exception as e:
            print(f"Error during shutdown: {e}")

def main():
    """Test the Cursor AI integration"""
    print("Testing Cursor AI Integration System...")
    
    cursor_ai = CursorAIConfig()
    
    if cursor_ai.initialize():
        print("✅ Cursor AI Integration initialized")
        
        # Get service status
        status = cursor_ai.get_service_status()
        print(f"Service status: {status['connected_services']}/{status['total_services']} connected")
        
        # Get usage statistics
        usage = cursor_ai.get_usage_statistics()
        print(f"Usage statistics: {usage['total_requests']} requests")
        
        # Test AI response (if API keys are available)
        if status['connected_services'] > 0:
            response = cursor_ai.get_ai_response("Hello, how are you?")
            if response.get('success'):
                print(f"AI response: {response['response'][:100]}...")
            else:
                print(f"AI response error: {response.get('error')}")
        else:
            print("No AI services connected - add API keys to test")
        
        # Shutdown
        cursor_ai.shutdown()
        print("✅ Cursor AI Integration test complete")
    else:
        print("❌ Failed to initialize Cursor AI Integration")

if __name__ == "__main__":
    main()
