import json
import sys


def read_exact(stream, size):
    data = b""
    while len(data) < size:
        chunk = stream.read(size - len(data))
        if not chunk:
            return None
        data += chunk
    return data


def read_message():
    headers = {}
    while True:
        line = sys.stdin.buffer.readline()
        if not line:
            return None
        if line in (b"\r\n", b"\n"):
            break
        try:
            key, value = line.decode("ascii", errors="replace").split(":", 1)
        except ValueError:
            continue
        headers[key.strip().lower()] = value.strip()

    length = int(headers.get("content-length", "0"))
    if length <= 0:
        return None

    payload = read_exact(sys.stdin.buffer, length)
    if payload is None:
        return None

    return json.loads(payload.decode("utf-8"))


def write_message(obj):
    payload = json.dumps(obj, separators=(",", ":")).encode("utf-8")
    header = f"Content-Length: {len(payload)}\r\n\r\n".encode("ascii")
    sys.stdout.buffer.write(header)
    sys.stdout.buffer.write(payload)
    sys.stdout.buffer.flush()


def make_tool_list_result():
    return {
        "tools": [
            {
                "name": "echo",
                "description": "Echoes input text",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "text": {"type": "string"}
                    }
                }
            }
        ]
    }


def main():
    while True:
        msg = read_message()
        if msg is None:
            return 0

        method = msg.get("method", "")
        msg_id = msg.get("id")

        if method == "initialize":
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "result": {
                    "protocolVersion": "2024-11-05",
                    "capabilities": {"tools": {}},
                    "serverInfo": {"name": "rawrxd-min-mcp", "version": "1.0"}
                }
            })
            continue

        if method == "notifications/initialized":
            continue

        if method == "tools/list":
            write_message({"jsonrpc": "2.0", "id": msg_id, "result": make_tool_list_result()})
            continue

        if method == "tools/call":
            params = msg.get("params", {})
            tool_name = params.get("name", "")
            args = params.get("arguments", {})
            if tool_name == "echo":
                text = args.get("text", "")
                write_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "result": {
                        "content": [
                            {"type": "text", "text": text}
                        ]
                    }
                })
            else:
                write_message({
                    "jsonrpc": "2.0",
                    "id": msg_id,
                    "error": {"code": -32601, "message": f"unknown tool: {tool_name}"}
                })
            continue

        if msg_id is not None:
            write_message({
                "jsonrpc": "2.0",
                "id": msg_id,
                "error": {"code": -32601, "message": f"unknown method: {method}"}
            })


if __name__ == "__main__":
    raise SystemExit(main())
