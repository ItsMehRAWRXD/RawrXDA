'use strict';
/**
 * Private Puller (No-Key): keyless search + fetch + extract
 * - Search: DuckDuckGo HTML endpoint (no API keys)
 * - Fetch: robots.txt-aware, no external secrets
 * - Modes: offline (summarize locally), online/local (send to your ai_cli.php)
 * - CORS + static UI
 */
const http = require('http');
const {URL} = require('url');
const crypto = require('crypto');
const fs = require('fs');
const fsp = require('fs/promises');
const path = require('path');

// ---- Config
const PORT = process.env.PORT || 8787;
const UI_DIR = path.join(__dirname, 'public');
const USER_AGENT = 'PrivatePull/1.0 (+local; keyless; contact: none)';
const MAX_BODY = 256 * 1024;
const RATE_QPS = 1;             // polite default
const TIMEOUT_MS = 12_000;

// Optional: path to your PHP CLI to summarize when mode=online/local
const PHP = process.env.PHP || 'php';
const AI_CLI = process.env.AI_CLI || path.join(__dirname, 'ultra_turbo_ai_cli.php');
const LOCAL_AI_API = process.env.LOCAL_AI_API || 'http://127.0.0.1:8080/api';

// ---- Token bucket rate limiter
let tokens = RATE_QPS;
setInterval(()=> { tokens = Math.min(RATE_QPS, tokens + 1); }, 1000);
const take = ()=> tokens>0 ? (tokens--, true) : false;

// ---- Simple helpers
function json(res, code, obj){
  const s = JSON.stringify(obj);
  res.writeHead(code, {'content-type':'application/json', 'access-control-allow-origin':'*'});
  res.end(s);
}
function notFound(res){ res.writeHead(404); res.end('not found'); }
function serveStatic(req,res){
  const u = new URL(req.url, 'http://x');
  const file = u.pathname === '/' ? 'index.html' : u.pathname.slice(1);
  const p = path.join(UI_DIR, file);
  if (!p.startsWith(UI_DIR)) return notFound(res);
  fs.readFile(p, (e,b)=>{
    if(e) return notFound(res);
    const ext = path.extname(p).slice(1);
    const type = ext==='html' ? 'text/html' : ext==='js' ? 'text/javascript'
      : ext==='css' ? 'text/css' : 'text/plain';
    res.writeHead(200, {'content-type': type});
    res.end(b);
  });
}
function readBody(req){
  return new Promise((resolve,reject)=>{
    const bufs = [];
    let n=0;
    req.on('data', d => { n+=d.length; if(n>MAX_BODY){ reject(new Error('payload too big')); req.destroy(); } else bufs.push(d); });
    req.on('end', ()=> resolve(Buffer.concat(bufs).toString('utf8')));
    req.on('error', reject);
  });
}

// ---- robots.txt (very minimal)
const robotsCache = new Map();
async function allowedByRobots(targetUrl){
  try{
    const u = new URL(targetUrl);
    const key = `${u.protocol}//${u.host}`;
    const cached = robotsCache.get(key);
    if (cached && (Date.now()-cached.t) < 60_000) return cached.ok; // 60s
    const robotsUrl = `${u.protocol}//${u.host}/robots.txt`;
    const txt = await fetchText(robotsUrl);
    let ok = true;
    if (txt && /Disallow:/i.test(txt)) {
      // naive: if site disallows everything to all, deny; else allow
      if (/User-agent:\s*\*/i.test(txt) && /Disallow:\s*\/\s*$/mi.test(txt)) ok = false;
    }
    robotsCache.set(key, {ok, t: Date.now()});
    return ok;
  } catch { return true; }
}

// ---- Fetch utils
async function fetchText(url, opts = {}){
  const ctrl = new AbortController();
  const t = setTimeout(()=>ctrl.abort(), opts.timeout || TIMEOUT_MS);
  try{
    const res = await fetch(url, { headers: {'user-agent': USER_AGENT}, signal: ctrl.signal });
    if (!res.ok) throw new Error(`HTTP ${res.status}`);
    const ct = res.headers.get('content-type') || '';
    if (!/text\/|application\/(json|xml|xhtml)/i.test(ct)) throw new Error(`unsupported content-type: ${ct}`);
    const txt = await res.text();
    return txt;
  } finally { clearTimeout(t); }
}

// ---- Keyless search (DuckDuckGo HTML)
async function ddgSearch(q, n=5){
  // Respect rate limit
  if (!take()) { await new Promise(r=>setTimeout(r,1000)); }
  const u = 'https://duckduckgo.com/html/?q=' + encodeURIComponent(q);
  const html = await fetchText(u);
  // very light parser
  const re = /<a[^>]+class="result__a"[^>]*href="([^"]+)"/gi;
  const out = [];
  let m;
  while ((m = re.exec(html)) && out.length < n) {
    let href = m[1];
    // DDG may wrap URLs with "/l/?kh=-1&uddg=<encoded>"
    const uddg = href.match(/[?&]uddg=([^&]+)/);
    if (uddg) href = decodeURIComponent(uddg[1]);
    out.push({ url: href });
  }
  return out;
}

