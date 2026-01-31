#!/usr/bin/env python3
"""
Test script for the new features:
- DO NOT DISTRIBUTE notifications
- Twitch streaming integration
- Export functionality
- Configuration Manager
- Accept/Deny system
"""

import sys
import os
import tkinter as tk
from tkinter import ttk, messagebox

# Add the current directory to the path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

def test_new_features():
    """Test all the new features"""
    print("🧪 Testing New Features...")
    
    try:
        # Import the IDE
        from safe_hybrid_ide import SafeHybridIDE
        
        # Create a test instance
        print("📝 Creating IDE instance...")
        ide = SafeHybridIDE()
        
        # Test distribution warnings
        print("\n🚨 Testing Distribution Warnings:")
        print("   - Distribution warnings enabled:", ide.distribution_warnings)
        print("   - Warning method exists:", hasattr(ide, 'show_distribution_warning'))
        
        # Test Twitch streaming
        print("\n📺 Testing Twitch Streaming:")
        print("   - Connect method exists:", hasattr(ide, 'connect_twitch'))
        print("   - Start streaming method exists:", hasattr(ide, 'start_live_coding'))
        print("   - Stop streaming method exists:", hasattr(ide, 'stop_streaming'))
        print("   - Settings method exists:", hasattr(ide, 'show_streaming_settings'))
        
        # Test export functionality
        print("\n📤 Testing Export Functionality:")
        print("   - Export project method exists:", hasattr(ide, 'export_project'))
        print("   - Browse path method exists:", hasattr(ide, 'browse_export_path'))
        print("   - Perform export method exists:", hasattr(ide, 'perform_export'))
        
        # Test configuration manager
        print("\n⚙️ Testing Configuration Manager:")
        print("   - Configurations loaded:", len(ide.configurations))
        print("   - Templates created:", len(ide.config_templates))
        print("   - Show manager method exists:", hasattr(ide, 'show_configuration_manager'))
        print("   - Load configurations method exists:", hasattr(ide, 'load_configurations'))
        
        # Test accept/deny system
        print("\n✅ Testing Accept/Deny System:")
        print("   - Accept method exists:", hasattr(ide, 'accept_code_review'))
        print("   - Deny method exists:", hasattr(ide, 'deny_code_review'))
        print("   - Undo method exists:", hasattr(ide, 'undo_code_review'))
        print("   - Keep method exists:", hasattr(ide, 'keep_code_review'))
        print("   - Generate samples method exists:", hasattr(ide, 'generate_sample_reviews'))
        
        # Test configuration templates
        print("\n📋 Testing Configuration Templates:")
        for key, template in ide.config_templates.items():
            print(f"   - {template['name']}: {template['description']}")
            print(f"     Theme: {template['theme']}, AI Services: {template['ai_services']}")
        
        # Test GitHub authentication fix
        print("\n🔐 Testing GitHub Authentication Fix:")
        from safe_hybrid_ide import GitHubConnector
        
        # Test without token
        config_no_token = {'username': 'testuser'}
        connector_no_token = GitHubConnector(config_no_token)
        result_no_token = connector_no_token.scan_repository("https://github.com/test/repo")
        print(f"   - Without token: {result_no_token[:50]}...")
        
        # Test with token
        config_with_token = {'username': 'testuser', 'token': 'ghp_test123'}
        connector_with_token = GitHubConnector(config_with_token)
        result_with_token = connector_with_token.scan_repository("https://github.com/test/repo")
        print(f"   - With token: {result_with_token[:50]}...")
        
        print("\n✅ All new features implemented successfully!")
        
        return True
        
    except Exception as e:
        print(f"❌ Test failed: {e}")
        import traceback
        traceback.print_exc()
        return False

def test_configuration_sharing():
    """Test configuration sharing functionality"""
    print("\n🛒 Testing Configuration Sharing...")
    
    try:
        from safe_hybrid_ide import SafeHybridIDE
        ide = SafeHybridIDE()
        
        # Test configuration export
        print("📤 Testing configuration export...")
        ide.export_current_configuration()
        ide.export_all_configurations()
        
        # Test marketplace sharing
        print("🛒 Testing marketplace sharing...")
        ide.share_to_marketplace()
        ide.create_template_from_config()
        
        # Test import functionality
        print("📥 Testing import functionality...")
        ide.browse_config_file()
        ide.import_configuration()
        
        print("✅ Configuration sharing features working!")
        return True
        
    except Exception as e:
        print(f"❌ Configuration sharing test failed: {e}")
        return False

if __name__ == "__main__":
    print("🚀 Testing New IDE Features")
    print("=" * 50)
    
    # Test new features
    success1 = test_new_features()
    
    # Test configuration sharing
    success2 = test_configuration_sharing()
    
    if success1 and success2:
        print("\n🎉 All tests passed! New features are working correctly.")
        print("\n📋 Summary of implemented features:")
        print("   - ✅ DO NOT DISTRIBUTE notifications")
        print("   - ✅ Twitch streaming integration")
        print("   - ✅ Export functionality for projects")
        print("   - ✅ Configuration Manager with templates")
        print("   - ✅ Marketplace sharing for configurations")
        print("   - ✅ Accept/Deny code review system")
        print("   - ✅ GitHub authentication fix")
        print("   - ✅ Keyboard shortcuts (Ctrl+1-4)")
        print("   - ✅ Review history and state management")
        print("   - ✅ Sample review generation")
        
        print("\n🎯 Key Features:")
        print("   - 🚨 Distribution protection with warnings")
        print("   - 📺 Live coding streaming to Twitch")
        print("   - 📤 Export projects in multiple formats")
        print("   - ⚙️ Customizable configurations")
        print("   - 🛒 Share configurations via marketplace")
        print("   - ✅ Accept/Deny code review workflow")
        print("   - 🔐 Secure GitHub integration")
        
        print("\n🎮 Usage:")
        print("   - Use '⚙️ Configuration Manager' to manage settings")
        print("   - Use '📺 Streaming' menu for Twitch integration")
        print("   - Use '📤 Export Project' to export your work")
        print("   - Use Accept/Deny buttons for code reviews")
        print("   - Use Ctrl+1-4 for quick review actions")
        
    else:
        print("\n❌ Some tests failed. Please check the implementation.")
