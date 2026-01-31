/**
 * Scratch AI Extension
 * 
 * A custom Scratch extension that provides AI-powered assistance for programming.
 * Communicates with the local Scratch AI Bridge server to provide:
 * - AI-powered code suggestions
 * - Programming help and explanations
 * - Code compilation and execution
 * - Search functionality
 * 
 * Usage in Scratch:
 * 1. Load this extension in Scratch
 * 2. Use the "ask AI" blocks to get help
 * 3. Use the "search AI" blocks for information
 * 4. Use the "compile code" blocks to test code
 */

class ScratchAIExtension {
    constructor() {
        this.name = 'AI Assistant';
        this.color1 = '#4C97FF';
        this.color2 = '#4280D7';
        this.menuIconURI = null;
        this.blockIconURI = null;
        
        // Configuration
        this.bridgeUrl = 'http://localhost:8001';
        this.isConnected = false;
        this.lastResponse = '';
        this.lastError = '';
        
        // Initialize connection
        this.checkConnection();
    }
    
    /**
     * Check if the bridge server is available
     */
    async checkConnection() {
        try {
            const response = await fetch(`${this.bridgeUrl}/health`);
            if (response.ok) {
                this.isConnected = true;
                console.log('Connected to Scratch AI Bridge');
            } else {
                this.isConnected = false;
                console.log('Bridge server not responding');
            }
        } catch (error) {
            this.isConnected = false;
            console.log('Cannot connect to bridge server:', error.message);
        }
    }
    
    /**
     * Get the extension's blocks
     */
    getInfo() {
        return {
            id: 'scratchai',
            name: this.name,
            color1: this.color1,
            color2: this.color2,
            blocks: [
                {
                    opcode: 'askAI',
                    blockType: Scratch.BlockType.REPORTER,
                    text: 'ask AI [PROMPT]',
                    arguments: {
                        PROMPT: {
                            type: Scratch.ArgumentType.STRING,
                            defaultValue: 'How do I make a sprite move?'
                        }
                    }
                },
                {
                    opcode: 'searchAI',
                    blockType: Scratch.BlockType.REPORTER,
                    text: 'search AI for [QUERY]',
                    arguments: {
                        QUERY: {
                            type: Scratch.ArgumentType.STRING,
                            defaultValue: 'Scratch programming tips'
                        }
                    }
                },
                {
                    opcode: 'compileCode',
                    blockType: Scratch.BlockType.REPORTER,
                    text: 'compile [LANGUAGE] code [CODE] as [FILENAME]',
                    arguments: {
                        LANGUAGE: {
                            type: Scratch.ArgumentType.STRING,
                            menu: 'languages',
                            defaultValue: 'python'
                        },
                        CODE: {
                            type: Scratch.ArgumentType.STRING,
                            defaultValue: 'print("Hello, World!")'
                        },
                        FILENAME: {
                            type: Scratch.ArgumentType.STRING,
                            defaultValue: 'hello.py'
                        }
                    }
                },
                {
                    opcode: 'isConnected',
                    blockType: Scratch.BlockType.BOOLEAN,
                    text: 'AI bridge connected?'
                },
                {
                    opcode: 'getLastResponse',
                    blockType: Scratch.BlockType.REPORTER,
                    text: 'last AI response'
                },
                {
                    opcode: 'getLastError',
                    blockType: Scratch.BlockType.REPORTER,
                    text: 'last AI error'
                },
                {
                    opcode: 'checkConnection',
                    blockType: Scratch.BlockType.COMMAND,
                    text: 'check AI connection'
                },
                {
                    opcode: 'setBridgeUrl',
                    blockType: Scratch.BlockType.COMMAND,
                    text: 'set bridge URL to [URL]',
                    arguments: {
                        URL: {
                            type: Scratch.ArgumentType.STRING,
                            defaultValue: 'http://localhost:8001'
                        }
                    }
                }
            ],
            menus: {
                languages: {
                    acceptReporters: true,
                    items: [
                        {text: 'Python', value: 'python'},
                        {text: 'JavaScript', value: 'node'},
                        {text: 'Java', value: 'java'},
                        {text: 'C', value: 'c'},
                        {text: 'C++', value: 'cpp'}
                    ]
                }
            }
        };
    }
    
