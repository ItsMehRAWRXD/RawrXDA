#!/usr/bin/env php
<?php
/**
 * ai.php — editor-agnostic AI assistant (Gemini, single-file, cURL-only)
 * ---------------------------------------------------------------------
 * Usage inside any editor (bind to Ctrl+Enter):
 *   :!ai.php --stdin          " replace current buffer
 *   :'<,'>!ai.php --stdin     " replace selection
 *   :!ai.php --ask "refactor to streams" %
 *
 * CLI examples:
 *   ai.php --search "java 21 virtual threads example"
 *   ai.php --file prompt.md --chat session.json --cache-dir ~/.ai_cache
 */

const DEF_MODEL = 'gemini-1.5-flash';
const DEF_TIMEOUT = 30;
const DEF_RETRIES = 3;
const DEF_BACKOFF = 400; // ms

function usage(): void {
    fwrite(STDERR, <<<TXT
ai.php — AI assistant for editors (Gemini, zero-deps)
----------------------------------------------------
Inside editor (bind to Ctrl+Enter):
  :!ai.php --stdin              " replace buffer
  :'<,'>!ai.php --stdin          " replace selection
  :!ai.php --ask "explain this" %

Shell examples:
  ai.php --search "quick sort python"
  ai.php --file prompt.md --chat talk.json --cache-dir ~/.ai_cache
  echo "why is the sky blue" | ai.php --stdin --json

Flags:
  --ask "text"       Ask directly
  --search "query"   Synthesise answer as if from search
  --stdin            Read prompt from stdin (default if no --ask/--search/--file)
  --file path        Read prompt from file
  --chat path.json   Persist multi-turn history
  --model NAME       Default: gemini-1.5-flash
  --auth KEY         Gemini API key (or GEMINI_API_KEY env)
  --api URL          Override endpoint
  --system "text"    System instruction
  --temperature F    0..2  (default 0.7)
  --topK N           int   (default 40)
  --maxTokens N      int   (default 1024)
  --timeout N        sec   (default 30)
  --retries N        int   (default 3)
  --backoff MS       int   (default 400)
  --cache-dir DIR    Disk cache for identical requests
  --json             Output raw JSON (for scripting)
  --help             This help
TXT);
}

function hash_req(string $s): string { return substr(hash('sha256', $s), 0, 16); }
function cache(?string $dir, string $key, ?string $val = null): ?string {
    if (!$dir) return $val;
    $f = "$dir/$key.json";
    if ($val === null) return is_file($f) ? file_get_contents($f) : null;
    @mkdir($dir, 0700, true);
    file_put_contents($f, $val);
    return $val;
}

function stream_ai(string $prompt, array $opts): void {
    $url = ($opts['api'] ?? "https://generativelanguage.googleapis.com/v1beta/models/{$opts['model']}:generateContent")
         . '?key=' . urlencode($opts['key']);

    $body = [
        'contents' => [
            ['role' => 'user', 'parts' => [['text' => $prompt]]]
        ],
        'generationConfig' => array_filter([
            'temperature' => $opts['temperature'] ?? 0.7,
            'topK'        => $opts['topK'] ?? 40,
            'maxOutputTokens' => $opts['maxTokens'] ?? 1024,
        ]),
    ];
    if ($opts['system'] ?? null) {
        $body['systemInstruction'] = ['parts' => [['text' => $opts['system']]]];
    }

    $json = json_encode($body, JSON_UNESCAPED_UNICODE);
    $hash = $opts['cacheDir'] ? cache($opts['cacheDir'], hash_req($json)) : null;
    if ($hash !== null && !($opts['json'] ?? false)) {
        fwrite(STDOUT, extract_text(json_decode($hash, true)));
        return;
    }

    $ch = curl_init($url);
    curl_setopt_array($ch, [
        CURLOPT_POST           => true,
        CURLOPT_RETURNTRANSFER => false, // we stream
        CURLOPT_HTTPHEADER     => ['Content-Type: application/json'],
        CURLOPT_POSTFIELDS     => $json,
        CURLOPT_TIMEOUT        => $opts['timeout'] ?? DEF_TIMEOUT,
        CURLOPT_WRITEFUNCTION  => function($ch, $data) {
            static $buf = '';
            $buf .= $data;
            while (($p = strpos($buf, "\n")) !== false) {
                $line = substr($buf, 0, $p);
                $buf  = substr($buf, $p + 1);
                if ($dec = json_decode($line, true)) {
                    foreach ($dec as $cand) {
                        if ($t = extract_text($cand)) fwrite(STDOUT, $t);
                    }
                }
            }
            return strlen($data);
        },
    ]);

    $retries = $opts['retries'] ?? DEF_RETRIES;
    $backoff = $opts['backoff'] ?? DEF_BACKOFF;
    for ($try = 0; $try <= $retries; $try++) {
        $buf = '';
        $ok  = curl_exec($ch);
        $code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        if ($ok && $code < 400) break;
        if ($try < $retries && ($code === 429 || $code >= 500)) {
            usleep($backoff * 1000 * (2 ** $try));
            continue;
        }
        if (!$ok || $code >= 400) {
            fwrite(STDERR, "AI error (HTTP $code): " . curl_error($ch) . PHP_EOL);
            exit(2);
        }
    }
    curl_close($ch);

    if (($opts['cacheDir'] ?? false) && isset($dec)) {
        cache($opts['cacheDir'], hash_req($json), json_encode($dec, JSON_UNESCAPED_UNICODE));
    }
}

