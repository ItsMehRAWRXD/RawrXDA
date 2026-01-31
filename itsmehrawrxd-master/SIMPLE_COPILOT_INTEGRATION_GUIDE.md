# RawrZ Universal IDE - Copilot Integration Guide

## Overview
This guide explains how to integrate the Local AI Copilot system into your main IDE.

## Components
- local_ai_copilot_system.py: Main copilot system
- pull_model_dialog.py: Model management dialog
- integrate_docker_compilation.py: Docker integration
- simple_ide_copilot_patch.py: IDE integration patch
- simple_copilot_menu_integration.py: Menu integration

## Integration Steps

### 1. Add to Main IDE File
```python
# Add these imports to your main IDE file
import sys
from pathlib import Path
sys.path.append(str(Path(__file__).parent))

try:
    from local_ai_copilot_system import LocalAICopilotSystem
    from pull_model_dialog import PullModelDialog, ModelManager
    COPILOT_AVAILABLE = True
except ImportError as e:
    print(f"Copilot system not available: {e}")
    COPILOT_AVAILABLE = False
```

### 2. Initialize Copilot System
```python
class YourIDE:
    def __init__(self):
        # ... existing initialization ...
        
        # Initialize copilot
        if COPILOT_AVAILABLE:
            self.copilot = LocalAICopilotSystem(Path(__file__).parent)
            self.model_manager = ModelManager()
```

### 3. Add Menu Integration
```python
def create_menus(self):
    # ... existing menus ...
    
    # Add copilot menu
    if COPILOT_AVAILABLE:
        self.create_copilot_menu()
```

## Features

### AI Services
- Tabby: Real-time code completion
- Continue: Context-aware chat
- LocalAI: OpenAI-compatible local AI
- CodeT5: Code analysis and documentation
- Ollama: Local LLM chat

### Model Management
- Pull new models
- List available models
- Remove models
- Model configuration

### Docker Integration
- Automatic Docker container management
- Service health monitoring
- Port mapping and networking

## Usage

### Starting Services
1. Open the IDE
2. Go to "AI Copilot" menu
3. Select "AI Services" > "Start All Services"
4. Wait for services to start

### Using Code Completion
1. Start typing code
2. Pause for a moment
3. See AI suggestions appear
4. Accept or ignore suggestions

### Using AI Chat
1. Go to "AI Copilot" > "AI Features" > "AI Chat"
2. Type your question
3. Get AI response

### Pulling Models
1. Go to "AI Copilot" > "Model Management" > "Pull New Model"
2. Enter model name (e.g., "codellama:7b")
3. Click "Pull Model"
4. Wait for download to complete

## Troubleshooting

### Services Not Starting
- Check if Docker is running
- Verify port availability
- Check service logs

### Models Not Loading
- Verify model name is correct
- Check internet connection
- Check available disk space

### Performance Issues
- Reduce model size
- Close unused services
- Check system resources

## Configuration

### Service Settings
- Port configurations
- Memory limits
- Timeout settings

### Model Settings
- Model selection
- Performance tuning
- Cache settings

## Support

For issues or questions:
1. Check the logs in copilot/logs/
2. Verify Docker is running
3. Check service status
4. Report issues with details
