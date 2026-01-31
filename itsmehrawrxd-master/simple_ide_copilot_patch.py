
# RawrZ Universal IDE - Simple Copilot Integration
# Add this to your main IDE file

import sys
from pathlib import Path

# Add copilot system to path
sys.path.append(str(Path(__file__).parent))

# Import copilot components
try:
    from local_ai_copilot_system import LocalAICopilotSystem
    from pull_model_dialog import PullModelDialog, ModelManager
    COPILOT_AVAILABLE = True
except ImportError as e:
    print(f"Copilot system not available: {e}")
    COPILOT_AVAILABLE = False

class IDEWithCopilot:
    def __init__(self):
        self.copilot = None
        self.model_manager = None
        
        if COPILOT_AVAILABLE:
            self.initialize_copilot()
    
    def initialize_copilot(self):
        """Initialize copilot system"""
        try:
            ide_root = Path(__file__).parent
            self.copilot = LocalAICopilotSystem(ide_root)
            self.model_manager = ModelManager()
            print("Copilot system initialized")
        except Exception as e:
            print(f"Failed to initialize copilot: {e}")
    
    def show_copilot_gui(self):
        """Show copilot GUI"""
        if self.copilot:
            return self.copilot.create_copilot_gui(self.root)
        else:
            messagebox.showwarning("Copilot Not Available", 
                                 "Copilot system is not available")
    
    def show_pull_model_dialog(self):
        """Show pull model dialog"""
        if self.model_manager:
            dialog = PullModelDialog(self.root, self.model_manager)
            return dialog.show_dialog()
        else:
            messagebox.showwarning("Model Manager Not Available", 
                                 "Model manager is not available")
    
    def get_code_completion(self, code, language, cursor_position):
        """Get code completion"""
        if self.copilot:
            return self.copilot.get_tabby_completion(code, language, cursor_position)
        return None
    
    def get_context_chat(self, message, context_files=None):
        """Get context-aware chat"""
        if self.copilot:
            return self.copilot.get_continue_chat(message, context_files)
        return None
    
    def get_code_analysis(self, code, analysis_type="explain"):
        """Get code analysis"""
        if self.copilot:
            return self.copilot.get_codet5_analysis(code, analysis_type)
        return None
    
    def get_ai_chat(self, message, model="codellama"):
        """Get AI chat"""
        if self.copilot:
            return self.copilot.get_ollama_chat(message, model)
        return None
