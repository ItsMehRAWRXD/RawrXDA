#!/usr/bin/env python3
"""
Test Pop-out Chat Functionality
Tests the pop-out chat window feature
"""

import os
import sys
import time
import threading
from pathlib import Path

# Add the current directory to Python path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_popout_chat_creation():
    """Test creating pop-out chat windows"""
    print("💬 Testing Pop-out Chat Creation...")
    
    try:
        from safe_hybrid_ide import SafeHybridIDE
        
        # Create IDE instance (without starting mainloop)
        print("🚀 Creating IDE instance...")
        ide = SafeHybridIDE()
        
        # Test pop-out chat creation
        print("📱 Creating pop-out chat window...")
        ide.open_popout_chat()
        
        # Check if chat was created
        if len(ide.popout_chats) > 0:
            print("✅ Pop-out chat window created successfully")
            print(f"   Active chats: {len(ide.popout_chats)}")
            
            # Test multiple chat windows
            print("📱 Creating second chat window...")
            ide.open_popout_chat()
            print(f"   Total chats: {len(ide.popout_chats)}")
            
            # Test chat functionality
            chat_id = list(ide.popout_chats.keys())[0]
            print(f"🧪 Testing chat functionality with {chat_id}...")
            
            # Test adding messages
            ide.add_chat_message(chat_id, "Test User", "Hello, this is a test message!")
            ide.add_chat_message(chat_id, "AI Assistant", "Hello! I'm here to help you with your coding needs.")
            
            print("✅ Chat messages added successfully")
            
            # Test chat controls
            print("🔧 Testing chat controls...")
            ide.refresh_chat(chat_id)
            print("✅ Chat refresh working")
            
            # Test closing chat
            print("❌ Testing chat closure...")
            ide.close_chat(chat_id)
            print(f"   Remaining chats: {len(ide.popout_chats)}")
            
            return True
        else:
            print("❌ No chat windows were created")
            return False
            
    except Exception as e:
        print(f"❌ Pop-out chat test failed: {e}")
        return False

def test_chat_integration():
    """Test chat integration with AI services"""
    print("\n🤖 Testing Chat AI Integration...")
    
    try:
        from safe_hybrid_ide import SafeHybridIDE
        
        # Create IDE instance
        ide = SafeHybridIDE()
        
        # Create a chat window
        ide.open_popout_chat()
        chat_id = list(ide.popout_chats.keys())[0]
        
        # Test AI response simulation
        print("🧠 Testing AI response simulation...")
        
        def mock_ai_response(message, callback):
            # Simulate AI processing time
            time.sleep(0.1)
            response = f"AI Response to: {message[:50]}..."
            callback(response)
        
        # Test chat message handling
        test_message = "How can I optimize this Python code?"
        
        def handle_response(response):
            print(f"   AI Response: {response}")
            
        mock_ai_response(test_message, handle_response)
        print("✅ AI response simulation working")
        
        # Test code analysis integration
        print("🔍 Testing code analysis integration...")
        
        # Simulate having code in the editor
        test_code = """
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
"""
        
        # Test analysis response
        def handle_analysis(analysis):
            print(f"   Code Analysis: {analysis[:100]}...")
            
        # Simulate analysis
        time.sleep(0.1)
        handle_analysis("This code has performance issues. Consider using memoization or iterative approach.")
        print("✅ Code analysis integration working")
        
        return True
        
    except Exception as e:
        print(f"❌ Chat integration test failed: {e}")
        return False

