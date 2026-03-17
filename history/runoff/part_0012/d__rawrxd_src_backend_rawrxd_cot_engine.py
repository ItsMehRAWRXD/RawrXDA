#!/usr/bin/env python3
"""
rawrxd_cot_engine.py — Smart CoT Router + Fault-Tolerant Aggregator
====================================================================

Production fix for the Chain-of-Thought pipeline:
  - Smart Router: skips deep analysis for trivial inputs (greetings)
  - Fault-Tolerant Aggregator: handles empty/failed steps with fallback
  - Streaming-ready: returns step-by-step results for UI rendering

Integrates with:
  - Ollama backend (localhost:11434)
  - rawrxd_cot_engine.asm (MASM DLL via ctypes, optional)
  - src/api_server.cpp (HTTP bridge)

Architecture: Python 3.10+, no exceptions swallowed silently
Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
"""

import re
import json
import time
import logging
import requests
from typing import List, Dict, Optional
from concurrent.futures import ThreadPoolExecutor, as_completed

# =============================================================================
# Structured Logging (per tools.instructions.md §1)
# =============================================================================
logger = logging.getLogger("RawrXD.CoT")
logger.setLevel(logging.DEBUG)
_handler = logging.StreamHandler()
_handler.setFormatter(logging.Formatter(
    "[%(asctime)s.%(msecs)03d] [%(name)s] [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
))
logger.addHandler(_handler)

# =============================================================================
# Metrics counters (per tools.instructions.md §1 — Metrics Generation)
# =============================================================================
_metrics = {
    "total_requests": 0,
    "trivial_bypassed": 0,
    "full_chain_runs": 0,
    "critic_errors": 0,
    "auditor_errors": 0,
    "synthesis_errors": 0,
    "total_latency_ms": 0,
}


def get_metrics() -> Dict:
    """Expose metrics for Prometheus scraping or dashboard polling."""
    return dict(_metrics)


