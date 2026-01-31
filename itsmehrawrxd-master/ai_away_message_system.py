#!/usr/bin/env python3
"""
AI Away Message System
Auto-response system for AI that can loop continuously
"""

import os
import sys
import json
import time
import threading
from datetime import datetime
import random

class AIAwayMessageSystem:
    """AI away message system with auto-response capabilities"""
    
    def __init__(self):
        self.away_active = False
        self.away_messages = []
        self.current_message_index = 0
        self.response_interval = 5  # seconds
        self.loop_mode = True
        self.auto_response_thread = None
        
        print("AI Away Message System initialized")
        self.load_away_messages()
    
    def load_away_messages(self):
        """Load away messages from file"""
        try:
            if os.path.exists('away_messages.json'):
                with open('away_messages.json', 'r') as f:
                    data = json.load(f)
                    self.away_messages = data.get('messages', [])
                    self.response_interval = data.get('interval', 5)
                    self.loop_mode = data.get('loop_mode', True)
            else:
                # Create default away messages
                self.away_messages = [
                    "I'm currently away, but I'll be back soon!",
                    "Auto-reply: Working on something important...",
                    "Away message: Please leave a message and I'll respond when I return!",
                    "🤖 AI is temporarily unavailable, but I'm still processing!",
                    "Auto-response: I'm in the middle of something, but I'm still here!",
                    "Away mode activated - responses will continue automatically!",
                    "I'm away but my AI is still running in the background!",
                    "Auto-reply: I'm not here right now, but my responses are!",
                    "Away message: I'm busy but my AI assistant is still working!",
                    "🤖 AI is away but still responding automatically!"
                ]
                self.save_away_messages()
        except Exception as e:
            print(f"Error loading away messages: {e}")
            self.away_messages = ["Default away message"]
    
    def save_away_messages(self):
        """Save away messages to file"""
        try:
            data = {
                'messages': self.away_messages,
                'interval': self.response_interval,
                'loop_mode': self.loop_mode,
                'last_updated': datetime.now().isoformat()
            }
            with open('away_messages.json', 'w') as f:
                json.dump(data, f, indent=2)
        except Exception as e:
            print(f"Error saving away messages: {e}")
    
    def add_away_message(self, message):
        """Add new away message"""
        self.away_messages.append(message)
        self.save_away_messages()
        print(f"Added away message: {message}")
    
    def remove_away_message(self, index):
        """Remove away message by index"""
        if 0 <= index < len(self.away_messages):
            removed = self.away_messages.pop(index)
            self.save_away_messages()
            print(f"Removed away message: {removed}")
            return True
        return False
    
    def start_away_mode(self):
        """Start away mode with auto-responses"""
        if self.away_active:
            print("Away mode is already active!")
            return {"success": False, "message": "Away mode already active"}
        
        if not self.away_messages:
            print("No away messages available!")
            return {"success": False, "message": "No away messages available"}
        
        self.away_active = True
        self.current_message_index = 0
        
        # Start auto-response thread
        self.auto_response_thread = threading.Thread(target=self.run_auto_responses)
        self.auto_response_thread.daemon = True
        self.auto_response_thread.start()
        
        print("Away mode activated! Auto-responses will start...")
        return {"success": True, "message": "Away mode activated"}
    
    def stop_away_mode(self):
        """Stop away mode"""
        if not self.away_active:
            print("Away mode is not active!")
            return {"success": False, "message": "Away mode not active"}
        
        self.away_active = False
        
        if self.auto_response_thread:
            self.auto_response_thread.join(timeout=1)
        
        print("Away mode deactivated!")
        return {"success": True, "message": "Away mode deactivated"}
    
    def run_auto_responses(self):
        """Run auto-response loop"""
        while self.away_active:
            try:
                # Get current message
                if self.loop_mode:
                    message = self.away_messages[self.current_message_index]
                    self.current_message_index = (self.current_message_index + 1) % len(self.away_messages)
                else:
                    message = random.choice(self.away_messages)
                
                # Send auto-response
                self.send_auto_response(message)
                
                # Wait for next response
                time.sleep(self.response_interval)
                
            except Exception as e:
                print(f"Auto-response error: {e}")
                time.sleep(self.response_interval)
    
    def send_auto_response(self, message):
        """Send auto-response message"""
        timestamp = datetime.now().strftime("%H:%M:%S")
        response = f"[{timestamp}] 🤖 AI Auto-Reply: {message}"
        
        # Log the response
        self.log_response(response)
        
        # In a real implementation, this would send to the AI chat system
        print(response)
        
        return response
    
    def log_response(self, response):
        """Log auto-response"""
        try:
            with open('ai_away_responses.log', 'a') as f:
                f.write(f"{response}\n")
        except Exception as e:
            print(f"Error logging response: {e}")
    
    def set_response_interval(self, interval):
        """Set response interval in seconds"""
        if interval < 1:
            interval = 1
        self.response_interval = interval
        self.save_away_messages()
        print(f"Response interval set to {interval} seconds")
    
    def set_loop_mode(self, loop_mode):
        """Set loop mode (True = sequential, False = random)"""
        self.loop_mode = loop_mode
        self.save_away_messages()
        mode_text = "sequential" if loop_mode else "random"
        print(f"Loop mode set to {mode_text}")
    
    def get_away_status(self):
        """Get away mode status"""
        return {
            'active': self.away_active,
            'message_count': len(self.away_messages),
            'current_index': self.current_message_index,
            'interval': self.response_interval,
            'loop_mode': self.loop_mode,
            'messages': self.away_messages
        }
    
    def create_away_message_presets(self):
        """Create preset away messages"""
        presets = {
            'busy': [
                "I'm currently busy with important work!",
                "Auto-reply: I'm in the middle of something critical!",
                "Away message: I'm working on something that requires my full attention!",
                "I'm busy but my AI is still running in the background!",
                "Auto-response: I'm focused on something important right now!"
            ],
            'meeting': [
                "I'm in a meeting right now, but I'll be back soon!",
                "Auto-reply: I'm currently in a meeting!",
                "Away message: I'm in a meeting, but I'm still processing!",
                "I'm in a meeting but my AI assistant is still working!",
                "Auto-response: I'm in a meeting, but I'm still here!"
            ],
            'coding': [
                "I'm deep in coding mode right now!",
                "Auto-reply: I'm in the zone coding!",
                "Away message: I'm coding something amazing!",
                "I'm coding but my AI is still running!",
                "Auto-response: I'm in coding mode, but I'm still responding!"
            ],
            'funny': [
                "I'm away but my AI is still here making dad jokes!",
                "Auto-reply: I'm not here, but my AI is still dad-joking!",
                "Away message: I'm away but my AI is still punning!",
                "I'm away but my AI is still here being punny!",
                "Auto-response: I'm away but my AI is still here being silly!"
            ],
            'professional': [
                "I'm currently unavailable, but I'll respond when I return!",
                "Auto-reply: I'm away from my desk, but I'll be back soon!",
                "Away message: I'm currently unavailable, but I'm still processing!",
                "I'm away but my AI assistant is still working!",
                "Auto-response: I'm unavailable right now, but I'm still here!"
            ]
        }
        
        return presets
    
    def load_preset(self, preset_name):
        """Load preset away messages"""
        presets = self.create_away_message_presets()
        
        if preset_name in presets:
            self.away_messages = presets[preset_name].copy()
            self.save_away_messages()
            print(f"Loaded preset: {preset_name}")
            return True
        else:
            print(f"Preset '{preset_name}' not found!")
            return False
    
    def get_available_presets(self):
        """Get list of available presets"""
        return list(self.create_away_message_presets().keys())
    
    def create_custom_away_message(self, template, variables):
        """Create custom away message with variables"""
        try:
            # Replace variables in template
            message = template
            for key, value in variables.items():
                message = message.replace(f"{{{key}}}", str(value))
            
            return message
        except Exception as e:
            print(f"Error creating custom message: {e}")
            return template
    
    def create_dynamic_away_messages(self):
        """Create dynamic away messages that change over time"""
        dynamic_messages = [
            f"I'm away but I've been responding for {int(time.time()) % 3600} seconds!",
            f"Auto-reply #{random.randint(1, 1000)}: I'm still here!",
            f"Away message: I've sent {len(self.away_messages)} responses so far!",
            f"I'm away but my AI is still running at {datetime.now().strftime('%H:%M:%S')}!",
            f"Auto-response: I'm away but I'm still processing at {datetime.now().strftime('%Y-%m-%d')}!"
        ]
        
        return dynamic_messages
    
    def start_dynamic_away_mode(self):
        """Start away mode with dynamic messages"""
        if self.away_active:
            print("Away mode is already active!")
            return {"success": False, "message": "Away mode already active"}
        
        # Add dynamic messages
        dynamic_messages = self.create_dynamic_away_messages()
        self.away_messages.extend(dynamic_messages)
        
        # Start away mode
        return self.start_away_mode()
    
    def create_away_message_generator(self):
        """Create away message generator"""
        generator_messages = [
            "I'm away but my AI is still here!",
            "Auto-reply: I'm not here right now!",
            "Away message: I'm busy but I'm still responding!",
            "I'm away but my AI assistant is still working!",
            "Auto-response: I'm away but I'm still here!"
        ]
        
        def generate_message():
            return random.choice(generator_messages)
        
        return generate_message
    
    def start_generator_mode(self):
        """Start generator mode"""
        if self.away_active:
            print("Away mode is already active!")
            return {"success": False, "message": "Away mode already active"}
        
        # Create generator
        generator = self.create_away_message_generator()
        
        # Start away mode with generator
        self.away_active = True
        
        def run_generator():
            while self.away_active:
                message = generator()
                self.send_auto_response(message)
                time.sleep(self.response_interval)
        
        self.auto_response_thread = threading.Thread(target=run_generator)
        self.auto_response_thread.daemon = True
        self.auto_response_thread.start()
        
        print("Generator mode activated!")
        return {"success": True, "message": "Generator mode activated"}

def main():
    """Test AI away message system"""
    print("Testing AI Away Message System...")
    
    away_system = AIAwayMessageSystem()
    
    # Test adding messages
    away_system.add_away_message("Test away message 1")
    away_system.add_away_message("Test away message 2")
    away_system.add_away_message("Test away message 3")
    
    # Test presets
    presets = away_system.get_available_presets()
    print(f"Available presets: {presets}")
    
    # Load busy preset
    away_system.load_preset('busy')
    
    # Test away mode
    result = away_system.start_away_mode()
    if result["success"]:
        print("✅ Away mode started")
        
        # Let it run for a bit
        time.sleep(10)
        
        # Stop away mode
        away_system.stop_away_mode()
        print("✅ Away mode stopped")
    else:
        print(f"❌ Failed to start away mode: {result['message']}")
    
    # Test status
    status = away_system.get_away_status()
    print(f"Away status: {status}")
    
    print("AI Away Message System test complete!")

if __name__ == "__main__":
    main()
