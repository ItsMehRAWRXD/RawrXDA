// Agentic Bridge - Converts AI responses to file operations
(function (global) {
    if (global.AgenticBridge) {
        // Already defined, avoid redeclaration
        return;
    }

    class AgenticBridge {
        constructor(ide) {
            this.ide = ide;
        }

        async run(provider, prompt) {
            const agenticPrompt = this.buildPrompt(prompt);
            const reply = await this.askProvider(provider, agenticPrompt);
            if (!reply) return;

            const plan = this.safeParse(reply);
            if (!plan) return;

            await this.execute(plan);
        }

        buildPrompt(userPrompt) {
            return `You are an agentic coding assistant. Answer with a single JSON block and nothing else.

Task: ${userPrompt}

Allowed actions:
- write_file {"type":"write_file","path":"...","content":"..."}
- run_command {"type":"run_command","cmd":"..."}
- read_file {"type":"read_file","path":"..."}

Response schema:
{"actions":[{"type":"write_file","path":"file.js","content":"code"}]}`;
        }

        askProvider(provider, prompt) {
            const urls = {
                chatgpt: 'https://chatgpt.com/?q=',
                claude: 'https://claude.ai/new?q=',
                gemini: 'https://gemini.google.com/?q=',
                kimi: 'https://kimi.moonshot.cn/?q=',
                perplexity: 'https://www.perplexity.ai/?q='
            };

            const base = urls[provider] || urls.chatgpt;
            window.open(base + encodeURIComponent(prompt), provider, 'width=1200,height=800');

            this.ide.addAIMessage('system', `AI ${provider} opened - paste JSON reply`);
            return prompt('Paste AI JSON response:');
        }

        safeParse(raw) {
            try {
                const cleaned = raw.replace(/```json|```/g, '').trim();
                return JSON.parse(cleaned);
            } catch (e) {
                this.ide.addAIMessage('system', `Invalid JSON - ${e.message}`);
                return null;
            }
        }

        async execute(plan) {
            if (!Array.isArray(plan?.actions)) {
                this.ide.addAIMessage('system', 'Plan missing actions array');
                return;
            }

            for (const a of plan.actions) {
                try {
                    switch (a.type) {
                        case 'write_file':
                            await this.writeFile(a.path, a.content);
                            break;
                        case 'run_command':
                            await this.runCommand(a.cmd);
                            break;
                        case 'read_file':
                            await this.readFile(a.path);
                            break;
                        default:
                            this.ide.addAIMessage('system', `Unknown action "${a.type}"`);
                    }
                } catch (e) {
                    this.ide.addAIMessage('system', `Action failed - ${e.message}`);
                }
            }
            const msg = 'Agentic task complete';
            this.ide.addAIMessage('system', msg);
            if (this.ide.screenReader) this.ide.screenReader.announce(msg);
        }

        async writeFile(path, content) {
            const r = await fetch('/api/write-file', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ path, content })
            });
            if (!r.ok) throw new Error(`write failed ${r.status}`);
            const msg = `Wrote ${path}`;
            this.ide.addAIMessage('system', msg);
            if (this.ide.screenReader) this.ide.screenReader.announce(msg);
        }

        async runCommand(cmd) {
            const r = await fetch('/api/run-command', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ cmd })
            });
            if (!r.ok) throw new Error(`command failed ${r.status}`);
            const { output } = await r.json();
            const msg = `Running ${cmd}`;
            this.ide.addAIMessage('system', `${cmd}\n${output}`);
            if (this.ide.screenReader) this.ide.screenReader.announce(msg);
        }

        async readFile(path) {
            const r = await fetch(`/api/read-file?path=${encodeURIComponent(path)}`);
            if (!r.ok) throw new Error(`read failed ${r.status}`);
            const { content } = await r.json();
            this.ide.addAIMessage('system', `Read ${path}`);
            return content;
        }
    }

    global.AgenticBridge = AgenticBridge;
    if (typeof module !== 'undefined') module.exports = AgenticBridge;
})(typeof window !== 'undefined' ? window : globalThis);
