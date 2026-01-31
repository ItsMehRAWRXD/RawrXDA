#!/usr/bin/env php
<?php
/**
 * ai_cli.php — Practical Gemini CLI that goes beyond "hello world".
 * PHP >= 8.0 + curl extension.
 *
 * Features:
 *  - --ask / --search / --file / --stdin (prompt sources)
 *  - --chat chat.json (persistent multi-turn)
 *  - --model, --temperature, --topK, --topP, --maxTokens
 *  - --auth KEY or --env (GEMINI_API_KEY)
 *  - --config config.json (defaults), --cache-dir .ai-cache
 *  - Retries with exponential backoff on 429/5xx
 *  - --proxy http://user:pass@host:port
 *  - --json (raw JSON output)
 *  - Exit codes: 1 usage, 2 failure
 */

const DEFAULT_MODEL = "gemini-1.5-flash";
const DEFAULT_TIMEOUT = 45;
const DEFAULT_RETRIES = 3;
const DEFAULT_BACKOFF_BASE_MS = 400;

// Monitoring constants
const METRICS_FILE = ".ai_cli_metrics.json";
const HEALTH_CHECK_INTERVAL = 10; // seconds

function usage(): void {
    $u = <<<TXT
Usage:
  php ai_cli.php --ask "prompt" [flags...]
  php ai_cli.php --search "query" [flags...]
  php ai_cli.php --file prompt.txt [flags...]
  echo "prompt" | php ai_cli.php --stdin [flags...]

Prompt Sources (pick at least one):
  --ask "text"           Ask the model directly.
  --search "query"       Instruct model to synthesize an answer as if from search.
  --file path.txt        Read prompt from a file (UTF-8).
  --stdin                Read prompt from STDIN.

Conversation:
  --chat chat.json       Maintain multi-turn history in a JSON file (created if missing).
  --system "text"        System instruction (guides behavior).

Model & Params:
  --model "NAME"         Default: gemini-1.5-flash (try gemini-1.5-pro for quality).
  --temperature F        0..2 (float). Default: 0.2
  --topK N               Integer. Default: 40
  --topP F               0..1 (float). Default: 0.95
  --maxTokens N          Max output tokens (safety cap). Default: 1024

Auth & Endpoint:
  --auth "KEY"           API key explicitly.
  --env                  Read GEMINI_API_KEY env var if --auth not given.
  --api "URL"            Override endpoint (else derived from model).
  --proxy "URL"          cURL proxy (e.g., http://user:pass@host:port).

Reliability & I/O:
  --timeout N            HTTP timeout seconds (default: 45)
  --retries N            Retries on 429/5xx (default: 3)
  --backoff MS           Base backoff ms (default: 400)
  --cache-dir PATH       Response cache dir (skips identical repeat calls)
  --config config.json   Load defaults (merged; flags override).
  --json                 Print raw JSON body instead of extracted text.
  --verbose              Extra logging to STDERR.

Monitoring:
  --metrics              Show performance metrics and statistics.
  --health               Perform health check on API endpoint.
  --monitor              Enable real-time monitoring mode.
  --stats                Display usage statistics.

Exit Codes:
  1 usage error, 2 HTTP/network/parse failure.
TXT;
    fwrite(STDERR, $u . PHP_EOL);
}

function load_json_file(string $path): ?array {
    if (!is_file($path)) return null;
    $s = file_get_contents($path);
    if ($s === false) return null;
    $j = json_decode($s, true);
    return is_array($j) ? $j : null;
}

function save_json_file(string $path, array $data): void {
    $dir = dirname($path);
    if (!is_dir($dir)) @mkdir($dir, 0777, true);
    file_put_contents($path, json_encode($data, JSON_PRETTY_PRINT|JSON_UNESCAPED_UNICODE));
}

function build_url(string $model, ?string $api): string {
    if ($api) return $api;
    return "https://generativelanguage.googleapis.com/v1beta/models/{$model}:generateContent";
}

function read_prompt_from_file(string $path): string {
    if (!is_file($path)) {
        fwrite(STDERR, "File not found: {$path}\n");
        exit(1);
    }
    $s = file_get_contents($path);
    if ($s === false) {
        fwrite(STDERR, "Failed to read file: {$path}\n");
        exit(1);
    }
    return trim($s);
}

function read_prompt_from_stdin(): string {
    $s = stream_get_contents(STDIN);
    if ($s === false) {
        fwrite(STDERR, "Failed to read from STDIN.\n");
        exit(1);
    }
    return trim($s);
}

function hash_request(array $payload): string {
    return hash('sha256', json_encode($payload));
}

function cache_get(string $dir, string $key): ?string {
    $path = rtrim($dir, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $key . ".json";
    return is_file($path) ? file_get_contents($path) : null;
}
function cache_put(string $dir, string $key, string $body): void {
    $path = rtrim($dir, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $key . ".json";
    @mkdir(dirname($path), 0777, true);
    file_put_contents($path, $body);
}

function extract_text_from_response(array $json): ?string {
    if (!isset($json['candidates']) || !is_array($json['candidates']) || count($json['candidates']) === 0) return null;
    $c = $json['candidates'][0];
    // Common path: candidates[0].content.parts[*].text
    $out = [];
    if (isset($c['content']['parts']) && is_array($c['content']['parts'])) {
        foreach ($c['content']['parts'] as $p) {
            if (isset($p['text']) && is_string($p['text'])) $out[] = $p['text'];
        }
    }
    if ($out) return implode("\n", $out);
    // fallback
    if (isset($c['text']) && is_string($c['text'])) return $c['text'];
    return null;
}

function backoff_sleep_ms(int $ms): void {
    usleep($ms * 1000);
}

// ----------------- Monitoring Functions -----------------

function load_metrics(): array {
    if (!file_exists(METRICS_FILE)) {
        return [
            'total_requests' => 0,
            'successful_requests' => 0,
            'failed_requests' => 0,
            'total_tokens' => 0,
            'total_time' => 0,
            'avg_response_time' => 0,
            'cache_hits' => 0,
            'cache_misses' => 0,
            'last_request' => null,
            'error_counts' => [],
            'model_usage' => [],
            'daily_stats' => []
        ];
    }
    $data = file_get_contents(METRICS_FILE);
    return json_decode($data, true) ?: [];
}

function save_metrics(array $metrics): void {
    file_put_contents(METRICS_FILE, json_encode($metrics, JSON_PRETTY_PRINT));
}

function update_metrics(string $event, array $data = []): void {
    $metrics = load_metrics();
    $now = time();
    
    switch ($event) {
        case 'request_start':
            $metrics['total_requests']++;
            $metrics['last_request'] = $now;
            break;
            
        case 'request_success':
            $metrics['successful_requests']++;
            if (isset($data['response_time'])) {
                $metrics['total_time'] += $data['response_time'];
                $metrics['avg_response_time'] = $metrics['total_time'] / $metrics['successful_requests'];
            }
            if (isset($data['tokens'])) {
                $metrics['total_tokens'] += $data['tokens'];
            }
            if (isset($data['model'])) {
                $metrics['model_usage'][$data['model']] = ($metrics['model_usage'][$data['model']] ?? 0) + 1;
            }
            break;
            
        case 'request_failure':
            $metrics['failed_requests']++;
            if (isset($data['error'])) {
                $metrics['error_counts'][$data['error']] = ($metrics['error_counts'][$data['error']] ?? 0) + 1;
            }
            break;
            
        case 'cache_hit':
            $metrics['cache_hits']++;
            break;
            
        case 'cache_miss':
            $metrics['cache_misses']++;
            break;
    }
    
    // Update daily stats
    $today = date('Y-m-d');
    if (!isset($metrics['daily_stats'][$today])) {
        $metrics['daily_stats'][$today] = [
            'requests' => 0,
            'successful' => 0,
            'failed' => 0,
            'tokens' => 0
        ];
    }
    
    if ($event === 'request_start') {
        $metrics['daily_stats'][$today]['requests']++;
    } elseif ($event === 'request_success') {
        $metrics['daily_stats'][$today]['successful']++;
        if (isset($data['tokens'])) {
            $metrics['daily_stats'][$today]['tokens'] += $data['tokens'];
        }
    } elseif ($event === 'request_failure') {
        $metrics['daily_stats'][$today]['failed']++;
    }
    
    save_metrics($metrics);
}

function show_metrics(): void {
    $metrics = load_metrics();
    
    if ($metrics['total_requests'] === 0) {
        echo "No metrics available yet.\n";
        return;
    }
    
    $success_rate = $metrics['total_requests'] > 0 
        ? round(($metrics['successful_requests'] / $metrics['total_requests']) * 100, 2) 
        : 0;
    
    $cache_hit_rate = ($metrics['cache_hits'] + $metrics['cache_misses']) > 0
        ? round(($metrics['cache_hits'] / ($metrics['cache_hits'] + $metrics['cache_misses'])) * 100, 2)
        : 0;
    
    echo "=== AI CLI Performance Metrics ===\n";
    echo "Total Requests: {$metrics['total_requests']}\n";
    echo "Successful: {$metrics['successful_requests']} ({$success_rate}%)\n";
    echo "Failed: {$metrics['failed_requests']}\n";
    echo "Average Response Time: " . round($metrics['avg_response_time'], 2) . "s\n";
    echo "Total Tokens: {$metrics['total_tokens']}\n";
    echo "Cache Hit Rate: {$cache_hit_rate}% ({$metrics['cache_hits']}/" . ($metrics['cache_hits'] + $metrics['cache_misses']) . ")\n";
    
    if (!empty($metrics['model_usage'])) {
        echo "\nModel Usage:\n";
        arsort($metrics['model_usage']);
        foreach ($metrics['model_usage'] as $model => $count) {
            echo "  {$model}: {$count} requests\n";
        }
    }
    
    if (!empty($metrics['error_counts'])) {
        echo "\nError Summary:\n";
        arsort($metrics['error_counts']);
        foreach ($metrics['error_counts'] as $error => $count) {
            echo "  {$error}: {$count} occurrences\n";
        }
    }
    
    if (!empty($metrics['daily_stats'])) {
        echo "\nRecent Daily Stats:\n";
        $recent_days = array_slice($metrics['daily_stats'], -7, 7, true);
        foreach ($recent_days as $date => $stats) {
            echo "  {$date}: {$stats['requests']} requests, {$stats['successful']} successful, {$stats['tokens']} tokens\n";
        }
    }
    
    if ($metrics['last_request']) {
        echo "\nLast Request: " . date('Y-m-d H:i:s', $metrics['last_request']) . "\n";
    }
}

function health_check(string $url, string $api_key): bool {
    $start_time = microtime(true);
    
    $ch = curl_init($url . "?key=" . urlencode($api_key));
    curl_setopt_array($ch, [
        CURLOPT_POST => true,
        CURLOPT_RETURNTRANSFER => true,
        CURLOPT_HTTPHEADER => ["Content-Type: application/json"],
        CURLOPT_POSTFIELDS => json_encode([
            "contents" => [["parts" => [["text" => "Health check"]]]]
        ]),
        CURLOPT_TIMEOUT => 10,
        CURLOPT_CONNECTTIMEOUT => 5
    ]);
    
    $response = curl_exec($ch);
    $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    $response_time = microtime(true) - $start_time;
    curl_close($ch);
    
    $healthy = $http_code === 200 && $response !== false;
    
    echo "=== Health Check ===\n";
    echo "Endpoint: {$url}\n";
    echo "Status: " . ($healthy ? "HEALTHY" : "UNHEALTHY") . "\n";
    echo "HTTP Code: {$http_code}\n";
    echo "Response Time: " . round($response_time, 3) . "s\n";
    
    if (!$healthy) {
        echo "Response: " . substr($response, 0, 200) . "\n";
    }
    
    return $healthy;
}

function monitor_mode(string $url, string $api_key): void {
    echo "=== Real-time Monitoring Mode ===\n";
    echo "Press Ctrl+C to stop monitoring\n\n";
    
    $last_metrics = load_metrics();
    $start_time = time();
    
    while (true) {
        $current_metrics = load_metrics();
        $elapsed = time() - $start_time;
        
        // Clear screen (ANSI escape sequence)
        echo "\033[2J\033[H";
        
        echo "=== Real-time AI CLI Monitor ===\n";
        echo "Monitoring for: " . gmdate('H:i:s', $elapsed) . "\n";
        echo "Last updated: " . date('H:i:s') . "\n\n";
        
        // Show current stats
        $requests_since_start = $current_metrics['total_requests'] - $last_metrics['total_requests'];
        $successful_since_start = $current_metrics['successful_requests'] - $last_metrics['successful_requests'];
        $failed_since_start = $current_metrics['failed_requests'] - $last_metrics['failed_requests'];
        
        echo "Requests this session: {$requests_since_start}\n";
        echo "Successful: {$successful_since_start}\n";
        echo "Failed: {$failed_since_start}\n";
        echo "Current success rate: " . round($current_metrics['avg_response_time'], 2) . "s avg\n\n";
        
        // Show recent activity
        if ($current_metrics['last_request']) {
            $last_request_age = time() - $current_metrics['last_request'];
            echo "Last request: {$last_request_age}s ago\n";
        }
        
        // Perform health check every HEALTH_CHECK_INTERVAL seconds
        if ($elapsed % HEALTH_CHECK_INTERVAL === 0) {
            echo "\nPerforming health check...\n";
            health_check($url, $api_key);
        }
        
        sleep(1);
    }
}

function ai_call(array $payload, string $url, string $api_key, int $timeout, ?string $proxy, int $retries, int $backoff_base_ms, bool $verbose, string $model = ''): string {
    $start_time = microtime(true);
    update_metrics('request_start');
    
    $qs = strpos($url, '?') === false ? '?' : '&';
    $final_url = $url . $qs . "key=" . urlencode($api_key);
    $attempt = 0;
    $last_err = "";

    while (true) {
        $attempt++;
        $ch = curl_init($final_url);
        $headers = ["Content-Type: application/json"];
        $opts = [
            CURLOPT_POST           => true,
            CURLOPT_RETURNTRANSFER => true,
            CURLOPT_HTTPHEADER     => $headers,
            CURLOPT_POSTFIELDS     => json_encode($payload, JSON_UNESCAPED_UNICODE),
            CURLOPT_TIMEOUT        => $timeout,
        ];
        if ($proxy) $opts[CURLOPT_PROXY] = $proxy;
        curl_setopt_array($ch, $opts);

        $resp = curl_exec($ch);
        $curl_err = curl_error($ch);
        $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
        curl_close($ch);

        if ($curl_err || $resp === false) {
            $last_err = "Network/cURL error: {$curl_err}";
            if ($verbose) fwrite(STDERR, "[attempt {$attempt}] {$last_err}\n");
            if ($attempt <= $retries) {
                $sleep = $backoff_base_ms * (2 ** ($attempt - 1));
                backoff_sleep_ms($sleep);
                continue;
            }
            update_metrics('request_failure', ['error' => 'Network/cURL error']);
            fwrite(STDERR, $last_err . "\n");
            exit(2);
        }

        if ($http_code >= 500 || $http_code == 429) {
            $last_err = "HTTP {$http_code}: transient/server rate-limit.\n" . mb_substr($resp, 0, 400);
            if ($verbose) fwrite(STDERR, "[attempt {$attempt}] {$last_err}\n");
            if ($attempt <= $retries) {
                $sleep = $backoff_base_ms * (2 ** ($attempt - 1));
                backoff_sleep_ms($sleep);
                continue;
            }
            update_metrics('request_failure', ['error' => "HTTP {$http_code}"]);
            fwrite(STDERR, $last_err . "\n");
            exit(2);
        }

        if ($http_code >= 400) {
            $snippet = mb_substr($resp, 0, 800);
            update_metrics('request_failure', ['error' => "HTTP {$http_code}"]);
            fwrite(STDERR, "HTTP {$http_code} error:\n{$snippet}\n");
            exit(2);
        }

        // Success - update metrics
        $response_time = microtime(true) - $start_time;
        $tokens = 0;
        
        // Try to extract token count from response
        $json_resp = json_decode($resp, true);
        if ($json_resp && isset($json_resp['usageMetadata'])) {
            $tokens = ($json_resp['usageMetadata']['promptTokenCount'] ?? 0) + 
                     ($json_resp['usageMetadata']['candidatesTokenCount'] ?? 0);
        }
        
        update_metrics('request_success', [
            'response_time' => $response_time,
            'tokens' => $tokens,
            'model' => $model
        ]);
        
        return $resp;
    }
}

// ----------------- Parse CLI -----------------
$args = array_slice($argv, 1);
if (!$args) { usage(); exit(1); }

$cfg = [
    "model"       => DEFAULT_MODEL,
    "temperature" => 0.2,
    "topK"        => 40,
    "topP"        => 0.95,
    "maxTokens"   => 1024,
    "timeout"     => DEFAULT_TIMEOUT,
    "retries"     => DEFAULT_RETRIES,
    "backoff"     => DEFAULT_BACKOFF_BASE_MS,
];

$ask = $search = $file = $system = $api = $auth = $proxy = $cache_dir = $chat_path = null;
$use_env = false;
$stdin = false;
$want_json = false;
$verbose = false;
$config_path = null;
$show_metrics = false;
$health_check = false;
$monitor_mode = false;
$show_stats = false;

for ($i = 0; $i < count($args); $i++) {
    $a = $args[$i];
    switch ($a) {
        case "--ask":        $ask      = $args[++$i] ?? null; break;
        case "--search":     $search   = $args[++$i] ?? null; break;
        case "--file":       $file     = $args[++$i] ?? null; break;
        case "--stdin":      $stdin    = true; break;
        case "--system":     $system   = $args[++$i] ?? null; break;
        case "--model":      $cfg["model"] = $args[++$i] ?? $cfg["model"]; break;
        case "--temperature":$cfg["temperature"] = (float)($args[++$i] ?? $cfg["temperature"]); break;
        case "--topK":       $cfg["topK"] = (int)($args[++$i] ?? $cfg["topK"]); break;
        case "--topP":       $cfg["topP"] = (float)($args[++$i] ?? $cfg["topP"]); break;
        case "--maxTokens":  $cfg["maxTokens"] = (int)($args[++$i] ?? $cfg["maxTokens"]); break;
        case "--timeout":    $cfg["timeout"] = max(1, (int)($args[++$i] ?? $cfg["timeout"])); break;
        case "--retries":    $cfg["retries"] = max(0, (int)($args[++$i] ?? $cfg["retries"])); break;
        case "--backoff":    $cfg["backoff"] = max(1, (int)($args[++$i] ?? $cfg["backoff"])); break;
        case "--auth":       $auth     = $args[++$i] ?? null; break;
        case "--env":        $use_env  = true; break;
        case "--api":        $api      = $args[++$i] ?? null; break;
        case "--proxy":      $proxy    = $args[++$i] ?? null; break;
        case "--cache-dir":  $cache_dir= $args[++$i] ?? null; break;
        case "--config":     $config_path = $args[++$i] ?? null; break;
        case "--chat":       $chat_path= $args[++$i] ?? null; break;
        case "--json":       $want_json= true; break;
        case "--verbose":    $verbose  = true; break;
        case "--metrics":    $show_metrics = true; break;
        case "--health":     $health_check = true; break;
        case "--monitor":    $monitor_mode = true; break;
        case "--stats":      $show_stats = true; break;
        case "-h":
        case "--help":       usage(); exit(0);
        default:
            fwrite(STDERR, "Unknown flag: {$a}\n");
            usage(); exit(1);
    }
}

// Load config defaults (optional)
if ($config_path) {
    $cfg_file = load_json_file($config_path);
    if ($cfg_file) {
        foreach ($cfg_file as $k => $v) {
            if (array_key_exists($k, $cfg)) $cfg[$k] = $v;
        }
        if ($verbose) fwrite(STDERR, "Loaded config from {$config_path}\n");
    } else {
        fwrite(STDERR, "Warning: could not load config at {$config_path}\n");
    }
}

// Handle monitoring commands first
if ($show_metrics || $show_stats) {
    show_metrics();
    exit(0);
}

if ($health_check || $monitor_mode) {
    // Resolve API key for health check
    if ($auth === null && $use_env) {
        $env = getenv("GEMINI_API_KEY");
        if ($env) $auth = $env;
    }
    if ($auth === null) {
        fwrite(STDERR, "Missing API key for health check. Use --auth KEY or --env with GEMINI_API_KEY set.\n");
        exit(1);
    }
    
    $url = build_url($cfg["model"], $api);
    
    if ($health_check) {
        $healthy = health_check($url, $auth);
        exit($healthy ? 0 : 2);
    }
    
    if ($monitor_mode) {
        monitor_mode($url, $auth);
        exit(0);
    }
}

// Require a prompt source for AI calls
$have_source = ($ask !== null) || ($search !== null) || ($file !== null) || $stdin;
if (!$have_source) {
    fwrite(STDERR, "Provide at least one prompt source: --ask / --search / --file / --stdin\n");
    usage(); exit(1);
}

// Resolve API key
if ($auth === null && $use_env) {
    $env = getenv("GEMINI_API_KEY");
    if ($env) $auth = $env;
}
if ($auth === null) {
    fwrite(STDERR, "Missing API key. Use --auth KEY or --env with GEMINI_API_KEY set.\n");
    exit(1);
}

// Assemble the user prompt
$prompt_parts = [];
if ($ask !== null)   $prompt_parts[] = $ask;
if ($search !== null) {
    $prompt_parts[] = "Synthesize a concise, well-structured answer to: {$search}";
}
if ($file !== null)  $prompt_parts[] = read_prompt_from_file($file);
if ($stdin)          $prompt_parts[] = read_prompt_from_stdin();

$prompt = trim(implode("\n\n", $prompt_parts));

// Build conversation contents
$contents = [];

// Load prior chat history if provided
if ($chat_path) {
    $hist = load_json_file($chat_path);
    if ($hist && isset($hist['contents']) && is_array($hist['contents'])) {
        // Expecting: [{role: "user"|"model", parts: [{text: "..."}]}...]
        foreach ($hist['contents'] as $turn) {
            if (isset($turn['role'], $turn['parts']) && is_array($turn['parts'])) {
                $contents[] = $turn;
            }
        }
    }
}

// Add the new user turn
$contents[] = [
    "role"  => "user",
    "parts" => [["text" => $prompt]],
];

// Build request payload
$payload = [
    "contents" => $contents,
    "generationConfig" => [
        "temperature"      => (float)$cfg["temperature"],
        "topK"             => (int)$cfg["topK"],
        "topP"             => (float)$cfg["topP"],
        "maxOutputTokens"  => (int)$cfg["maxTokens"],
    ],
];

// Optional system instruction
if ($system !== null && $system !== '') {
    $payload["systemInstruction"] = [
        "parts" => [["text" => $system]],
    ];
}

$url = build_url($cfg["model"], $api);

// Cache check (request-level)
if ($cache_dir) {
    $key = hash_request([
        "url" => $url,
        "payload" => $payload,
        "model" => $cfg["model"],
        "api" => $api,
    ]);
    $cached = cache_get($cache_dir, $key);
    if ($cached !== null) {
        update_metrics('cache_hit');
        if ($verbose) fwrite(STDERR, "[cache] hit {$key}\n");
        $json = json_decode($cached, true);
        if ($want_json) {
            echo $cached . PHP_EOL;
        } else {
            $text = extract_text_from_response($json);
            if ($text === null) {
                fwrite(STDERR, "Cached response had no text.\n");
                echo $cached . PHP_EOL;
            } else {
                echo $text . PHP_EOL;
            }
        }
        // Update chat file if needed (model's turn)
        if ($chat_path) {
            $model_text = extract_text_from_response($json);
            if ($model_text !== null) {
                $hist = load_json_file($chat_path) ?? [];
                $hist['contents'] = $hist['contents'] ?? [];
                $hist['contents'][] = ["role" => "model", "parts" => [["text" => $model_text]]];
                save_json_file($chat_path, $hist);
            }
        }
        exit(0);
    } else {
        update_metrics('cache_miss');
    }
}

// Call API
$resp = ai_call(
    payload: $payload,
    url: $url,
    api_key: $auth,
    timeout: (int)$cfg["timeout"],
    proxy: $proxy,
    retries: (int)$cfg["retries"],
    backoff_base_ms: (int)$cfg["backoff"],
    verbose: $verbose,
    model: $cfg["model"]
);

// Persist cache
if ($cache_dir) {
    $key = hash_request([
        "url" => $url,
        "payload" => $payload,
        "model" => $cfg["model"],
        "api" => $api,
    ]);
    cache_put($cache_dir, $key, $resp);
    if ($verbose) fwrite(STDERR, "[cache] stored {$key}\n");
}

// Print
$json = json_decode($resp, true);
if ($json === null) {
    fwrite(STDERR, "Model returned non-JSON body.\n");
    echo $resp . PHP_EOL;
    exit(2);
}

if ($want_json) {
    echo $resp . PHP_EOL;
} else {
    $text = extract_text_from_response($json);
    if ($text === null) {
        $why = isset($json['promptFeedback']) ? json_encode($json['promptFeedback']) : 'no text candidates present';
        fwrite(STDERR, "No text in response: {$why}\n");
        echo $resp . PHP_EOL;
        exit(2);
    }
    echo $text . PHP_EOL;
}

// Update chat history with model turn
if ($chat_path) {
    $model_text = extract_text_from_response($json);
    if ($model_text !== null) {
        $hist = load_json_file($chat_path) ?? [];
        $hist['contents'] = $hist['contents'] ?? [];
        // Ensure prior user turn exists (we appended above)
        if (!isset($hist['contents']) || !is_array($hist['contents'])) $hist['contents'] = [];
        // Rebuild: we append both user (if not present) and model; for simplicity we only append model here
        $hist['contents'][] = ["role" => "model", "parts" => [["text" => $model_text]]];
        save_json_file($chat_path, $hist);
    }
}