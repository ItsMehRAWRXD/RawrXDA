# AI Integration in EON Compiler IDE

## How AI Switches Between Chat and Coding Areas

The AI integration works through several sophisticated mechanisms that allow seamless interaction between the chat interface and coding areas:

### 1. **AI Function Calling System**

The AI can execute IDE functions through structured function calls:

```python
class AIFunctionAccess:
    def __init__(self, ide_instance):
        self.ide = ide_instance
        self.available_functions = {
            # Terminal Operations
            'execute_terminal_command': {
                'function': self._execute_terminal_command,
                'description': 'Execute a command in the terminal',
                'parameters': {
                    'command': 'Command to execute (string)',
                    'working_directory': 'Optional working directory (string)'
                }
            },
            
            # File Operations
            'create_file': {
                'function': self._create_file,
                'description': 'Create a new file with content',
                'parameters': {
                    'filepath': 'Path for new file (string)',
                    'content': 'File content (string)'
                }
            },
            
            # Code Analysis
            'analyze_code': {
                'function': self._analyze_code,
                'description': 'Analyze code for issues and suggestions',
                'parameters': {
                    'filepath': 'Path to file to analyze (string)'
                }
            }
        }
```

### 2. **AI Response Processing**

When the AI responds, it can include function calls in this format:

```
```function_call
{
    "function": "create_file",
    "parameters": {
        "filepath": "example.py",
        "content": "print('Hello World')"
    }
}
```
```

### 3. **Context Awareness**

The AI knows about:
- **Current active file** in the editor
- **Open tabs** and their content
- **Project structure** from file explorer
- **Terminal history** and current working directory
- **Debug state** and breakpoints

### 4. **Smart Switching Mechanisms**

#### A. **Code Attachment**
```python
def attach_code_to_chat(self):
    """Attach current editor code to chat"""
    if self.current_tab:
        editor = self.open_files[self.current_tab]['editor']
        code = editor.get(1.0, tk.END).strip()
        if code:
            code_block = f"```python\n{code}\n```"
            self.chat_input.insert(tk.END, code_block)
```

#### B. **AI Function Execution**
```python
def _process_ai_function_calls(self, response):
    """Process AI responses for function calls"""
    # Look for function_call blocks in AI response
    if "```function_call" in response:
        # Extract and execute function calls
        # Switch to appropriate IDE area
        # Update UI based on function results
```

#### C. **Context Switching**
```python
def switch_to_editor(self, filename):
    """Switch focus to editor tab"""
    if filename in self.open_files:
        # Switch to the tab
        # Update status bar
        # Focus the editor
        
def switch_to_terminal(self):
    """Switch focus to terminal"""
    # Focus terminal input
    # Show terminal output
    
def switch_to_debugger(self):
    """Switch to debug mode"""
    # Start debugger
    # Show debug panel
    # Set breakpoints
```

### 5. **AI Service Integration**

The AI can use different services:
- **ChatGPT**: General coding assistance
- **Claude**: Advanced reasoning
- **Kimi**: Chinese language support
- **GitHub Copilot**: Code completion
- **Free Copilot**: Learning from user patterns

### 6. **Real-time Updates**

The AI can:
- **Monitor file changes** and suggest improvements
- **Watch terminal output** and provide feedback
- **Analyze compilation errors** and suggest fixes
- **Track debugging progress** and help with issues

### 7. **Learning System**

The AI learns from:
- **User coding patterns**
- **Common errors and fixes**
- **Project structure preferences**
- **Debugging approaches**

## Example AI Workflow

1. **User**: "Create a Python function to calculate fibonacci"
2. **AI**: Analyzes request, creates function, switches to editor
3. **AI**: Writes code in new file, shows in editor
4. **AI**: Suggests testing the function
5. **AI**: Switches to terminal, runs test
6. **AI**: Shows results, suggests improvements

## Key Features

- **Seamless switching** between chat and coding areas
- **Context awareness** of current IDE state
- **Function calling** for direct IDE manipulation
- **Learning system** for personalized assistance
- **Multi-AI support** with different capabilities
- **Real-time collaboration** between user and AI

This creates a truly integrated AI coding experience where the AI becomes an active participant in the development process!
