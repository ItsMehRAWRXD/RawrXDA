#!/usr/bin/env python3
"""
RawrXD CORS & Auth Middleware - Universal Access Gateway.
Adds cross-origin support and API key validation to RawrEngine.
"""

from __future__ import annotations

import argparse
import ipaddress
import json
import logging
import os
import secrets
import time
from threading import Lock
from typing import Iterable, Optional
from urllib.parse import urlparse

from flask import Flask, Response, jsonify, make_response, request

logger = logging.getLogger(__name__)


def _as_bool(value: str) -> bool:
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _split_env(value: str) -> list[str]:
    return [item.strip() for item in value.split(",") if item.strip()]


def _normalize_origin(origin: str) -> str:
    return origin.strip().rstrip("/").lower()


def _is_loopback_address(remote_addr: Optional[str]) -> bool:
    if not remote_addr:
        return False
    if remote_addr == "localhost":
        return True
    try:
        return ipaddress.ip_address(remote_addr).is_loopback
    except ValueError:
        return False


class UniversalAccessGateway:
    """Zero-dependency CORS and auth layer for RawrEngine HTTP API."""

    DEFAULT_ORIGINS = [
        "http://localhost",
        "http://127.0.0.1",
        "https://rawrxd.local",
        "app://rawrxd",
    ]
    DEFAULT_PUBLIC_PATHS = {"/status", "/health", "/healthz"}
    ALLOW_HEADERS = "Content-Type, X-API-Key, Authorization"
    ALLOW_METHODS = "GET, POST, PUT, PATCH, DELETE, OPTIONS"

    def __init__(
        self,
        allowed_origins: Optional[Iterable[str]] = None,
        api_keys: Optional[Iterable[str]] = None,
        require_auth: bool = False,
        public_paths: Optional[Iterable[str]] = None,
    ) -> None:
        origins = allowed_origins if allowed_origins is not None else self.DEFAULT_ORIGINS
        self.allowed_origins = {_normalize_origin(origin) for origin in origins}
        self.allow_all_origins = "*" in self.allowed_origins
        self.api_keys = {key.strip() for key in (api_keys or set()) if key and key.strip()}
        self.require_auth = require_auth
        self.public_paths = set(public_paths or self.DEFAULT_PUBLIC_PATHS)

        self.request_count = 0
        self.active_sessions: dict[str, dict[str, str | float]] = {}
        self._state_lock = Lock()

    @classmethod
    def from_environment(cls) -> "UniversalAccessGateway":
        """Build gateway from environment variables."""
        env_origins = _split_env(os.getenv("RAWRXD_CORS_ORIGINS", ""))
        env_keys = _split_env(os.getenv("RAWRXD_API_KEYS", ""))
        env_require_auth = _as_bool(os.getenv("RAWRXD_REQUIRE_AUTH", "0"))
        return cls(
            allowed_origins=env_origins or None,
            api_keys=set(env_keys),
            require_auth=env_require_auth,
        )

    def init_app(self, app: Flask) -> None:
        """Register middleware hooks with Flask app."""

        @app.before_request
        def _before_request():
            with self._state_lock:
                self.request_count += 1

            origin = request.headers.get("Origin", "")
            if request.method == "OPTIONS":
                return self._build_preflight_response(origin)

            return self._validate_auth()

        @app.after_request
        def _after_request(response: Response) -> Response:
            origin = request.headers.get("Origin", "")
            return self._add_cors_headers(response, origin)

    def _validate_auth(self):
        if request.path in self.public_paths:
            return None

        if not self.require_auth and _is_loopback_address(request.remote_addr):
            return None

        api_key = self._extract_api_key(
            request.headers.get("X-API-Key"),
            request.headers.get("Authorization"),
        )

        if self.require_auth and not self.api_keys:
            logger.error("Auth is required but no API keys are configured")
            return jsonify({"error": "Server misconfigured: no API keys available"}), 503

        if self.require_auth and not api_key:
            return jsonify({"error": "API key required"}), 401

        if api_key and api_key not in self.api_keys:
            logger.warning("Invalid API key attempt from %s", request.remote_addr)
            return jsonify({"error": "Invalid API key"}), 403

        if api_key:
            with self._state_lock:
                self.active_sessions[api_key] = {
                    "ip": request.remote_addr or "",
                    "user_agent": request.user_agent.string,
                    "last_seen": time.time(),
                }
        return None

    def _extract_api_key(self, header_key: Optional[str], auth_header: Optional[str]) -> Optional[str]:
        if header_key:
            return header_key.strip()
        if auth_header:
            stripped = auth_header.strip()
            if stripped.lower().startswith("bearer "):
                return stripped[7:].strip()
            return stripped
        return None

    def _is_origin_allowed(self, origin: str) -> bool:
        if not origin:
            return False
        if self.allow_all_origins:
            return True

        normalized = _normalize_origin(origin)
        if normalized in self.allowed_origins:
            return True

        parsed = urlparse(normalized)
        if parsed.hostname in {"localhost", "127.0.0.1", "::1"}:
            return True
        return False

    def _build_preflight_response(self, origin: str) -> Response:
        response = make_response("", 204)
        if self._is_origin_allowed(origin):
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Access-Control-Allow-Credentials"] = "true"
        response.headers["Access-Control-Allow-Headers"] = self.ALLOW_HEADERS
        response.headers["Access-Control-Allow-Methods"] = self.ALLOW_METHODS
        response.headers["Access-Control-Max-Age"] = "86400"
        response.headers["Vary"] = "Origin"
        return response

    def _add_cors_headers(self, response: Response, origin: str) -> Response:
        if self._is_origin_allowed(origin):
            response.headers["Access-Control-Allow-Origin"] = origin
            response.headers["Access-Control-Allow-Credentials"] = "true"
            response.headers["Vary"] = "Origin"
        return response

    def generate_api_key(self, tier: str = "standard") -> str:
        """Generate a new API key."""
        key = f"rawrxd_{tier}_{secrets.token_urlsafe(32)}"
        with self._state_lock:
            self.api_keys.add(key)
        return key

    def revoke_key(self, key: str) -> None:
        """Revoke a key and any active session associated with it."""
        with self._state_lock:
            self.api_keys.discard(key)
            self.active_sessions.pop(key, None)


