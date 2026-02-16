#!/usr/bin/env python3
"""
RawrXD CORS & Auth Middleware — Universal Access Gateway
Adds cross-origin support and API key validation to RawrEngine
"""

import secrets
from typing import Optional
from flask import Flask, request, jsonify, make_response, Response, g
import logging

logger = logging.getLogger(__name__)


class UniversalAccessGateway:
    """Zero-dependency CORS and Auth layer for RawrEngine HTTP API"""

    def __init__(
        self,
        allowed_origins: list = None,
        api_keys: set = None,
        require_auth: bool = False,
    ):
        self.allowed_origins = allowed_origins or [
            "http://localhost",
            "http://127.0.0.1",
            "https://rawrxd.local",
            "app://rawrxd",
        ]
        self.api_keys = api_keys or set()
        self.require_auth = require_auth
        self.request_count = 0
        self.active_sessions = {}

    def init_app(self, app: Flask):
        """Register with Flask application"""

        @app.before_request
        def handle_cors_preflight():
            """Handle CORS preflight and validation"""
            origin = request.headers.get("Origin", "")

            # Always allow localhost for local IDE
            if "localhost" in origin or "127.0.0.1" in origin:
                allowed = True
            else:
                allowed = any(o in origin for o in self.allowed_origins)

            if request.method == "OPTIONS":
                response = make_response()
                response.headers.add(
                    "Access-Control-Allow-Origin",
                    origin if allowed else "",
                )
                response.headers.add(
                    "Access-Control-Allow-Headers",
                    "Content-Type, X-API-Key, Authorization",
                )
                response.headers.add(
                    "Access-Control-Allow-Methods",
                    "GET, POST, PUT, DELETE, OPTIONS",
                )
                response.headers.add("Access-Control-Max-Age", "86400")
                response.headers.add("Access-Control-Allow-Credentials", "true")
                return response

            if allowed:
                g.cors_origin = origin

        @app.after_request
        def add_cors_headers(response: Response) -> Response:
            """Add CORS headers to response when origin is allowed"""
            origin = g.get("cors_origin")
            if origin:
                response.headers.add("Access-Control-Allow-Origin", origin)
                response.headers.add("Access-Control-Allow-Credentials", "true")
                response.headers.add("Vary", "Origin")
            return response

        @app.before_request
        def validate_auth():
            """API Key validation for external access"""
            if (
                not self.require_auth
                and request.remote_addr in ("127.0.0.1", "localhost")
            ):
                return  # Skip auth for local IDE

            if request.endpoint == "status":
                return  # Status endpoint always public

            api_key = request.headers.get("X-API-Key") or request.headers.get(
                "Authorization", ""
            ).replace("Bearer ", "")

            if self.require_auth and not api_key:
                return jsonify({"error": "API key required"}), 401

            if api_key and api_key not in self.api_keys:
                logger.warning(f"Invalid API key attempt from {request.remote_addr}")
                return jsonify({"error": "Invalid API key"}), 403

            if api_key:
                self.active_sessions[api_key] = {
                    "ip": request.remote_addr,
                    "user_agent": request.user_agent.string,
                }

    def generate_api_key(self, tier: str = "standard") -> str:
        """Generate new API key for external user"""
        key = f"rawrxd_{tier}_{secrets.token_urlsafe(32)}"
        self.api_keys.add(key)
        return key

    def revoke_key(self, key: str):
        """Revoke API key"""
        self.api_keys.discard(key)
        self.active_sessions.pop(key, None)


# FastAPI/Starlette equivalent for async RawrEngine
class AsyncGateway:
    """ASGI middleware for async backends"""

    def __init__(self, app, allowed_origins=None, api_keys=None):
        self.app = app
        self.allowed_origins = allowed_origins or ["*"]
        self.api_keys = api_keys or set()

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http":
            await self.app(scope, receive, send)
            return

        # Handle CORS
        headers = [(b"access-control-allow-origin", b"*")]

        async def wrapped_send(message):
            if message["type"] == "http.response.start":
                message["headers"].extend(headers)
            await send(message)

        await self.app(scope, receive, wrapped_send)


# CLI Key generator
if __name__ == "__main__":
    import sys

    gateway = UniversalAccessGateway()

    if len(sys.argv) > 1 and sys.argv[1] == "genkey":
        tier = sys.argv[2] if len(sys.argv) > 2 else "standard"
        print(f"Generated API Key: {gateway.generate_api_key(tier)}")
    else:
        print("Usage: python CORSAuthMiddleware.py genkey [tier]")
