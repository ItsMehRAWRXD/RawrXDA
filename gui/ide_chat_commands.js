// ============================================================================
// ide_chat_commands.js — Chat Command Router
// ============================================================================
//
// Action Item #19: Expose P-settings, bulk fixes, and reasoning modes
// from the chatbox via /commands.
//
// Commands:
//   /p <0-8>          — Set reasoning depth
//   /preset <name>    — Load reasoning profile
//   /cot <mode>       — Chain-of-Thought visibility
//   /fix <strat> <f>  — Autonomous bulk fix
//   /agent <task>     — Dispatch autonomous agent
//   /maxlen <n>       — Set max input length
//   /thermal          — Show CPU/thermal status
//   /swarm <on|off>   — Toggle parallel agent execution
//   /trace            — Toggle pipeline trace mode
//   /help             — Show help
//
// Integration: Loaded by ide_chatbot.html after ide_chatbot_engine.js
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

(function () {
  'use strict';

  // ========================================================================
  // Chat Command Router
  // ========================================================================
  class ChatCommandRouter {
    constructor() {
      this.prefix = '/';
      this.commands = {
        'p': this.cmdReasoningDepth.bind(this),
        'preset': this.cmdReasoningPreset.bind(this),
        'cot': this.cmdToggleCoT.bind(this),
        'fix': this.cmdBulkFix.bind(this),
        'agent': this.cmdAutonomousAgent.bind(this),
        'maxlen': this.cmdMaxInputLength.bind(this),
        'thermal': this.cmdThermalStatus.bind(this),
        'swarm': this.cmdSwarmMode.bind(this),
        'trace': this.cmdTrace.bind(this),
        'help': this.cmdHelp.bind(this)
      };
    }

    /**
     * Process a chat input. Returns command result or null if not a command.
     * @param {string} input — raw user input
     * @returns {Object|null} — { text, type, hint?, actions? } or null
     */
    async process(input) {
      if (!input || !input.startsWith(this.prefix)) return null;

      const parts = input.slice(1).trim().split(/\s+/);
      const cmd = parts[0].toLowerCase();
      const args = parts.slice(1);

      if (this.commands[cmd]) {
        try {
          return await this.commands[cmd](args);
        } catch (e) {
          return { text: 'Command error: ' + e.message, type: 'error' };
        }
      }
      return { error: 'Unknown command: ' + cmd + '. Type /help for list.' };
    }

    // ====================================================================
    // REASONING PIPELINE COMMANDS
    // ====================================================================

    async cmdReasoningDepth(args) {
      const depth = parseInt(args[0]);
      if (isNaN(depth) || depth < 0 || depth > 8) {
        return {
          text: 'Usage: /p <0-8> (0=instant, 4=normal, 8=exhaustive)',
          type: 'system'
        };
      }

      // Call C++ backend via bridge (if available)
      if (window.ideBridge && window.ideBridge.call) {
        await window.ideBridge.call('setReasoningDepth', [depth]);
      }

      // Update local state
      this._ensureState();
      window.State.reasoning.depth = depth;

      const label = this._getDepthLabel(depth);
      return {
        text: 'Reasoning depth set to P' + depth + ' (' + label + ')',
        type: 'system',
        meta: { depth: depth, mode: window.State.reasoning.mode }
      };
    }

    async cmdReasoningPreset(args) {
      const preset = (args[0] || '').toLowerCase();
      const valid = ['fast', 'normal', 'deep', 'critical', 'swarm',
        'adaptive', 'dev', 'max'];

      if (!valid.includes(preset)) {
        return {
          text: 'Usage: /preset <' + valid.join('|') + '>',
          type: 'error'
        };
      }

      if (window.ideBridge && window.ideBridge.call) {
        await window.ideBridge.call('applyReasoningPreset', [preset]);
      }

      this._ensureState();
      window.State.reasoning.preset = preset;

      return {
        text: 'Reasoning preset: ' + preset.toUpperCase(),
        type: 'system',
        hint: preset === 'swarm' ? 'Parallel agents enabled' :
          preset === 'adaptive' ? 'Auto-scaling based on complexity' : ''
      };
    }

    async cmdToggleCoT(args) {
      const mode = args[0] || 'toggle';
      const validModes = ['off', 'summary', 'full', 'dev'];

      this._ensureState();
      let visibility;
      if (validModes.includes(mode)) {
        visibility = mode;
      } else {
        // Toggle between off and summary
        visibility = window.State.reasoning.cotVisible ? 'off' : 'summary';
      }

      window.State.reasoning.cotVisible = (visibility !== 'off');
      window.State.reasoning.cotMode = visibility;

      return {
        text: 'Chain-of-Thought: ' + visibility.toUpperCase(),
        type: 'system',
        note: visibility === 'off' ? 'Hidden (fastest)' :
          visibility === 'summary' ? 'Showing step count only' :
            'Full visibility'
      };
    }

    // ====================================================================
    // AUTONOMOUS AGENT COMMANDS
    // ====================================================================

    async cmdBulkFix(args) {
      if (args.length < 2) {
        return {
          text: 'Usage: /fix <strategy> <target...>\n' +
            'Strategies: compile, format, refactor, stubs, headers, ' +
            'lint, tests, docs, security',
          type: 'error'
        };
      }

      const strategy = args[0];
      const targets = args.slice(1);

      try {
        const resp = await fetch('/api/agent/bulkfix', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            strategy: strategy,
            targets: targets,
            parentAgentId: (window.State && window.State.currentAgentId) ||
              'user-initiated',
            autoVerify: true,
            maxConcurrency: 4
          })
        });
        const result = await resp.json();

        if (!resp.ok) {
          return {
            text: 'Bulk fix error: ' + (result.errorDetail || result.error || 'Unknown'),
            type: 'error'
          };
        }

        return {
          text: 'Bulk fix complete: ' + (result.fixed || 0) + '/' +
            (result.total || 0) + ' items processed\n' +
            (result.report || ''),
          type: 'agent',
          actions: (result.failed && result.failed.length > 0) ? [{
            label: 'Retry Failed',
            command: '/fix ' + strategy + ' ' + result.failed.join(' ')
          }] : []
        };
      } catch (e) {
        return {
          text: 'Bulk fix request failed: ' + e.message,
          type: 'error'
        };
      }
    }

    async cmdAutonomousAgent(args) {
      const task = args.join(' ');
      if (!task) {
        return { text: 'Usage: /agent <task description>', type: 'error' };
      }

      try {
        const resp = await fetch('/api/agent/plan', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ intent: task, autoExecute: true })
        });
        const plan = await resp.json();

        return {
          text: 'Autonomous agent dispatched: ' +
            (plan.steps ? plan.steps.length : 0) + ' steps planned',
          type: 'agent',
          plan: plan.steps,
          duration: plan.estimatedDuration
        };
      } catch (e) {
        return {
          text: 'Agent dispatch failed: ' + e.message,
          type: 'error'
        };
      }
    }

    // ====================================================================
    // SYSTEM COMMANDS
    // ====================================================================

    async cmdMaxInputLength(args) {
      const len = parseInt(args[0]);
      this._ensureState();

      if (isNaN(len) || len < 1) {
        const current = window.State.security.inputGuard.maxLength;
        return {
          text: 'Current max input: ' + current.toLocaleString() + ' chars',
          type: 'system'
        };
      }

      // Clamp to reasonable bounds
      const clamped = Math.max(1, Math.min(len, 1000000000));
      window.State.security.inputGuard.maxLength = clamped;

      return {
        text: 'Max input length set to: ' + clamped.toLocaleString(),
        type: 'system'
      };
    }

    async cmdThermalStatus() {
      try {
        if (window.ideBridge && window.ideBridge.call) {
          const status = await window.ideBridge.call('getThermalStatus');
          return {
            text: 'Thermal: ' + (status.cpuTemp || '?') + ' C | ' +
              'Load: ' + (status.cpuLoad || '?') + '% | ' +
              'Throttle: ' + (status.throttling ? 'ACTIVE' : 'inactive'),
            type: 'system',
            alert: status.throttling ?
              'Performance reduced due to thermal limits' : null
          };
        }
        return {
          text: 'Thermal monitoring: bridge not available',
          type: 'system'
        };
      } catch (e) {
        return { text: 'Thermal query failed: ' + e.message, type: 'error' };
      }
    }

    async cmdSwarmMode(args) {
      this._ensureState();
      const enabled = args[0] === 'on' ||
        (args[0] !== 'off' && !window.State.reasoning.swarm);
      window.State.reasoning.swarm = enabled;

      if (enabled) {
        if (window.ideBridge && window.ideBridge.call) {
          await window.ideBridge.call('setReasoningPreset', ['swarm']);
        }
        return { text: 'Swarm mode enabled: Parallel agent execution', type: 'system' };
      }
      return { text: 'Swarm mode disabled', type: 'system' };
    }

    async cmdTrace(args) {
      this._ensureState();
      const enabled = args[0] === 'on' ||
        (args[0] !== 'off' && !window.State.reasoning.traceEnabled);
      window.State.reasoning.traceEnabled = enabled;

      return {
        text: 'Pipeline trace mode: ' + (enabled ? 'ON' : 'OFF'),
        type: 'system',
        note: enabled ? 'Add ?trace=1 to see routing decisions' : ''
      };
    }

    cmdHelp() {
      return {
        text:
          'Available Commands:\n' +
          '/p <0-8>           --- Set reasoning depth (P-settings)\n' +
          '/preset <name>     --- Load reasoning profile\n' +
          '     fast|normal|deep|critical|swarm|adaptive|dev|max\n' +
          '/cot <mode>        --- CoT visibility (off|summary|full|dev)\n' +
          '/fix <strat> <f>   --- Autonomous bulk fix\n' +
          '     compile|format|refactor|stubs|headers|lint|tests|docs|security\n' +
          '/agent <task>      --- Dispatch autonomous agent\n' +
          '/maxlen <n>        --- Set max input length\n' +
          '/thermal           --- Show CPU/thermal status\n' +
          '/swarm <on|off>    --- Toggle parallel agents\n' +
          '/trace <on|off>    --- Toggle pipeline trace\n' +
          '/help              --- Show this help',
        type: 'system'
      };
    }

    // ====================================================================
    // Helpers
    // ====================================================================

    _getDepthLabel(d) {
      const labels = [
        'Instant', 'Fast', 'Light', 'Normal', 'Deep',
        'Analysis', 'Research', 'Maximal', 'Exhaustive'
      ];
      return labels[d] || 'Custom';
    }

    _ensureState() {
      if (!window.State) window.State = {};
      if (!window.State.reasoning) {
        window.State.reasoning = {
          depth: 4,
          preset: 'normal',
          mode: 'adaptive',
          cotVisible: false,
          cotMode: 'summary',
          swarm: false,
          thermalThrottling: false,
          traceEnabled: false
        };
      }
      if (!window.State.security) {
        window.State.security = {
          inputGuard: { maxLength: 1000000000 }
        };
      }
    }
  }

  // ========================================================================
  // Initialize and hook into global sendMessage
  // ========================================================================
  const cmdRouter = new ChatCommandRouter();

  // Helper: display a command result in the chat
  function displayCommandResult(result) {
    if (!result) return;

    // Use addMessage if available (from ide_chatbot_engine.js)
    const addMsg = window.addMessage || window.addSystemMessage;
    if (addMsg) {
      if (result.error) {
        addMsg('error', result.error);
      } else {
        addMsg(result.type || 'system', result.text || '');
        if (result.hint) {
          addMsg('hint', result.hint);
        }
        if (result.note) {
          addMsg('system', result.note);
        }
      }
      return;
    }

    // Fallback: log to console
    console.log('[ChatCmd]', result.text || result.error);
  }

  /**
   * Hook: intercept sendMessage to process /commands first.
   * Preserves the original sendMessage for non-command inputs.
   */
  function hookSendMessage() {
    if (window._chatCmdHooked) return;

    const originalSend = window.sendMessage;
    if (typeof originalSend !== 'function') {
      // sendMessage not yet defined; retry later
      setTimeout(hookSendMessage, 500);
      return;
    }

    window.sendMessage = async function (text) {
      // Check if it's a command
      const result = await cmdRouter.process(text);
      if (result) {
        displayCommandResult(result);
        return; // Don't forward to LLM
      }
      // Not a command — proceed normally
      return originalSend.apply(this, arguments);
    };

    window._chatCmdHooked = true;
  }

  // ========================================================================
  // Exports
  // ========================================================================
  window.ChatCommandRouter = ChatCommandRouter;
  window.chatCmdRouter = cmdRouter;

  // Hook on load
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', hookSendMessage);
  } else {
    hookSendMessage();
  }

})();
