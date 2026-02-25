// Copilot Switch - Dual-Mode Execution Router
// Routes between Microsoft Copilot API and Agentic Copilot

const CopilotSwitch = {
    mode: "auto", // "auto", "microsoft", "agentic", "hybrid"
    microsoftAvailable: false,
    agenticAvailable: false,

    async initialize() {
        // Detect Microsoft Copilot API
        this.microsoftAvailable = this.detectMicrosoftAPI();

        // Check if Agentic Copilot (BigDaddyG) is registered
        this.agenticAvailable = AgentRegistry.isRegistered('BigDaddyG');

        // Log initialization status
        Telemetry.log('CopilotSwitchInitialized', {
            mode: this.mode,
            microsoftAvailable: this.microsoftAvailable,
            agenticAvailable: this.agenticAvailable,
            activeCopilot: this.getActiveCopilot()
        });

        console.log(`🔀 Copilot Switch initialized: ${this.getActiveCopilot()} mode active`);
    },

    detectMicrosoftAPI() {
        // Check for various Microsoft Copilot APIs
        return typeof window.MicrosoftCopilotAPI !== "undefined" ||
               typeof window.copilot !== "undefined" ||
               (typeof window.edge !== "undefined" && window.edge.copilot) ||
               (typeof window.office !== "undefined" && window.office.copilot);
    },

    getActiveCopilot() {
        switch (this.mode) {
            case "microsoft":
                return this.microsoftAvailable ? "Microsoft" : "Agentic";
            case "agentic":
                return this.agenticAvailable ? "Agentic" : "Microsoft";
            case "hybrid":
                return this.microsoftAvailable && this.agenticAvailable ? "Hybrid" : "Agentic";
            case "auto":
            default:
                if (this.microsoftAvailable) return "Microsoft";
                if (this.agenticAvailable) return "Agentic";
                return "None";
        }
    },

    async run(input, options = {}) {
        const active = this.getActiveCopilot();

        Telemetry.log('CopilotExecution', {
            mode: this.mode,
            activeCopilot: active,
            input: input?.substring(0, 100),
            options: Object.keys(options).length > 0
        });

        switch (active) {
            case "Microsoft":
                return await this.executeMicrosoft(input, options);

            case "Agentic":
                return await this.executeAgentic(input, options);

            case "Hybrid":
                return await this.executeHybrid(input, options);

            default:
                return "No Copilot available.";
        }
    },

    async executeMicrosoft(input, options) {
        try {
            const startTime = performance.now();

            let result;
            if (window.MicrosoftCopilotAPI) {
                result = await window.MicrosoftCopilotAPI.generate(input, options);
            } else if (window.copilot) {
                result = await window.copilot.generate(input, options);
            } else {
                throw new Error("Microsoft API not accessible");
            }

            const duration = performance.now() - startTime;
            Telemetry.performance('MicrosoftCopilotExecution', duration, { success: true });

            return {
                source: 'Microsoft',
                result,
                duration: `${duration}ms`,
                timestamp: new Date().toISOString()
            };

        } catch (error) {
            Telemetry.error(error, { copilot: 'Microsoft', input: input?.substring(0, 50) });
            return {
                source: 'Microsoft',
                error: error.message,
                timestamp: new Date().toISOString()
            };
        }
    },

    async executeAgentic(input, options) {
        try {
            const startTime = performance.now();
            const result = AgentRegistry.invoke('BigDaddyG', input, options);
            const duration = performance.now() - startTime;

            Telemetry.performance('AgenticCopilotExecution', duration, {
                success: !result.includes('not found'),
                agent: 'BigDaddyG'
            });

            return {
                source: 'Agentic',
                result,
                duration: `${duration}ms`,
                timestamp: new Date().toISOString()
            };

        } catch (error) {
            Telemetry.error(error, { copilot: 'Agentic', input: input?.substring(0, 50) });
            return {
                source: 'Agentic',
                error: error.message,
                timestamp: new Date().toISOString()
            };
        }
    },

    async executeHybrid(input, options) {
        try {
            const startTime = performance.now();

            // Execute both in parallel
            const [microsoftResult, agenticResult] = await Promise.allSettled([
                this.executeMicrosoft(input, options),
                this.executeAgentic(input, options)
            ]);

            const duration = performance.now() - startTime;

            return {
                source: 'Hybrid',
                microsoft: microsoftResult.status === 'fulfilled' ? microsoftResult.value : microsoftResult.reason,
                agentic: agenticResult.status === 'fulfilled' ? agenticResult.value : agenticResult.reason,
                duration: `${duration}ms`,
                timestamp: new Date().toISOString()
            };

        } catch (error) {
            Telemetry.error(error, { copilot: 'Hybrid', input: input?.substring(0, 50) });
            return {
                source: 'Hybrid',
                error: error.message,
                timestamp: new Date().toISOString()
            };
        }
    },

    getStatus() {
        return {
            mode: this.mode,
            microsoftAvailable: this.microsoftAvailable,
            agenticAvailable: this.agenticAvailable,
            activeCopilot: this.getActiveCopilot()
        };
    }
};

// Export to global scope for easy access
if (typeof window !== 'undefined') {
    window.CopilotSwitch = CopilotSwitch;
}
