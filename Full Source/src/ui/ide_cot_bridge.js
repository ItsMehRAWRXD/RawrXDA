/**
 * ide_cot_bridge.js — RawrXD CoT Streaming Renderer
 * ==================================================
 *
 * Frontend bridge for the CoT pipeline. Shows thought steps as they
 * arrive, preventing the "empty response" UI freeze.
 *
 * Integrates with:
 *   - /api/cot endpoint (rawrxd_cot_engine.py)
 *   - Chat panel (src/ui/chat_panel.cpp → WebView)
 *   - MASM CoT DLL status via /api/cot/health
 *
 * Architecture: Vanilla JS, no framework dependency
 * Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
 */

// =============================================================================
// CoTStream — Streaming Chain-of-Thought Renderer
// =============================================================================
class CoTStream {
  /**
   * @param {string} containerId — DOM element ID for CoT panel
   * @param {string} apiBase    — Base URL for CoT API (default: same origin)
   */
  constructor(containerId, apiBase = '') {
    this.container = document.getElementById(containerId);
    this.apiBase = apiBase;
    this.steps = [];
    this.abortController = null;

    if (!this.container) {
      console.error(`[CoTStream] Container '${containerId}' not found`);
    }
  }

  /**
   * Run the full CoT pipeline and render results.
   * Returns the final answer string for the chat handler to display.
   *
   * @param {string} userInput  — User's message
   * @param {string} model      — Ollama model name
   * @returns {Promise<string>} — Final answer text
   */
  async process(userInput, model = 'bigdaddyg-fast:latest') {
    // Show loading state immediately
    this._showLoading();

    // Cancel any in-flight request
    if (this.abortController) {
      this.abortController.abort();
    }
    this.abortController = new AbortController();

    try {
      const response = await fetch(`${this.apiBase}/api/cot`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ message: userInput, model }),
        signal: this.abortController.signal,
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      this.render(data);
      return data.final_answer || userInput;

    } catch (e) {
      if (e.name === 'AbortError') {
        this._showAborted();
        return '';
      }
      this._showError(e.message);
      console.error('[CoTStream] Pipeline error:', e);
      return userInput; // Fallback: echo user input so chat doesn't break
    } finally {
      this.abortController = null;
    }
  }

  /**
   * Render CoT pipeline result into the container.
   * Handles trivial bypass, step-by-step display, and empty recovery.
   *
   * @param {Object} data — Response from /api/cot
   */
  render(data) {
    if (!this.container) return;

    // Fast path: trivial input was detected — show skip badge
    if (data.trivial) {
      this.container.innerHTML =
        '<div class="cot-skip">' +
        '\u26A1 Fast response (greeting detected \u2014 CoT bypassed)' +
        '</div>';
      return;
    }

    let html = '<div class="cot-pipeline">';

    // Render each step
    if (data.steps && data.steps.length > 0) {
      data.steps.forEach((step, idx) => {
        const icon = this._roleIcon(step.role);
        const timeStr = step.latency_ms ? `${step.latency_ms}ms` : '';
        const content = step.content
          ? this._escapeHtml(step.content).replace(/\n/g, '<br>')
          : '<em>[Empty response recovered]</em>';
        const skippedBadge = step.skipped
          ? ' <span class="cot-badge-skip">BYPASSED</span>'
          : '';

        html += `
                    <div class="cot-step" style="animation-delay: ${idx * 0.1}s">
                        <div class="cot-header">
                            <span class="cot-icon">${icon}</span>
                            <span class="cot-role">${this._escapeHtml(step.role)}</span>
                            ${skippedBadge}
                            <span class="cot-model">${this._escapeHtml(step.model || '')}</span>
                            <span class="cot-time">${timeStr}</span>
                        </div>
                        <div class="cot-content">${content}</div>
                    </div>
                `;
      });
    } else {
      html += '<div class="cot-empty">No steps returned from pipeline</div>';
    }

    // Final answer summary bar
    const totalTime = data.total_ms ? `${data.total_ms}ms` : '';
    html += `
            <div class="cot-final">
                \u2705 Final Answer (${totalTime})
            </div>
        `;
    html += '</div>';

    this.container.innerHTML = html;
  }

  /**
   * Cancel any in-flight CoT request.
   */
  cancel() {
    if (this.abortController) {
      this.abortController.abort();
      this.abortController = null;
    }
  }

  /**
   * Fetch CoT engine health status.
   * @returns {Promise<Object>} — Health check result
   */
  async checkHealth() {
    try {
      const resp = await fetch(`${this.apiBase}/api/cot/health`, {
        timeout: 3000,
      });
      return await resp.json();
    } catch (e) {
      return { status: 'unreachable', error: e.message };
    }
  }

  /**
   * Fetch aggregated metrics from the CoT engine.
   * @returns {Promise<Object>} — Metrics counters
   */
  async getMetrics() {
    try {
      const resp = await fetch(`${this.apiBase}/api/cot/metrics`);
      return await resp.json();
    } catch (e) {
      return { error: e.message };
    }
  }

  // =========================================================================
  // Private helpers
  // =========================================================================

  _roleIcon(role) {
    const icons = {
      'Critic': '\uD83E\uDDD0', // 🧐
      'Auditor': '\uD83D\uDEE1',  // 🛡
      'Synthesizer': '\u2705',         // ✅
      'Reviewer': '\uD83D\uDD0D',   // 🔍
      'Thinker': '\uD83E\uDDE0',   // 🧠
      'Researcher': '\uD83D\uDCDA',   // 📚
      'Verifier': '\u2714\uFE0F',    // ✔️
    };
    return icons[role] || '\uD83D\uDD39'; // 🔹 default
  }

  _escapeHtml(text) {
    const map = { '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;', "'": '&#039;' };
    return String(text).replace(/[&<>"']/g, c => map[c]);
  }

  _showLoading() {
    if (this.container) {
      this.container.innerHTML =
        '<div class="cot-loading">' +
        '\uD83E\uDDE0 Initializing chain...' +
        '</div>';
    }
  }

  _showError(message) {
    if (this.container) {
      this.container.innerHTML =
        `<div class="cot-error">Chain failed: ${this._escapeHtml(message)}</div>`;
    }
  }

  _showAborted() {
    if (this.container) {
      this.container.innerHTML =
        '<div class="cot-aborted">Chain cancelled by user</div>';
    }
  }
}

// =============================================================================
// Export for module systems or global usage
// =============================================================================
if (typeof module !== 'undefined' && module.exports) {
  module.exports = { CoTStream };
} else if (typeof window !== 'undefined') {
  window.CoTStream = CoTStream;
}

// =============================================================================
// Usage example (integrate with your chat handler):
// =============================================================================
// const cot = new CoTStream('cot-panel');
// const answer = await cot.process(userMessage);
// addMessage('assistant', answer);
