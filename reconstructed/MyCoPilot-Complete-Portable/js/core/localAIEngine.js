class LocalAIEngine {
    constructor() {
        this.models = {
            'codellama': { loaded: false, path: null },
            'mistral': { loaded: false, path: null },
            'phi': { loaded: false, path: null }
        };
    }

    async loadModel(modelName) {
        try {
            const response = await fetch('http://localhost:8080/api/ollama/status');
            const data = await response.json();
            const available = data.models?.find(m => m.name.includes(modelName));
            if (available) {
                this.models[modelName].loaded = true;
                return { success: true, model: modelName };
            }
            return { success: false, error: 'Model not found. Install with: ollama pull ' + modelName };
        } catch (error) {
            return { success: false, error: 'Ollama not running. Start with: ollama serve' };
        }
    }

    async generate(prompt, model = 'codellama:7b') {
        console.log('[LocalAI] Generating with model:', model);
        console.log('[LocalAI] Prompt:', prompt.substring(0, 100) + '...');
        try {
            console.log('[LocalAI] Fetching backend proxy /api/ollama');
            const response = await fetch('http://localhost:8080/api/ollama', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ model, prompt, stream: false })
            });
            console.log('[LocalAI] Response status:', response.status);
            if (!response.ok) {
                const errorText = await response.text();
                console.error('[LocalAI] Error response:', errorText);
                return { success: false, error: `HTTP ${response.status}: ${errorText}` };
            }
            const data = await response.json();
            console.log('[LocalAI] Success! Response length:', data.response?.length || 0);
            return { success: true, response: data.response };
        } catch (error) {
            console.error('[LocalAI] Exception:', error);
            return { success: false, error: error.message };
        }
    }

    async chat(messages, model = 'codellama:7b') {
        console.log('[LocalAI] Chat with model:', model);
        console.log('[LocalAI] Messages:', messages);
        try {
            console.log('[LocalAI] Fetching backend proxy /api/ollama/chat');
            const response = await fetch('http://localhost:8080/api/ollama/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ model, messages, stream: false })
            });
            console.log('[LocalAI] Response status:', response.status);
            if (!response.ok) {
                const errorText = await response.text();
                console.error('[LocalAI] Error response:', errorText);
                return { success: false, error: `HTTP ${response.status}: ${errorText}` };
            }
            const data = await response.json();
            console.log('[LocalAI] Success! Response:', data.message?.content?.substring(0, 100) + '...');
            return { success: true, response: data.message.content };
        } catch (error) {
            console.error('[LocalAI] Exception:', error);
            return { success: false, error: error.message };
        }
    }
}