    /**
     * Ask AI a question
     */
    async askAI(args) {
        if (!this.isConnected) {
            this.lastError = 'Not connected to AI bridge';
            return 'Error: Not connected to AI bridge';
        }
        
        try {
            const response = await fetch(`${this.bridgeUrl}/ask`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    prompt: args.PROMPT
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                this.lastResponse = data.data;
                this.lastError = '';
                return data.data;
            } else {
                this.lastError = data.error || 'Unknown error';
                return `Error: ${this.lastError}`;
            }
        } catch (error) {
            this.lastError = error.message;
            return `Error: ${error.message}`;
        }
    }
    
    /**
     * Search AI for information
     */
    async searchAI(args) {
        if (!this.isConnected) {
            this.lastError = 'Not connected to AI bridge';
            return 'Error: Not connected to AI bridge';
        }
        
        try {
            const response = await fetch(`${this.bridgeUrl}/search`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    query: args.QUERY
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                this.lastResponse = data.data;
                this.lastError = '';
                return data.data;
            } else {
                this.lastError = data.error || 'Unknown error';
                return `Error: ${this.lastError}`;
            }
        } catch (error) {
            this.lastError = error.message;
            return `Error: ${error.message}`;
        }
    }
    
    /**
     * Compile code using the secure compilation service
     */
    async compileCode(args) {
        if (!this.isConnected) {
            this.lastError = 'Not connected to AI bridge';
            return 'Error: Not connected to AI bridge';
        }
        
        try {
            const response = await fetch(`${this.bridgeUrl}/compile`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify({
                    language: args.LANGUAGE,
                    filename: args.FILENAME,
                    source: args.CODE
                })
            });
            
            const data = await response.json();
            
            if (data.success) {
                // Parse the compilation result
                const compileResult = typeof data.data === 'string' ? JSON.parse(data.data) : data.data;
                
                if (compileResult.ok) {
                    this.lastResponse = `Compilation successful! Output: ${compileResult.stdout}`;
                    this.lastError = '';
                    return `Success: ${compileResult.stdout}`;
                } else {
                    this.lastError = `Compilation failed: ${compileResult.stderr}`;
                    return `Error: ${compileResult.stderr}`;
                }
            } else {
                this.lastError = data.error || 'Unknown error';
                return `Error: ${this.lastError}`;
            }
        } catch (error) {
            this.lastError = error.message;
            return `Error: ${error.message}`;
        }
    }
    
    /**
     * Check if connected to bridge
     */
    isConnected() {
        return this.isConnected;
    }
    
    /**
     * Get last AI response
     */
    getLastResponse() {
        return this.lastResponse || 'No response yet';
    }
    
    /**
     * Get last error
     */
    getLastError() {
        return this.lastError || 'No errors';
    }
    
    /**
     * Manually check connection
     */
    async checkConnection() {
        await this.checkConnection();
        return '';
    }
    
    /**
     * Set bridge URL
     */
    setBridgeUrl(args) {
        this.bridgeUrl = args.URL;
        this.checkConnection();
        return '';
    }
}

// Register the extension
Scratch.extensions.register(new ScratchAIExtension());

/**
 * Setup instructions for Scratch:
 * 
 * 1. Open Scratch (scratch.mit.edu or Scratch Desktop)
 * 2. Go to Extensions (bottom left corner)
 * 3. Click "Load an Extension"
 * 4. Choose "Load Experimental Extension"
 * 5. Copy and paste this JavaScript code
 * 6. The AI Assistant blocks will appear in the extensions section
 * 
 * Prerequisites:
 * - Scratch AI Bridge server must be running (ScratchAIBridge.java)
 * - AI clients (AIChatClient.java or ai_cli.php) must be available
 * - GEMINI_API_KEY environment variable must be set
 * - Secure compilation service (optional) should be running on port 4040
 * 
 * Example Scratch projects:
 * 
 * 1. AI Chatbot:
 *    - Use "ask AI" block with user input
 *    - Display response in a speech bubble
 * 
 * 2. Code Helper:
 *    - Use "search AI" to find programming solutions
 *    - Use "compile code" to test code snippets
 * 
 * 3. Learning Assistant:
 *    - Ask AI questions about programming concepts
 *    - Get step-by-step explanations
 */
