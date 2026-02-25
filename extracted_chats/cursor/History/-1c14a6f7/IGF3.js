/**
 * Code Supernova MAX Stealth Extension - Main Entry Point
 * Integrates the ultra-capacity stealth agent into the local IDE runtime
 */

import { RuntimeAPI } from "../core/runtimeApi.js";
import { SupernovaAgent } from "./agent/supernovaAgent.js";
import { createDashboard } from "./ui/dashboard.js";
import { createAgentChatPanel } from "./ui/agentChatPanel.js";
import { createAgentDock } from "./ui/agentDock.js";
import { createAgentConsole } from "./debug/agentConsole.js";

export function activate(context) {
    const { api } = context;

    console.log('🚀 Activating Code Supernova MAX Stealth Agent...');

    // Initialize the Supernova agent
    const agent = new SupernovaAgent(api);

    // Register commands
    api.registerCommand("supernova.runAnalysis", async () => {
        api.showInformation("Running Supernova deep code analysis...");
        const result = await agent.runAnalysis();
        api.showInformation(`Analysis complete: ${result.summary}`);
    });

    api.registerCommand("supernova.generateCode", async () => {
        const query = await api.showInputBox({
            placeHolder: "Describe the code you want to generate...",
            prompt: "Code Generation Request"
        });
        if (query) {
            const result = await agent.generateCode(query);
            api.showInformation(`Generated code: ${result.length} characters`);
        }
    });

    api.registerCommand("supernova.contextSweep", async () => {
        api.showInformation("Sweeping workspace context...");
        await agent.contextSweep();
        api.showInformation("Context sweep complete");
    });

    api.registerCommand("supernova.openDashboard", () => {
        createDashboard(api, agent);
    });

    // Register agent tools
    api.registerTool("supernova", agent);
    api.registerTool("supernova.analyze", (code) => agent.analyze(code));
    api.registerTool("supernova.generate", (query) => agent.generateCode(query));
    api.registerTool("supernova.explain", (code) => agent.explainCode(code));

    // Create UI components
    createAgentChatPanel(api, agent);
    createAgentDock(api);
    createAgentConsole(api);

    // Initialize multi-session chat
    api.registerCommand("supernova.newChat", () => {
        const sessionId = `session-${Date.now()}`;
        agent.createSession(sessionId);
        api.showInformation(`New Supernova chat session: ${sessionId}`);
    });

    console.log('✅ Code Supernova MAX Stealth activated');
    api.showInformation("Code Supernova MAX Stealth loaded (offline mode, 1M context).");
}

export function deactivate() {
    console.log('🔄 Deactivating Code Supernova MAX Stealth...');
    // Cleanup resources
}
