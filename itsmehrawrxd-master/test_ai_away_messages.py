#!/usr/bin/env python3
"""
Test AI Away Message System
Demonstrates the auto-response system
"""

import sys
import os
import time
from ai_away_message_system import AIAwayMessageSystem

def test_away_message_system():
    """Test the AI away message system"""
    print("Testing AI Away Message System...")
    print("="*50)
    
    # Initialize system
    away_system = AIAwayMessageSystem()
    
    # Test 1: Add custom messages
    print("\n1. Adding custom away messages...")
    away_system.add_away_message("I'm away but my AI is still here!")
    away_system.add_away_message("Auto-reply: I'm not here right now!")
    away_system.add_away_message("Away message: I'm busy but I'm still responding!")
    
    # Test 2: Load presets
    print("\n2. Testing presets...")
    presets = away_system.get_available_presets()
    print(f"Available presets: {presets}")
    
    # Load busy preset
    away_system.load_preset('busy')
    print("Loaded 'busy' preset")
    
    # Test 3: Show status
    print("\n3. Away message status:")
    status = away_system.get_away_status()
    print(f"  Active: {status['active']}")
    print(f"  Message count: {status['message_count']}")
    print(f"  Interval: {status['interval']} seconds")
    print(f"  Loop mode: {status['loop_mode']}")
    
    # Test 4: Start away mode
    print("\n4. Starting away mode...")
    result = away_system.start_away_mode()
    if result["success"]:
        print("✅ Away mode started successfully!")
        print("Auto-responses will begin...")
        
        # Let it run for a bit
        print("\n5. Running auto-responses for 15 seconds...")
        for i in range(15):
            time.sleep(1)
            print(f"  {i+1}/15 seconds...")
        
        # Stop away mode
        print("\n6. Stopping away mode...")
        away_system.stop_away_mode()
        print("✅ Away mode stopped!")
    else:
        print(f"❌ Failed to start away mode: {result['message']}")
    
    # Test 5: Test generator mode
    print("\n7. Testing generator mode...")
    result = away_system.start_generator_mode()
    if result["success"]:
        print("✅ Generator mode started!")
        print("Random responses will begin...")
        
        # Let it run for a bit
        print("\n8. Running generator mode for 10 seconds...")
        for i in range(10):
            time.sleep(1)
            print(f"  {i+1}/10 seconds...")
        
        # Stop generator mode
        print("\n9. Stopping generator mode...")
        away_system.stop_away_mode()
        print("✅ Generator mode stopped!")
    else:
        print(f"❌ Failed to start generator mode: {result['message']}")
    
    # Test 6: Test settings
    print("\n10. Testing settings...")
    away_system.set_response_interval(3)
    away_system.set_loop_mode(False)
    print("✅ Settings updated!")
    
    # Test 7: Show final status
    print("\n11. Final status:")
    status = away_system.get_away_status()
    print(f"  Active: {status['active']}")
    print(f"  Message count: {status['message_count']}")
    print(f"  Interval: {status['interval']} seconds")
    print(f"  Loop mode: {status['loop_mode']}")
    
    print("\n" + "="*50)
    print("AI Away Message System test complete!")
    print("Check 'ai_away_responses.log' for response history")

def test_preset_messages():
    """Test preset message creation"""
    print("\nTesting preset message creation...")
    
    away_system = AIAwayMessageSystem()
    
    # Test all presets
    presets = away_system.get_available_presets()
    
    for preset in presets:
        print(f"\nTesting preset: {preset}")
        away_system.load_preset(preset)
        status = away_system.get_away_status()
        print(f"  Messages loaded: {status['message_count']}")
        print(f"  Sample messages:")
        for i, message in enumerate(status['messages'][:3]):  # Show first 3
            print(f"    {i+1}. {message}")

def test_dynamic_messages():
    """Test dynamic message generation"""
    print("\nTesting dynamic message generation...")
    
    away_system = AIAwayMessageSystem()
    
    # Test dynamic messages
    dynamic_messages = away_system.create_dynamic_away_messages()
    print(f"Generated {len(dynamic_messages)} dynamic messages:")
    for i, message in enumerate(dynamic_messages):
        print(f"  {i+1}. {message}")
    
    # Test custom message creation
    template = "I'm away but I've been responding for {seconds} seconds!"
    variables = {"seconds": 123}
    custom_message = away_system.create_custom_away_message(template, variables)
    print(f"\nCustom message: {custom_message}")

def main():
    """Main test function"""
    print("AI Away Message System Test Suite")
    print("="*50)
    
    try:
        # Test basic functionality
        test_away_message_system()
        
        # Test presets
        test_preset_messages()
        
        # Test dynamic messages
        test_dynamic_messages()
        
        print("\n🎉 All tests completed successfully!")
        
    except Exception as e:
        print(f"\n❌ Test failed with error: {e}")
        import traceback
        traceback.print_exc()

if __name__ == "__main__":
    main()
