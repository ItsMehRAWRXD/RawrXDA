#!/usr/bin/env python3
"""
RawrEngine — HTTP API for RawrXD Universal Access
Provides /status, /v1/models, /api/chat, /api/tools, /api/agent/wish endpoints
for the web interface. Integrate with CORSAuthMiddleware for CORS and auth.
"""

import json
import os
import sys

# Optional Flask; fallback to stdlib if not available
try:
    from flask import Flask, request, Response, jsonify
    HAS_FLASK = True
except ImportError:
    HAS_FLASK = False

HOST = os.environ.get("RAWRXD_HOST", "0.0.0.0")
PORT = int(os.environ.get("RAWRXD_PORT", "23959"))


def create_app():
    if not HAS_FLASK:
        raise ImportError("Flask required. Install with: pip install flask")

    app = Flask(__name__)

    # Apply CORS & Auth middleware
    try:
        try:
            from Ship.CORSAuthMiddleware import UniversalAccessGateway
        except ImportError:
            from CORSAuthMiddleware import UniversalAccessGateway

        cors = os.environ.get("RAWRXD_CORS_ORIGINS")
        gateway = UniversalAccessGateway(
            allowed_origins=[x.strip() for x in cors.split(",")] if cors else None,
            require_auth=os.environ.get("RAWRXD_REQUIRE_AUTH", "").lower() == "true",
        )
        gateway.init_app(app)
    except ImportError:
        pass  # Run without middleware if not in Ship context

    @app.route("/status", methods=["GET"])
    def status():
        return jsonify(
            {"status": "ok", "service": "RawrEngine", "port": PORT}
        )

    @app.route("/v1/models", methods=["GET"])
    def list_models():
        # Stub: return default model; integrate with actual model loader
        models = [
            {"id": "rawrxd-default", "object": "model"},
        ]
        model_path = os.environ.get("RAWRXD_MODEL_PATH", "")
        if model_path and os.path.isdir(model_path):
            for f in os.listdir(model_path)[:10]:
                if f.endswith(".gguf"):
                    models.append({"id": f.replace(".gguf", ""), "object": "model"})
        return jsonify({"models": models, "object": "list"})

    @app.route("/api/tools", methods=["GET"])
    def list_tools():
        return jsonify(
            {
                "tools": [
                    {"id": "read_file", "name": "Read File"},
                    {"id": "write_file", "name": "Write File"},
                    {"id": "run_terminal", "name": "Run Terminal"},
                ]
            }
        )

    @app.route("/api/chat", methods=["POST"])
    def chat():
        data = request.get_json() or {}
        messages = data.get("messages", [])
        stream = data.get("stream", False)
        model = data.get("model", "rawrxd-default")

        def generate():
            # Stub response; wire to actual inference
            text = "Hello from RawrEngine. Connect your inference backend for full responses."
            if stream:
                for chunk in [text[i : i + 5] for i in range(0, len(text), 5)]:
                    yield f"data: {json.dumps({'choices': [{'delta': {'content': chunk}}]})}\n\n"
                yield "data: [DONE]\n\n"
            else:
                yield json.dumps(
                    {"choices": [{"message": {"content": text}}]}
                )

        if stream:
            return Response(
                generate(),
                mimetype="text/event-stream",
                headers={"Cache-Control": "no-cache"},
            )
        return jsonify(
            {"choices": [{"message": {"content": "Hello from RawrEngine."}}]}
        )

    @app.route("/api/agent/wish", methods=["POST"])
    def agent_wish():
        data = request.get_json() or {}
        wish = data.get("wish", "")
        mode = data.get("mode", "ask")
        auto_execute = data.get("auto_execute", False)

        if mode == "plan":
            return jsonify(
                {
                    "plan": [
                        "Analyze request",
                        "Create implementation plan",
                        "Execute changes",
                    ],
                    "requires_confirmation": not auto_execute,
                }
            )
        return jsonify(
            {
                "response": f"Received wish: {wish}. Connect inference for execution.",
                "tool_calls": [],
            }
        )

    @app.route("/api/agentic/config", methods=["POST"])
    def agentic_config():
        data = request.get_json() or {}
        model = data.get("model", "")
        return jsonify({"ok": True, "model": model})

    return app


def main():
    app = create_app()
    print(f"RawrEngine starting on {HOST}:{PORT}")
    app.run(host=HOST, port=PORT, threaded=True)


if __name__ == "__main__":
    main()
