from fastapi import FastAPI, Request, Response
from starlette.types import Receive
import gzip
import orjson

app = FastAPI()

def _make_receive(decompressed: bytes) -> Receive:
    async def _receive() -> dict:
        return {"type": "http.request", "body": decompressed, "more_body": False}
    return _receive

@app.middleware("http")
async def compact_middleware(request: Request, call_next):
    compact_req = request.headers.get("X-Compact") == "1"
    if compact_req:
        body = await request.body()
        try:
            body = gzip.decompress(body)
        except Exception:
            pass
        request = Request(request.scope, _make_receive(body))

    response = await call_next(request)

    if compact_req:
        # Drain response body (supports StreamingResponse)
        body_bytes = b""
        async for chunk in response.body_iterator:
            body_bytes += chunk
        # If JSON-like, re-serialize compact; else compress raw
        try:
            obj = orjson.loads(body_bytes)
            body_bytes = orjson.dumps(obj)
        except Exception:
            pass
        gz = gzip.compress(body_bytes)
        headers = dict(response.headers)
        headers["Content-Encoding"] = "gzip"
        headers["X-Compact"] = "1"
        return Response(content=gz, status_code=response.status_code, headers=headers, media_type=response.media_type)

    return response

# Minimal echo route for manual testing
@app.post("/echo")
async def echo(request: Request):
    data = await request.json()
    return {"ok": True, "received": data}
