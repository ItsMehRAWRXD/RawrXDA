#!/usr/bin/env python3
"""
Integration script to add Enhanced Code Generator to your existing IDE
Run this to upgrade your 3,653-line EON Compiler GUI with real code generation
"""

import sys
import os
from pathlib import Path

# Add current directory to path so we can import our modules
sys.path.insert(0, str(Path(__file__).parent))

def integrate_with_existing_ide():
    """
    Integrate Enhanced Code Generator with your existing IDE files
    """
    
    print("🚀 INTEGRATING ENHANCED CODE GENERATOR")
    print("=====================================")
    
    # Check if main IDE files exist
    ide_files = [
        "eon-compiler-gui.py",
        "safe_hybrid_ide.py"  
    ]
    
    available_ides = []
    for ide_file in ide_files:
        if os.path.exists(ide_file):
            available_ides.append(ide_file)
            print(f"✅ Found: {ide_file}")
        else:
            print(f"❌ Missing: {ide_file}")
    
    if not available_ides:
        print("\n❌ No IDE files found. Please ensure you have:")
        print("   - eon-compiler-gui.py")  
        print("   - safe_hybrid_ide.py")
        return False
    
    # Import our enhanced code generator
    try:
        from enhanced_code_generator import RealCodeGenerator, ProjectTemplateGenerator, integrate_with_ide
        print("✅ Enhanced Code Generator imported successfully")
    except ImportError as e:
        print(f"❌ Failed to import Enhanced Code Generator: {e}")
        return False
    
    # Try to integrate with each available IDE
    for ide_file in available_ides:
        print(f"\n🔧 INTEGRATING WITH {ide_file.upper()}")
        print("-" * 50)
        
        try:
            # Create integration patch
            create_integration_patch(ide_file)
            print(f"✅ Integration patch created for {ide_file}")
            
        except Exception as e:
            print(f"❌ Integration failed for {ide_file}: {e}")
    
    # Create launcher script
    create_enhanced_launcher()
    
    print("\n🎉 INTEGRATION COMPLETE!")
    print("=======================")
    print("\n📝 Next Steps:")
    print("1. Run: python enhanced_ide_launcher.py")
    print("2. Use 'Tools → Generate Code from Description'")
    print("3. Use 'File → New Project from Template'")
    print("4. Enjoy AI-powered code generation! 🚀")
    
    return True