function extract_text(array $cand): ?string {
    $parts = $cand['content']['parts'] ?? [];
    foreach ($parts as $p) if (isset($p['text'])) return $p['text'];
    return null;
}

// ---------- CLI ----------
$args = array_slice($argv, 1);
if (!$args) { usage(); exit(1); }

$opts = [
    'model' => DEF_MODEL,
    'temperature' => 0.7,
    'topK' => 40,
    'maxTokens' => 1024,
    'timeout' => DEF_TIMEOUT,
    'retries' => DEF_RETRIES,
    'backoff' => DEF_BACKOFF,
];
$prompt = null;

for ($i = 0; $i < count($args); $i++) {
    switch ($args[$i]) {
        case '--ask':      $prompt = $args[++$i] ?? ''; break;
        case '--search':   $prompt = 'Synthesise a concise answer for: ' . ($args[++$i] ?? ''); break;
        case '--stdin':    /* default below */ break;
        case '--file':     $prompt = trim(file_get_contents($args[++$i] ?? '')); break;
        case '--model':    $opts['model'] = $args[++$i] ?? DEF_MODEL; break;
        case '--auth':     $opts['key'] = $args[++$i] ?? ''; break;
        case '--api':      $opts['api'] = $args[++$i] ?? ''; break;
        case '--system':   $opts['system'] = $args[++$i] ?? ''; break;
        case '--temperature': $opts['temperature'] = (float)($args[++$i] ?? 0.7); break;
        case '--topK':     $opts['topK'] = (int)($args[++$i] ?? 40); break;
        case '--maxTokens': $opts['maxTokens'] = (int)($args[++$i] ?? 1024); break;
        case '--timeout':  $opts['timeout'] = (int)($args[++$i] ?? DEF_TIMEOUT); break;
        case '--retries':  $opts['retries'] = (int)($args[++$i] ?? DEF_RETRIES); break;
        case '--backoff':  $opts['backoff'] = (int)($args[++$i] ?? DEF_BACKOFF); break;
        case '--cache-dir': $opts['cacheDir'] = $args[++$i] ?? ''; break;
        case '--json':     $opts['json'] = true; break;
        case '--help':     usage(); exit(0);
        default:           fwrite(STDERR, "Unknown flag: {$args[$i]}\n"); exit(1);
    }
}

if (!($opts['key'] ?? false)) {
    $opts['key'] = getenv('GEMINI_API_KEY');
    if (!$opts['key']) { fwrite(STDERR, "Missing API key. Set GEMINI_API_KEY or use --auth\n"); exit(1); }
}

if ($prompt === null && !in_array('--stdin', $args, true)) {
    $prompt = trim(stream_get_contents(STDIN));
}
if ($prompt === '') { fwrite(STDERR, "Empty prompt\n"); exit(1); }

stream_ai($prompt, $opts);
exit(0);