class AsyncGateway:
    """ASGI middleware for async backends."""

    ALLOW_HEADERS = b"Content-Type, X-API-Key, Authorization"
    ALLOW_METHODS = b"GET, POST, PUT, PATCH, DELETE, OPTIONS"

    def __init__(
        self,
        app,
        allowed_origins: Optional[Iterable[str]] = None,
        api_keys: Optional[Iterable[str]] = None,
        require_auth: bool = False,
        public_paths: Optional[Iterable[str]] = None,
    ):
        self.app = app
        origins = allowed_origins if allowed_origins is not None else UniversalAccessGateway.DEFAULT_ORIGINS
        self.allowed_origins = {_normalize_origin(origin) for origin in origins}
        self.allow_all_origins = "*" in self.allowed_origins
        self.api_keys = {key.strip() for key in (api_keys or set()) if key and key.strip()}
        self.require_auth = require_auth
        self.public_paths = set(public_paths or UniversalAccessGateway.DEFAULT_PUBLIC_PATHS)

    async def __call__(self, scope, receive, send):
        if scope.get("type") != "http":
            await self.app(scope, receive, send)
            return

        headers = self._headers_to_dict(scope.get("headers", []))
        method = scope.get("method", "GET").upper()
        path = scope.get("path", "/")
        origin = headers.get("origin", "")
        remote_addr = (scope.get("client") or ("", 0))[0]

        if method == "OPTIONS":
            allow_origin = origin if self._is_origin_allowed(origin) else ""
            await self._send_response(
                send=send,
                status=204,
                body=b"",
                origin=allow_origin,
                include_preflight=True,
            )
            return

        if path not in self.public_paths and not (not self.require_auth and _is_loopback_address(remote_addr)):
            api_key = self._extract_api_key(headers.get("x-api-key"), headers.get("authorization"))

            if self.require_auth and not self.api_keys:
                payload = json.dumps({"error": "Server misconfigured: no API keys available"}).encode("utf-8")
                await self._send_response(send, 503, payload, origin, content_type=b"application/json")
                return

            if self.require_auth and not api_key:
                payload = json.dumps({"error": "API key required"}).encode("utf-8")
                await self._send_response(send, 401, payload, origin, content_type=b"application/json")
                return

            if api_key and api_key not in self.api_keys:
                payload = json.dumps({"error": "Invalid API key"}).encode("utf-8")
                await self._send_response(send, 403, payload, origin, content_type=b"application/json")
                return

        async def wrapped_send(message):
            if message.get("type") == "http.response.start":
                response_headers = list(message.get("headers", []))
                if self._is_origin_allowed(origin):
                    response_headers.append((b"access-control-allow-origin", origin.encode("utf-8")))
                    response_headers.append((b"access-control-allow-credentials", b"true"))
                    response_headers.append((b"vary", b"Origin"))
                message["headers"] = response_headers
            await send(message)

        await self.app(scope, receive, wrapped_send)

    def _headers_to_dict(self, raw_headers):
        output: dict[str, str] = {}
        for key, value in raw_headers:
            output[key.decode("latin-1").lower()] = value.decode("latin-1")
        return output

    def _is_origin_allowed(self, origin: str) -> bool:
        if not origin:
            return False
        if self.allow_all_origins:
            return True
        normalized = _normalize_origin(origin)
        if normalized in self.allowed_origins:
            return True
        parsed = urlparse(normalized)
        return parsed.hostname in {"localhost", "127.0.0.1", "::1"}

    def _extract_api_key(self, header_key: Optional[str], auth_header: Optional[str]) -> Optional[str]:
        if header_key:
            return header_key.strip()
        if auth_header:
            stripped = auth_header.strip()
            if stripped.lower().startswith("bearer "):
                return stripped[7:].strip()
            return stripped
        return None

    async def _send_response(
        self,
        send,
        status: int,
        body: bytes,
        origin: str,
        *,
        content_type: bytes = b"text/plain; charset=utf-8",
        include_preflight: bool = False,
    ) -> None:
        headers = [
            (b"content-type", content_type),
            (b"content-length", str(len(body)).encode("ascii")),
        ]
        if self._is_origin_allowed(origin):
            headers.extend(
                [
                    (b"access-control-allow-origin", origin.encode("utf-8")),
                    (b"access-control-allow-credentials", b"true"),
                    (b"vary", b"Origin"),
                ]
            )
        if include_preflight:
            headers.extend(
                [
                    (b"access-control-allow-headers", self.ALLOW_HEADERS),
                    (b"access-control-allow-methods", self.ALLOW_METHODS),
                    (b"access-control-max-age", b"86400"),
                ]
            )
        await send({"type": "http.response.start", "status": status, "headers": headers})
        await send({"type": "http.response.body", "body": body})


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="RawrXD CORS/Auth Middleware Utility")
    subparsers = parser.add_subparsers(dest="command", required=True)

    genkey = subparsers.add_parser("genkey", help="Generate a new API key")
    genkey.add_argument("tier", nargs="?", default="standard", help="API key tier label")

    return parser


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()
    gateway = UniversalAccessGateway()

    if args.command == "genkey":
        print(gateway.generate_api_key(args.tier))
        return 0

    parser.print_help()
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
