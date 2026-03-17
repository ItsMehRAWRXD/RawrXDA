// =======================================================
// BIGDADDYG LOCAL AGENTIC BRIDGE CLIENT
// Full FS, Terminal, Agents, GitHub integration
// =======================================================

// ---------- CONFIG ----------
window.AgentBridge = {
    base: "http://localhost:15500",
    githubToken: "",      // Set dynamically by user
    githubUser: "",
};

// ---------- UTIL ----------
async function api(path, method = "GET", body = null) {
    const opt = { method, headers: {} };
    if (body) {
        opt.headers["Content-Type"] = "application/json";
        opt.body = JSON.stringify(body);
    }
    const r = await fetch(AgentBridge.base + path, opt);
    return await r.json();
}

// =======================================================
// FILESYSTEM API
// =======================================================
window.fsAPI = {
    list: (path) => api(`/api/fs/list?path=${encodeURIComponent(path)}`),
    read: (path) => api('/api/fs/read', "POST", { path }),
    write: (path, content) => api('/api/fs/write', "POST", { path, content }),
    delete: (path) => api('/api/fs/delete', "POST", { path }),
};

// =======================================================
// POWERSHELL / TERMINAL API
// =======================================================
window.shell = {
    run: (command) => api('/api/pwsh/run', "POST", { command })
};

// =======================================================
// LOCAL MODEL API - CONNECTED TO OLLAMA
// =======================================================
window.Models = {
    // Fetch available models from Ollama
    async list() {
        try {
            const response = await fetch('http://localhost:11434/api/tags');
            if (!response.ok) throw new Error(`HTTP ${ response.status }`);
            const data = await response.json();
            return data.models?.map(m => m.name) || [];
        } catch (err) {
            console.error("Failed to fetch Ollama models:", err);
            // Fallback: try Orchestra Server
            try {
                const orchestraResponse = await fetch('http://localhost:11442/v1/models');
                if (orchestraResponse.ok) {
                    const orchestraData = await orchestraResponse.json();
                    return orchestraData.data?.map(m => m.id) || [];
                }
            } catch (e) {
                console.error("Orchestra Server also unavailable:", e);
            }
            return [];
        }
    },

    // Run model via Ollama or Orchestra Server
    async run(model, prompt) {
        // Try Orchestra Server first (port 11442)
        try {
            const response = await fetch('http://localhost:11442/v1/chat/completions', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model: model || 'BigDaddyG:Latest',
                    messages: [
                        { role: 'user', content: prompt }
                    ],
                    temperature: 0.7,
                    max_tokens: 2048
                })
            });

            if (response.ok) {
                const data = await response.json();
                return data.choices?.[0]?.message?.content || data.response || '';
            }
        } catch (orchestraError) {
            console.warn('Orchestra Server unavailable, trying direct Ollama:', orchestraError);
        }

        // Fallback to direct Ollama API
        try {
            const response = await fetch('http://localhost:11434/api/generate', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    model: model || 'bigdaddyg:latest',
                    prompt: prompt,
                    stream: false,
                    options: {
                        temperature: 0.7,
                        num_predict: 2048
                    }
                })
            });

            if (!response.ok) {
                throw new Error(`Ollama returned ${ response.status }`);
            }

            const data = await response.json();
            return data.response || '';
        } catch (err) {
            console.error("Ollama API error:", err);
            throw new Error(`Failed to connect to Ollama. Make sure Ollama is running on port 11434. Error: ${ err.message }`);
        }
    }
};

// =======================================================
// CLOUD AGENTS (AI SERVICES)
// =======================================================

// Example endpoints (replace with your own API keys later)
window.CloudAgents = {
    gpt: async (prompt) => {
        return "GPT response stub. Add your API key.";
    },
    claude: async (prompt) => {
        return "Claude response stub. Add your API key.";
    },
    gemini: async (prompt) => {
        return "Gemini response stub. Add your API key.";
    },
    deepseek: async (prompt) => {
        return "DeepSeek response stub. Add your API key.";
    }
};

// =======================================================
// GITHUB AGENT (Reads/Writes Repos)
// =======================================================
window.GitHubAgent = {

    setToken(token) {
        AgentBridge.githubToken = token;
    },

    async request(path, method = "GET", body = null) {
        const opt = {
            method,
            headers: {
                "Authorization": `Bearer ${AgentBridge.githubToken}`,
                "Accept": "application/vnd.github+json"
            }
        };
        if (body) {
            opt.headers["Content-Type"] = "application/json";
            opt.body = JSON.stringify(body);
        }

        const r = await fetch(`https://api.github.com${path}`, opt);
        return await r.json();
    },

    getUser() {
        return this.request("/user");
    },

    listRepos() {
        return this.request("/user/repos");
    },

    getFile(owner, repo, path, branch = "main") {
        return this.request(`/repos/${owner}/${repo}/contents/${path}?ref=${branch}`);
    },

    updateFile(owner, repo, path, message, contentBase64, sha) {
        return this.request(`/repos/${owner}/${repo}/contents/${path}`, "PUT", {
            message,
            content: contentBase64,
            sha
        });
    },

    createPR(owner, repo, title, head, base = "main") {
        return this.request(`/repos/${owner}/${repo}/pulls`, "POST", {
            title,
            head,
            base
        });
    },

    commitAutoFix: async function (owner, repo, filePath, newContent, message) {
        const file = await this.getFile(owner, repo, filePath);
        const sha = file.sha;

        const base64 = btoa(unescape(encodeURIComponent(newContent)));

        return this.updateFile(owner, repo, filePath, message, base64, sha);
    }
};

// =======================================================
// AGENTIC WRAPPER (Auto-route to chosen model)
// =======================================================
window.Agent = {

    activeModel: "local",

    setModel(name) {
        this.activeModel = name;
        addMessageToChat("agent", "Model switched → <b>" + name + "</b>");
    },

    async run(prompt) {

        if (this.activeModel.startsWith("local:")) {
            const model = this.activeModel.replace("local:", "");
            return await Models.run(model, prompt);
        }

        if (this.activeModel === "gpt") return await CloudAgents.gpt(prompt);
        if (this.activeModel === "claude") return await CloudAgents.claude(prompt);
        if (this.activeModel === "gemini") return await CloudAgents.gemini(prompt);
        if (this.activeModel === "deepseek") return await CloudAgents.deepseek(prompt);

        return "Unknown model.";
    }
};

// =======================================================
// INTEGRATION READY
// Call fsAPI, shell, Models, Agent, GitHubAgent from UI
// =======================================================
console.log("🔥 Agentic Bridge Ready — FS, Terminal, Models, GitHub Enabled");