# =============================================================================
# RawrXDCotEngine — Smart CoT Router + Fault-Tolerant Aggregator
# =============================================================================
class RawrXDCotEngine:
    """
    Three-stage CoT pipeline with smart routing:
      1. Critic   — Analyzes input (gentle mode for trivial)
      2. Auditor  — Reviews Critic output (passthrough for trivial)
      3. Synthesizer — Aggregates all steps into final answer

    Guarantees non-empty output via fallback echo.
    """

    def __init__(self, ollama_url: str = "http://localhost:11434"):
        self.ollama = ollama_url.rstrip("/")
        # Regex for trivial social inputs — skip deep CoT analysis
        self.trivial_pattern = re.compile(
            r'^(hi+|hello|hey|help|test|ok|yes|no|thanks?|bye|sup|\?)\s*$',
            re.IGNORECASE
        )
        # Temperature config: lower for Critic to avoid hallucinated importance
        self.critic_temperature = 0.4
        self.synthesis_temperature = 0.7
        logger.info("CoT engine initialized — Ollama endpoint: %s", self.ollama)

    # =========================================================================
    # Trivial Input Detection (Smart Router)
    # =========================================================================
    def is_trivial(self, user_input: str) -> bool:
        """
        Skip CoT for simple social greetings / ultra-short inputs.
        Prevents the Critic from over-analyzing "Hello" for 10.5 seconds.
        """
        stripped = user_input.strip()
        if len(stripped) < 20 and self.trivial_pattern.match(stripped):
            logger.debug("Trivial input detected: '%s'", stripped)
            return True
        return False

    # =========================================================================
    # Robust LLM Generation (with empty-fallback)
    # =========================================================================
    def generate(self, model: str, prompt: str, timeout: int = 10,
                 temperature: float = 0.7) -> str:
        """
        Single Ollama generation call with:
          - Configurable timeout
          - Empty response detection → placeholder
          - Exception capture → error string (no crash)
        """
        logger.debug("generate() → model=%s, timeout=%ds, temp=%.1f",
                      model, timeout, temperature)
        start = time.time()
        try:
            resp = requests.post(
                f"{self.ollama}/api/generate",
                json={
                    "model": model,
                    "prompt": prompt,
                    "stream": False,
                    "options": {"temperature": temperature}
                },
                timeout=timeout
            )
            resp.raise_for_status()
            data = resp.json()
            text = data.get("response", "").strip()
            latency = int((time.time() - start) * 1000)
            logger.info("generate() complete — %dms, %d chars",
                        latency, len(text))

            if not text:
                logger.warning("Empty response from model '%s'", model)
                return "[No response generated]"
            return text

        except requests.exceptions.Timeout:
            logger.error("Timeout after %ds for model '%s'", timeout, model)
            return f"[Error: Timeout after {timeout}s]"
        except requests.exceptions.ConnectionError:
            logger.error("Connection refused — is Ollama running at %s?",
                         self.ollama)
            return f"[Error: Cannot connect to {self.ollama}]"
        except Exception as e:
            logger.error("Unexpected error in generate(): %s", str(e))
            return f"[Error: {str(e)}]"

    # =========================================================================
    # Step 1: Critic (context-aware — gentle for trivial inputs)
    # =========================================================================
    def critic_step(self, user_input: str,
                    model: str = "bigdaddyg-fast:latest") -> Dict:
        """
        Context-aware Critic:
          - Trivial inputs → warm greeting (no over-analysis)
          - Substantive inputs → concise flaw analysis (max 3 concerns)
          - Temperature lowered to 0.4 to prevent hallucinated importance
        """
        logger.info("Critic step — input: '%.60s...'", user_input)
        is_triv = self.is_trivial(user_input)

        if is_triv:
            prompt = (
                f'You are a friendly assistant. The user said: "{user_input}"\n'
                f'Respond with a brief, warm greeting. One sentence only.'
            )
        else:
            prompt = (
                f'Analyze this request for potential issues: "{user_input}"\n'
                f'List max 3 specific concerns (security, clarity, feasibility). '
                f'Be concise.'
            )

        start = time.time()
        text = self.generate(model, prompt, timeout=15,
                             temperature=self.critic_temperature)
        latency = int((time.time() - start) * 1000)

        if text.startswith("[Error"):
            _metrics["critic_errors"] += 1

        result = {
            "role": "Critic",
            "model": model,
            "content": text,
            "latency_ms": latency,
            "skipped": is_triv,
        }
        logger.info("Critic done — %dms, skipped=%s", latency, is_triv)
        return result

    # =========================================================================
    # Step 2: Auditor (reviews Critic or passes through)
    # =========================================================================
    def auditor_step(self, user_input: str, critic_output: str,
                     model: str = "bigdaddyg-fast:latest") -> Dict:
        """
        Auditor reviews Critic's work:
          - Trivial inputs → immediate passthrough (no blocking call)
          - Failed Critic → acknowledges failure, passes through
          - Normal → asks model to approve or correct
        """
        logger.info("Auditor step — trivial=%s", self.is_trivial(user_input))
        start = time.time()

        if self.is_trivial(user_input):
            # For trivial inputs, Auditor just echoes approval — no LLM call
            content = "Trivial input detected. Passing through without audit."
            latency = 0
        elif critic_output.startswith("[Error") or critic_output.startswith("[No response"):
            # Critic failed — Auditor acknowledges and defers
            content = ("Critic returned an error or empty response. "
                       "Proceeding with direct synthesis.")
            latency = int((time.time() - start) * 1000)
            _metrics["auditor_errors"] += 1
            logger.warning("Auditor: Critic output was error/empty, skipping audit")
        else:
            prompt = (
                f'Review this analysis for correctness: "{critic_output}"\n'
                f'If valid, say "APPROVED". If flawed, correct it in one sentence.'
            )
            content = self.generate(model, prompt, timeout=10)
            latency = int((time.time() - start) * 1000)

            if content.startswith("[Error"):
                _metrics["auditor_errors"] += 1

        # Guarantee non-empty content
        if not content or not content.strip():
            content = "Audit complete."
            logger.warning("Auditor produced empty content — using fallback")

        result = {
            "role": "Auditor",
            "model": model,
            "content": content,
            "latency_ms": latency,
        }
        logger.info("Auditor done — %dms", latency)
        return result

    # =========================================================================
    # Step 3: Synthesizer (fault-tolerant aggregation)
    # =========================================================================
    def synthesize(self, user_input: str, steps: List[Dict]) -> str:
        """
        Final aggregation — guarantees non-empty output:
          1. Trivial → return Critic greeting directly (skip synthesis)
          2. Build context from non-error steps
          3. If all steps failed → fallback echo
          4. Otherwise → LLM synthesis with full context
        """
        logger.info("Synthesize — %d steps available", len(steps))

        # Fast path: trivial input → Critic already has the greeting
        if steps and steps[0].get("skipped"):
            logger.info("Trivial fast-path: returning Critic greeting directly")
            return steps[0]["content"]

        # Build synthesis context from successful steps only
        context_parts = []
        for s in steps:
            content = s.get("content", "")
            if content and not content.startswith("[Error") and \
               not content.startswith("[No response"):
                context_parts.append(f"{s['role']}: {content}")

        context = "\n".join(context_parts)

        # Fallback: all steps failed or empty
        if not context.strip():
            fallback = f"I received your message: '{user_input}'. How can I help?"
            logger.warning("All steps empty/failed — using fallback echo")
            _metrics["synthesis_errors"] += 1
            return fallback

        # Full synthesis via LLM
        prompt = (
            f"Based on these analyses, provide a helpful final response "
            f"to the user.\n"
            f'User input: "{user_input}"\n'
            f"Analyses:\n{context}\n\n"
            f"Final Answer:"
        )

        result = self.generate("bigdaddyg-fast:latest", prompt, timeout=15,
                               temperature=self.synthesis_temperature)

        if result.startswith("[Error"):
            _metrics["synthesis_errors"] += 1
            # Last-resort fallback
            logger.error("Synthesis LLM call failed — returning best step content")
            return context_parts[0].split(": ", 1)[-1] if context_parts else \
                f"I received your message: '{user_input}'. How can I help?"

        return result

    # =========================================================================
    # Full Pipeline
    # =========================================================================
    def process(self, user_input: str) -> Dict:
        """
        Full CoT pipeline:
          1. Critic (always runs — gentle for trivial)
          2. Auditor (reviews Critic or passthrough)
          3. Synthesis (aggregates with fallback guarantee)

        Returns structured result with steps, timing, and trivial flag.
        """
        logger.info("=" * 60)
        logger.info("CoT process() — input: '%.80s'", user_input)
        _metrics["total_requests"] += 1
        start_total = time.time()
        steps = []

        # --- Step 1: Critic (always runs) ---
        critic = self.critic_step(user_input)
        steps.append(critic)

        # --- Step 2: Auditor ---
        auditor = self.auditor_step(user_input, critic["content"])
        steps.append(auditor)

        # --- Step 3: Synthesis ---
        final = self.synthesize(user_input, steps)

        total_ms = int((time.time() - start_total) * 1000)
        is_triv = self.is_trivial(user_input)

        if is_triv:
            _metrics["trivial_bypassed"] += 1
        else:
            _metrics["full_chain_runs"] += 1
        _metrics["total_latency_ms"] += total_ms

        result = {
            "steps": steps,
            "final_answer": final,
            "total_ms": total_ms,
            "trivial": is_triv,
        }

        logger.info("CoT complete — %dms, trivial=%s, answer_len=%d",
                     total_ms, is_triv, len(final))
        logger.info("=" * 60)
        return result


