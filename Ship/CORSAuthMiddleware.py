#!/usr/bin/env python3
"""
RawrXD CORS & Auth Middleware — Universal Access Gateway

Production-ready, zero-extra-deps middleware layer for Flask apps:
  - CORS for browser-based Web UI
  - Optional API key validation for external access
  - Safe defaults: always allow localhost origins, keep /status public

Environment variables (optional):
  - RAWRXD_CORS_ORIGINS: comma-separated list of allowed origins (e.g. "https://example.com,http://localhost:8080")
  - RAWRXD_API_KEYS: comma-separated API keys (e.g. "rawrxd_standard_...,rawrxd_admin_...")
  - RAWRXD_REQUIRE_AUTH: "1|true|yes|on" to require a key for non-local requests

Notes:
  - This module does not start a server; it patches an existing Flask app.
  - For ASGI backends, see AsyncGateway below.
"""

from __future__ import annotations

import os
import secrets
import logging
import ipaddress
from dataclasses import dataclass
from typing import Callable, Iterable, Optional, Set, List, Dict, Any

try:
    from flask import Flask, request, jsonify, make_response, Response
except Exception as e:  # pragma: no cover
    Flask = object  # type: ignore
    request = None  # type: ignore
    jsonify = None  # type: ignore
    make_response = None  # type: ignore
    Response = object  # type: ignore

logger = logging.getLogger("RawrXD.UniversalAccess")


def _split_csv(value: str) -> List[str]:
    return [v.strip() for v in (value or "").split(",") if v.strip()]


def _is_truthy(value: Optional[str]) -> bool:
    if not value:
        return False
    return value.strip().lower() in ("1", "true", "yes", "on")


def _is_loopback(remote_addr: Optional[str]) -> bool:
    if not remote_addr:
        return False
    try:
        return ipaddress.ip_address(remote_addr).is_loopback
    except Exception:
        return remote_addr in ("127.0.0.1", "::1", "localhost")


def _normalize_origin(origin: str) -> str:
    return (origin or "").strip()


def _origin_allowed(origin: str, allowed: Iterable[str]) -> bool:
    origin = _normalize_origin(origin)
    if not origin or origin == "null":
        return False
    allowed_list = list(allowed)
    if "*" in allowed_list:
        return True
    if origin in allowed_list:
        return True
    # Always allow localhost origins (any port) for local IDE/dev.
    if origin.startswith("http://localhost") or origin.startswith("http://127.0.0.1"):
        return True
    if origin.startswith("https://localhost") or origin.startswith("https://127.0.0.1"):
        return True
    return False


def _extract_api_key(headers: Dict[str, str]) -> str:
    api_key = (headers.get("X-API-Key") or "").strip()
    if api_key:
        return api_key
    auth = (headers.get("Authorization") or "").strip()
    if auth.lower().startswith("bearer "):
        return auth[7:].strip()
    return auth


@dataclass
class SessionInfo:
    ip: str
    user_agent: str