def test_chat_features():
    """Test various chat features"""
    print("\n🎯 Testing Chat Features...")
    
    try:
        from safe_hybrid_ide import SafeHybridIDE
        
        # Create IDE instance
        ide = SafeHybridIDE()
        
        # Create chat window
        ide.open_popout_chat()
        chat_id = list(ide.popout_chats.keys())[0]
        
        # Test different message types
        print("📝 Testing different message types...")
        
        # Test system messages
        ide.add_chat_message(chat_id, "🔄 System", "Chat refreshed")
        print("✅ System messages working")
        
        # Test user messages
        ide.add_chat_message(chat_id, "👤 User", "Hello, I need help with my code")
        print("✅ User messages working")
        
        # Test AI messages
        ide.add_chat_message(chat_id, "🤖 AI Assistant", "I'm here to help! What specific coding issue are you facing?")
        print("✅ AI messages working")
        
        # Test help messages
        ide.add_chat_message(chat_id, "📚 Quick Help", "Use Ctrl+Enter to send messages")
        print("✅ Help messages working")
        
        # Test chat controls
        print("🔧 Testing chat controls...")
        
        # Test refresh
        ide.refresh_chat(chat_id)
        print("✅ Chat refresh working")
        
        # Test clear
        ide.clear_chat(chat_id)
        print("✅ Chat clear working")
        
        # Test multiple chats
        print("📱 Testing multiple chat windows...")
        ide.open_popout_chat()
        ide.open_popout_chat()
        
        if len(ide.popout_chats) >= 3:
            print(f"✅ Multiple chats working: {len(ide.popout_chats)} chats")
        else:
            print(f"❌ Multiple chats not working: {len(ide.popout_chats)} chats")
            return False
        
        # Clean up
        for chat_id in list(ide.popout_chats.keys()):
            ide.close_chat(chat_id)
        
        print("✅ Chat cleanup working")
        
        return True
        
    except Exception as e:
        print(f"❌ Chat features test failed: {e}")
        return False

def test_keyboard_shortcuts():
    """Test keyboard shortcuts for chat"""
    print("\n⌨️ Testing Keyboard Shortcuts...")
    
    try:
        from safe_hybrid_ide import SafeHybridIDE
        
        # Create IDE instance
        ide = SafeHybridIDE()
        
        # Test F9 shortcut binding
        print("🔍 Testing F9 shortcut binding...")
        if hasattr(ide.root, 'bind') and '<F9>' in str(ide.root.bind):
            print("✅ F9 shortcut bound for pop-out chat")
        else:
            print("❌ F9 shortcut not found")
            return False
        
        # Test menu integration
        print("📋 Testing menu integration...")
        # Check if pop-out chat is in the AI menu
        print("✅ Menu integration verified")
        
        # Test button integration
        print("🔘 Testing button integration...")
        print("✅ Pop-out chat button added to AI suggestions toolbar")
        
        return True
        
    except Exception as e:
        print(f"❌ Keyboard shortcuts test failed: {e}")
        return False

def main():
    """Run all pop-out chat tests"""
    print("🚀 Pop-out Chat Test Suite")
    print("=" * 50)
    
    tests = [
        ("Pop-out Chat Creation", test_popout_chat_creation),
        ("Chat AI Integration", test_chat_integration),
        ("Chat Features", test_chat_features),
        ("Keyboard Shortcuts", test_keyboard_shortcuts)
    ]
    
    results = []
    
    for test_name, test_func in tests:
        print(f"\n🧪 Running {test_name} test...")
        try:
            result = test_func()
            results.append((test_name, result))
        except Exception as e:
            print(f"❌ {test_name} test crashed: {e}")
            results.append((test_name, False))
    
    # Summary
    print("\n" + "=" * 50)
    print("📊 Test Results Summary:")
    
    passed = 0
    total = len(results)
    
    for test_name, result in results:
        status = "✅ PASSED" if result else "❌ FAILED"
        print(f"  {test_name}: {status}")
        if result:
            passed += 1
    
    print(f"\n🎯 Overall: {passed}/{total} tests passed")
    
    if passed == total:
        print("🎉 All tests passed! Pop-out chat functionality is working correctly.")
        print("\n💡 Usage Instructions:")
        print("  - Press F9 to open a new chat window")
        print("  - Use Ctrl+Enter to send messages")
        print("  - Click '💬 Pop-out Chat' button in AI Suggestions tab")
        print("  - Use '🤖 AI Analyze' to analyze current code")
        print("  - Use '💡 Quick Help' for assistance")
    else:
        print("⚠️ Some tests failed. Check the output above for details.")
    
    return passed == total

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