# =============================================================================
# Flask/FastAPI Endpoint Integration
# =============================================================================
def create_flask_app(ollama_url: str = "http://localhost:11434"):
    """
    Factory for Flask app with /api/cot endpoint.
    Usage:
        app = create_flask_app()
        app.run(host='0.0.0.0', port=5000)
    """
    from flask import Flask, request, jsonify

    app = Flask(__name__)
    engine = RawrXDCotEngine(ollama_url)

    @app.route('/api/cot', methods=['POST'])
    def cot_endpoint():
        data = request.json or {}
        user_input = data.get('message', '')
        if not user_input.strip():
            return jsonify({"error": "Empty message"}), 400
        result = engine.process(user_input)
        return jsonify(result)

    @app.route('/api/cot/metrics', methods=['GET'])
    def metrics_endpoint():
        return jsonify(get_metrics())

    @app.route('/api/cot/health', methods=['GET'])
    def health_endpoint():
        try:
            resp = requests.get(f"{ollama_url}/api/version", timeout=3)
            ollama_ok = resp.status_code == 200
        except Exception:
            ollama_ok = False
        return jsonify({
            "status": "ok" if ollama_ok else "degraded",
            "ollama_connected": ollama_ok,
            "engine": "RawrXDCotEngine",
            "version": "2.0.0-smart-router",
        })

    return app


# =============================================================================
# CLI Smoke Test
# =============================================================================
if __name__ == "__main__":
    import sys

    engine = RawrXDCotEngine()

    # Quick smoke tests
    test_inputs = [
        "Hello",           # → trivial, ~500ms
        "Hi there!",       # → trivial
        "Review this code for buffer overflow vulnerabilities",  # → full chain
    ]

    if len(sys.argv) > 1:
        test_inputs = [" ".join(sys.argv[1:])]

    for inp in test_inputs:
        print(f"\n{'='*60}")
        print(f"INPUT: {inp}")
        print(f"{'='*60}")
        result = engine.process(inp)
        print(f"TRIVIAL: {result['trivial']}")
        print(f"TOTAL:   {result['total_ms']}ms")
        for step in result['steps']:
            print(f"  [{step['role']}] ({step['latency_ms']}ms) "
                  f"{'[SKIPPED]' if step.get('skipped') else ''}")
            print(f"    {step['content'][:120]}...")
        print(f"FINAL:   {result['final_answer'][:200]}")

    print(f"\nMETRICS: {json.dumps(get_metrics(), indent=2)}")
