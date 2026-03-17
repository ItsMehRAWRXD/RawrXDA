g++ main.cpp -o main.exe; .\main.exeg++ main.cpp -o main.exe; .\main.exewindow.ide.initializeAllSystems = async function() {
    console.log(" Master Rule: Initializing all systems...");

    try {
        // 1. Fix missing methods
        if (typeof this.addAIMessage !== "function") {
            this.addAIMessage = (sender, content) => {
                console.log(`[${sender}] ${content}`);
            };
        }

        // 2. Patch AI buttons
        if (typeof this.explainCode !== "function") {
            this.explainCode = () => this.addAIMessage("system", "ExplainCode not yet implemented.");
        }

        if (typeof this.sendAIMessage !== "function") {
            this.sendAIMessage = (msg) => this.addAIMessage("user", msg);
        }

        // 3. Cursor integration
        if (typeof this.initCursorIntegration !== "function") {
            this.initCursorIntegration = async () => {
                console.log(" Initializing Cursor integration...");
                try {
                    const res = await fetch("http://localhost:7070/api/status");
                    const status = await res.json();
                    if (status.ready) {
                        this.cursorAgent = {
                            connect: async () => {
                                const r = await fetch("http://localhost:7070/api/connect", { method: "POST" });
                                const d = await r.json();
                                this.addAIMessage("system", `Connected to Cursor: ${d.sessionId}`);
                            },
                            sendCode: async (code) => {
                                const r = await fetch("http://localhost:7070/api/code", {
                                    method: "POST",
                                    headers: { "Content-Type": "application/json" },
                                    body: JSON.stringify({ code })
                                });
                                const d = await r.json();
                                this.addAIMessage("cursor", d.output || "No response");
                            }
                        };
                        this.addAIMessage("system", " Cursor agent is ready.");
                    } else {
                        this.addAIMessage("system", " Cursor agent responded but is not ready.");
                    }
                } catch (err) {
                    console.error("Cursor connection failed:", err);
                    this.addAIMessage("system", " Could not connect to Cursor agent.");
                }
            };
        }

        // 4. Auto-heal broken UI buttons
        document.querySelectorAll("button[onclick]").forEach(btn => {
            const code = btn.getAttribute("onclick");
            try {
                new Function(code)(); // test execution
            } catch (e) {
                console.warn(`Auto-heal: fixing broken button`, btn, e);
                btn.onclick = () => this.addAIMessage("system", `Button "${btn.textContent}" not yet wired.`);
            }
        });

        // 5. Launch integrations
        await this.initCursorIntegration();

        this.addAIMessage("system", " All systems initialized.");
    } catch (err) {
        console.error("Master Rule failed:", err);
        this.addAIMessage("system", " Master Rule encountered an error.");
    }
};