class UniversalAccessGateway:
    """CORS + API-key auth layer for RawrEngine-compatible Flask APIs."""

    def __init__(
        self,
        allowed_origins: Optional[List[str]] = None,
        api_keys: Optional[Set[str]] = None,
        require_auth: Optional[bool] = None,
    ):
        env_origins = _split_csv(os.getenv("RAWRXD_CORS_ORIGINS", ""))
        env_keys = set(_split_csv(os.getenv("RAWRXD_API_KEYS", "")))
        env_require = _is_truthy(os.getenv("RAWRXD_REQUIRE_AUTH"))

        self.allowed_origins: List[str] = allowed_origins if allowed_origins is not None else (
            env_origins if env_origins else [
                "http://localhost",
                "http://127.0.0.1",
                "https://rawrxd.local",
                "app://rawrxd",
            ]
        )
        self.api_keys: Set[str] = api_keys if api_keys is not None else env_keys
        self.require_auth: bool = require_auth if require_auth is not None else env_require

        self.request_count: int = 0
        self.active_sessions: Dict[str, SessionInfo] = {}

    def init_app(self, app: Flask) -> None:
        """Register middleware hooks with the Flask application."""

        @app.before_request
        def _cors_and_auth_before() -> Optional[Response]:
            self.request_count += 1

            origin = _normalize_origin(request.headers.get("Origin", ""))
            cors_ok = _origin_allowed(origin, self.allowed_origins) if origin else False

            # Handle preflight
            if request.method == "OPTIONS":
                resp = make_response("", 204)
                if cors_ok:
                    self._apply_cors(resp, origin)
                    resp.headers["Access-Control-Allow-Headers"] = "Content-Type, X-API-Key, Authorization"
                    resp.headers["Access-Control-Allow-Methods"] = "GET, POST, PUT, DELETE, OPTIONS"
                    resp.headers["Access-Control-Max-Age"] = "86400"
                return resp

            # Auth (skip for loopback unless explicitly required)
            remote = request.remote_addr or ""
            is_local = _is_loopback(remote)
            is_public = (request.path == "/status")

            api_key = _extract_api_key(dict(request.headers))
            auth_configured = self.require_auth or bool(self.api_keys)

            if auth_configured and (not is_public) and (not is_local):
                if not api_key:
                    return jsonify({"error": "API key required"}), 401
                if api_key not in self.api_keys:
                    logger.warning("Invalid API key from %s", remote)
                    return jsonify({"error": "Invalid API key"}), 403

            if api_key:
                self.active_sessions[api_key] = SessionInfo(
                    ip=remote,
                    user_agent=getattr(request.user_agent, "string", "") or "",
                )

            return None

        @app.after_request
        def _cors_after(resp: Response) -> Response:
            origin = _normalize_origin(request.headers.get("Origin", ""))
            if origin and _origin_allowed(origin, self.allowed_origins):
                self._apply_cors(resp, origin)
            return resp

    def _apply_cors(self, resp: Response, origin: str) -> None:
        resp.headers["Access-Control-Allow-Origin"] = origin
        resp.headers["Access-Control-Allow-Credentials"] = "true"
        resp.headers["Vary"] = "Origin"

    def generate_api_key(self, tier: str = "standard") -> str:
        """Generate a new API key and add it to the in-memory allowlist."""
        key = f"rawrxd_{tier}_{secrets.token_urlsafe(32)}"
        self.api_keys.add(key)
        return key

    def revoke_key(self, key: str) -> None:
        self.api_keys.discard(key)
        self.active_sessions.pop(key, None)


class AsyncGateway:
    """
    ASGI middleware for async backends (Starlette/FastAPI).
    Adds permissive CORS headers; API-key auth can be enforced upstream.
    """

    def __init__(self, app, allowed_origins: Optional[List[str]] = None):
        self.app = app
        self.allowed_origins = allowed_origins or ["*"]

    async def __call__(self, scope, receive, send):
        if scope.get("type") != "http":
            await self.app(scope, receive, send)
            return

        headers = dict(scope.get("headers") or [])
        origin = (headers.get(b"origin") or b"").decode("utf-8", "ignore").strip()
        allow_all = "*" in self.allowed_origins
        cors_ok = allow_all or (origin and origin in self.allowed_origins)

        async def wrapped_send(message):
            if message.get("type") == "http.response.start" and cors_ok:
                h = message.setdefault("headers", [])
                h.extend([
                    (b"access-control-allow-origin", origin.encode("utf-8")),
                    (b"access-control-allow-credentials", b"true"),
                    (b"vary", b"Origin"),
                    (b"access-control-allow-headers", b"Content-Type, X-API-Key, Authorization"),
                    (b"access-control-allow-methods", b"GET, POST, PUT, DELETE, OPTIONS"),
                ])
            await send(message)

        await self.app(scope, receive, wrapped_send)


if __name__ == "__main__":
    import sys

    gw = UniversalAccessGateway()
    if len(sys.argv) > 1 and sys.argv[1] == "genkey":
        tier = sys.argv[2] if len(sys.argv) > 2 else "standard"
        print(gw.generate_api_key(tier))
    else:
        print("Usage: python Ship/CORSAuthMiddleware.py genkey [tier]")