// ---- Readability-lite extractor
function extractReadable(html){
  // crude: strip scripts/styles, keep text & links
  const text = html
    .replace(/<script[\s\S]*?<\/script>/gi,'')
    .replace(/<style[\s\S]*?<\/style>/gi,'')
    .replace(/<\/(p|div|br|li|h[1-6])>/gi,'$&\n')
    .replace(/<[^>]+>/g,'')
    .replace(/\n{3,}/g,'\n\n')
    .trim();
  const title = (html.match(/<title[^>]*>([\s\S]*?)<\/title>/i) || [,''])[1].trim();
  const links = [];
  const are = /<a[^>]+href="([^"]+)"[^>]*>([\s\S]*?)<\/a>/gi;
  let m;
  while ((m = are.exec(html)) && links.length<100) {
    const href = m[1];
    const label = m[2].replace(/<[^>]+>/g,'').trim();
    if (!href.startsWith('javascript:')) links.push({ href, label });
  }
  return { title, text, links };
}

// ---- Offline summarizer (no model): simple extractive
function summarize(text, sentences=5){
  const lines = text.split(/\.\s+/).filter(Boolean).slice(0, 200);
  const freq = new Map();
  for (const w of text.toLowerCase().match(/\b[a-z]{3,}\b/g)||[]) {
    freq.set(w, (freq.get(w)||0)+1);
  }
  const scored = lines.map((s,i)=>{
    let score=0;
    for (const w of s.toLowerCase().match(/\b[a-z]{3,}\b/g)||[]) score += (freq.get(w)||0);
    // small position bias
    score += Math.max(0, (lines.length - i)/lines.length);
    return { s, score };
  });
  scored.sort((a,b)=>b.score-a.score);
  return scored.slice(0, sentences).map(x=>x.s).join('. ') + '.';
}

// ---- Optional: call your PHP CLI to summarize (online/local)
const { spawn } = require('child_process');
function callAiCliSummarize(prompt, mode){
  return new Promise((resolve,reject)=>{
    const args = [AI_CLI, '--ask', prompt, '--mode', mode, '--timeout', '30'];
    if (mode !== 'offline') args.push('--env'); // let your CLI pick key/local
    const child = spawn(PHP, args, { stdio: ['ignore','pipe','pipe'] });
    let out='', err='';
    child.stdout.on('data', d=> out+=d.toString());
    child.stderr.on('data', d=> err+=d.toString());
    child.on('close', code => code===0 ? resolve(out.trim()) : reject(new Error(err || `exit ${code}`)));
  });
}

// ---- HTTP server
const server = http.createServer(async (req,res)=>{
  // CORS preflight
  if (req.method==='OPTIONS') {
    res.writeHead(204, {
      'access-control-allow-origin':'*',
      'access-control-allow-methods':'GET,POST,OPTIONS',
      'access-control-allow-headers':'content-type'
    }); return res.end();
  }

  try{
    if (req.method==='GET') return serveStatic(req,res);

    if (req.method==='POST' && req.url==='/search') {
      const body = JSON.parse(await readBody(req));
      const q = String(body.q||'').trim();
      const n = Math.max(1, Math.min(10, Number(body.n)||5));
      const mode = (body.mode||'offline').toLowerCase();
      if (!q) return json(res, 400, { error:'missing q' });

      const results = await ddgSearch(q, n);
      // If mode != offline: fetch first result and summarize via ai_cli
      if (mode==='online' || mode==='local') {
        if (results[0]?.url && await allowedByRobots(results[0].url)) {
          const html = await fetchText(results[0].url);
          const { title, text } = extractReadable(html);
          const prompt = `Summarize the following content for the query "${q}" in 6 sentences:\n\n${text.slice(0, 8000)}`;
          try {
            const ai = await callAiCliSummarize(prompt, mode);
            return json(res, 200, { query:q, mode, results, ai_summary: ai, title, source: results[0].url });
          } catch (e) {
            // fall back to offline summarizer
            return json(res, 200, { query:q, mode:'offline-fallback', results, summary: summarize(text), title, source: results[0].url });
          }
        }
      }
      // offline: just return search results
      return json(res, 200, { query:q, mode:'offline', results });
    }

    if (req.method==='POST' && req.url==='/fetch') {
      const body = JSON.parse(await readBody(req));
      const url = String(body.url||'').trim();
      const mode = (body.mode||'offline').toLowerCase();
      if (!url) return json(res, 400, { error:'missing url' });
      if (!await allowedByRobots(url)) return json(res, 403, { error:'blocked by robots.txt' });
      if (!take()) await new Promise(r=>setTimeout(r, 1000));

      const html = await fetchText(url);
      const { title, text, links } = extractReadable(html);

      if (mode==='online' || mode==='local') {
        const prompt = `Summarize this page in 8 bullets, then list 5 key facts:\n\n${text.slice(0, 8000)}`;
        try {
          const ai = await callAiCliSummarize(prompt, mode);
          return json(res, 200, { url, title, summary: ai, links: links.slice(0,50) });
        } catch (e) {
          // fall back to offline summary
          return json(res, 200, { url, title, summary: summarize(text), links: links.slice(0,50) });
        }
      } else {
        // pure offline
        return json(res, 200, { url, title, text: text.slice(0, 20_000), links: links.slice(0,100) });
      }
    }

    return notFound(res);
  } catch (e) {
    return json(res, 500, { error:String(e.message||e) });
  }
});

server.listen(PORT, ()=> console.log(`[puller] listening on http://127.0.0.1:${PORT}`));