def create_integration_patch(ide_file):
    """Create integration patch for existing IDE file"""
    
    # Read the existing IDE file
    with open(ide_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Check if already integrated
    if "enhanced_code_generator" in content:
        print(f"✅ {ide_file} already integrated")
        return
    
    # Create backup
    backup_file = f"{ide_file}.backup"
    with open(backup_file, 'w', encoding='utf-8') as f:
        f.write(content)
    print(f"✅ Backup created: {backup_file}")
    
    # Add integration code
    integration_code = '''
# ==================== ENHANCED CODE GENERATOR INTEGRATION ====================

def integrate_code_generator(self):
    """Integrate Enhanced Code Generator with this IDE"""
    try:
        from enhanced_code_generator import integrate_with_ide
        self.code_generator, self.project_generator = integrate_with_ide(self)
        
        # Add menu items if menu system exists
        self.add_code_generator_menus()
        
        print("✅ Enhanced Code Generator integrated")
        return True
    except Exception as e:
        print(f"❌ Code Generator integration failed: {e}")
        return False

def add_code_generator_menus(self):
    """Add code generator menu items"""
    try:
        # Check if we have a menu system
        if hasattr(self, 'menubar'):
            # Try to add to Tools menu
            if hasattr(self, 'tools_menu') or 'tools_menu' in dir(self):
                self.add_tools_menu_items()
            
            # Try to add to File menu  
            if hasattr(self, 'file_menu') or 'file_menu' in dir(self):
                self.add_file_menu_items()
                
        print("✅ Code generator menus added")
    except Exception as e:
        print(f"⚠️  Menu integration partially failed: {e}")

def add_tools_menu_items(self):
    """Add items to Tools menu"""
    if hasattr(self, 'tools_menu'):
        self.tools_menu.add_separator()
        self.tools_menu.add_command(label="Generate Code from Description", 
                                   command=self.show_code_generator)
        self.tools_menu.add_command(label="Code Templates", 
                                   command=self.show_code_templates)

def add_file_menu_items(self):
    """Add items to File menu"""
    if hasattr(self, 'file_menu'):
        self.file_menu.add_separator()
        self.file_menu.add_command(label="New Project from Template", 
                                  command=self.show_project_generator)

def show_code_generator(self):
    """Show code generator dialog"""
    if hasattr(self, 'code_generator'):
        from enhanced_code_generator import show_code_generator_dialog
        show_code_generator_dialog(self)
    else:
        print("Code generator not available")

def show_project_generator(self):
    """Show project generator dialog"""
    if hasattr(self, 'project_generator'):
        from enhanced_code_generator import show_project_generator_dialog
        show_project_generator_dialog(self)
    else:
        print("Project generator not available")

def show_code_templates(self):
    """Show available code templates"""
    if hasattr(self, 'code_generator'):
        templates = self.code_generator.templates
        template_info = "\\n".join([f"• {k}: {len(v)} languages" for k, v in templates.items()])
        
        if hasattr(self, 'show_info_dialog'):
            self.show_info_dialog("Code Templates", f"Available templates:\\n\\n{template_info}")
        else:
            print(f"Available templates:\\n{template_info}")
    else:
        print("Code generator not available")

# ==================== END ENHANCED CODE GENERATOR INTEGRATION ====================
'''
    
    # Find a good place to insert (after imports, before main class)
    lines = content.split('\n')
    insert_line = -1
    
    # Look for main class definition
    for i, line in enumerate(lines):
        if line.strip().startswith('class ') and ('IDE' in line or 'App' in line or 'GUI' in line):
            insert_line = i
            break
    
    if insert_line == -1:
        # If no main class found, insert after imports
        for i, line in enumerate(lines):
            if not line.strip() or line.startswith('#') or line.startswith('import') or line.startswith('from'):
                continue
            else:
                insert_line = i
                break
    
    if insert_line == -1:
        insert_line = len(lines) // 2  # Fallback: middle of file
    
    # Insert integration code
    lines.insert(insert_line, integration_code)
    
    # Write back to file
    with open(ide_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
    
    print(f"✅ Integration code added to {ide_file} at line {insert_line}")

def create_enhanced_launcher():
    """Create launcher script for enhanced IDE"""
    
    launcher_code = '''#!/usr/bin/env python3
"""
Enhanced IDE Launcher
Launches your existing IDE with Enhanced Code Generator integration
"""

import sys
import os
from pathlib import Path

def main():
    print("🚀 LAUNCHING ENHANCED IDE")
    print("========================")
    
    # Check available IDE files
    ide_options = []
    
    if os.path.exists("eon-compiler-gui.py"):
        ide_options.append(("EON Compiler GUI", "eon-compiler-gui.py"))
        
    if os.path.exists("safe_hybrid_ide.py"):
        ide_options.append(("Safe Hybrid IDE", "safe_hybrid_ide.py"))
    
    if not ide_options:
        print("❌ No IDE files found!")
        print("Please ensure you have:")
        print("   - eon-compiler-gui.py")
        print("   - safe_hybrid_ide.py")
        return 1
    
    # If multiple options, let user choose
    if len(ide_options) == 1:
        selected_ide = ide_options[0]
    else:
        print("\\nAvailable IDEs:")
        for i, (name, file) in enumerate(ide_options, 1):
            print(f"  {i}. {name} ({file})")
        
        while True:
            try:
                choice = input("\\nSelect IDE (1-{}): ".format(len(ide_options)))
                idx = int(choice) - 1
                if 0 <= idx < len(ide_options):
                    selected_ide = ide_options[idx]
                    break
                else:
                    print("Invalid choice. Try again.")
            except (ValueError, KeyboardInterrupt):
                print("\\nCancelled.")
                return 1
    
    ide_name, ide_file = selected_ide
    
    print(f"\\n🚀 Starting {ide_name}...")
    print(f"📁 File: {ide_file}")
    print("✨ Enhanced with Code Generator!")
    
    # Import and run the selected IDE
    try:
        # Add current directory to Python path
        sys.path.insert(0, str(Path.cwd()))
        
        # Import the IDE module
        module_name = ide_file.replace('.py', '')
        ide_module = __import__(module_name)
        
        # Try to run the main function or start the GUI
        if hasattr(ide_module, 'main'):
            ide_module.main()
        elif hasattr(ide_module, 'run'):
            ide_module.run()
        else:
            print(f"✅ {ide_name} module loaded successfully")
            print("Note: No main() or run() function found in module")
            print("You may need to run the IDE manually or check the integration")
            
    except ImportError as e:
        print(f"❌ Failed to import {ide_file}: {e}")
        print("\\nTrying direct execution...")
        os.system(f"python {ide_file}")
        
    except Exception as e:
        print(f"❌ Error running {ide_name}: {e}")
        print("\\nTrying direct execution...")
        os.system(f"python {ide_file}")

if __name__ == "__main__":
    exit(main())
'''
    
    with open("enhanced_ide_launcher.py", 'w', encoding='utf-8') as f:
        f.write(launcher_code)
    
    print("✅ Enhanced IDE launcher created: enhanced_ide_launcher.py")

def test_integration():
    """Test the integration"""
    
    print("\n🧪 TESTING INTEGRATION")
    print("=====================")
    
    try:
        from enhanced_code_generator import RealCodeGenerator
        
        # Create mock IDE
        class MockIDE:
            def __init__(self):
                self.ai_system = None
                
        mock_ide = MockIDE()
        code_gen = RealCodeGenerator(mock_ide)
        
        # Test code generation
        test_code = code_gen.generate_from_description("Create a hello world program", "python")
        
        if test_code and len(test_code) > 50:
            print("✅ Code generation test passed")
            print(f"   Generated {len(test_code)} characters")
        else:
            print("⚠️  Code generation test warning: Short output")
            
        print("✅ Integration test completed")
        return True
        
    except Exception as e:
        print(f"❌ Integration test failed: {e}")
        return False

if __name__ == "__main__":
    print("🚀 ENHANCED IDE INTEGRATION TOOL")
    print("=================================")
    
    success = integrate_with_existing_ide()
    
    if success:
        test_integration()
        
        print("\\n🎉 ALL DONE!")
        print("===========")
        print("\\n🚀 Run your enhanced IDE with:")
        print("   python enhanced_ide_launcher.py")
        print("\\n📚 Or run individual IDEs:")
        for ide_file in ["eon-compiler-gui.py", "safe_hybrid_ide.py"]:
            if os.path.exists(ide_file):
                print(f"   python {ide_file}")
    else:
        print("\\n❌ Integration failed. Check the errors above.")
        sys.exit(1)
