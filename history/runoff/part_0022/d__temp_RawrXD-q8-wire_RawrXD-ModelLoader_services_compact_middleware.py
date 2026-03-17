#!/usr/bin/env python3
"""
compact_middleware.py — FastAPI middleware for gzip + JSON minification
Phase 5.5: Server-side compact wire protocol
Target: Auto-compress responses when X-Compact: 1 header present
"""

import gzip
import orjson
from fastapi import Request, Response
from starlette.middleware.base import BaseHTTPMiddleware
from starlette.types import ASGIApp

class CompactWireMiddleware(BaseHTTPMiddleware):
    """Automatically compress/decompress JSON payloads with gzip"""
    
    async def dispatch(self, request: Request, call_next):
        # Decompress incoming request if X-Compact header present
        if request.headers.get("X-Compact") == "1":
            body = await request.body()
            if body:
                try:
                    decompressed = gzip.decompress(body)
                    # Re-create request with decompressed body
                    async def receive():
                        return {"type": "http.request", "body": decompressed}
                    request._receive = receive
                except Exception as e:
                    print(f"⚠️ Decompression failed: {e}")
        
        # Process request
        response = await call_next(request)
        
        # Compress response if client wants compact format
        if request.headers.get("X-Compact") == "1":
            # Read response body
            body = b""
            async for chunk in response.body_iterator:
                body += chunk
            
            # Compress with gzip
            try:
                compressed = gzip.compress(body, compresslevel=9)
                return Response(
                    content=compressed,
                    status_code=response.status_code,
                    headers={
                        **dict(response.headers),
                        "Content-Encoding": "gzip",
                        "X-Compact": "1",
                        "Content-Length": str(len(compressed))
                    },
                    media_type=response.media_type
                )
            except Exception as e:
                print(f"⚠️ Compression failed: {e}")
                return response
        
        return response


# Usage in FastAPI app:
# from fastapi import FastAPI
# from compact_middleware import CompactWireMiddleware
# 
# app = FastAPI()
# app.add_middleware(CompactWireMiddleware)
