import { createServer } from "node:http";
import { readFile } from "node:fs/promises";
import { extname, join } from "node:path";

const PORT = Number(process.env.PORT || 4173);
const ROOT = new URL("./public/", import.meta.url);

const MIME = {
  ".html": "text/html; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".json": "application/json; charset=utf-8"
};

function sendJson(res, statusCode, payload) {
  const body = JSON.stringify(payload);
  res.writeHead(statusCode, {
    "Content-Type": "application/json; charset=utf-8",
    "Content-Length": Buffer.byteLength(body)
  });
  res.end(body);
}

const server = createServer(async (req, res) => {
  const method = req.method || "GET";
  const url = req.url || "/";

  if (method === "GET" && url === "/api/health") {
    return sendJson(res, 200, { ok: true, service: "agentic-react-server" });
  }

  if (method === "POST" && url === "/api/echo") {
    let raw = "";
    req.on("data", chunk => {
      raw += chunk;
      if (raw.length > 1024 * 128) {
        req.destroy();
      }
    });
    req.on("end", () => {
      try {
        const parsed = raw ? JSON.parse(raw) : {};
        sendJson(res, 200, { echoed: parsed });
      } catch {
        sendJson(res, 400, { error: "Invalid JSON" });
      }
    });
    return;
  }

  const requestedPath = url === "/" ? "/index.html" : url;
  const safePath = requestedPath.replace(/\.\./g, "");
  const fullPath = new URL(`.${safePath}`, ROOT);

  try {
    const data = await readFile(fullPath);
    const ext = extname(safePath).toLowerCase();
    res.writeHead(200, {
      "Content-Type": MIME[ext] || "application/octet-stream",
      "Content-Length": data.length
    });
    res.end(data);
  } catch {
    sendJson(res, 404, { error: "Not found" });
  }
});

server.listen(PORT, "127.0.0.1", () => {
  console.log(`Agentic React server running at http://127.0.0.1:${PORT}`);
});
