#!/usr/bin/env php
<?php
/**
 * Minimal AI CLI:
 *  - Only runs when --ask or --search is supplied
 *  - Talks to Google Generative Language API (Gemini)
 *  - Robust response parsing + clear errors
 */

function usage(): void {
    $u = <<<TXT
Usage:
  php ai_chat_only.php --ask "prompt" [--auth KEY|--env] [--model gemini-pro] [--system "text"] [--json]
  php ai_chat_only.php --search "query" [--auth KEY|--env] [--model gemini-pro] [--system "text"] [--json]
  php ai_chat_only.php --ask "..." --search "..." ...

Flags:
  --ask "text"        Ask the AI directly.
  --search "query"    Ask the AI to synthesize an answer as if from search results.
  --auth "KEY"        API key explicitly.
  --env               Read API key from GEMINI_API_KEY env var (ignored if --auth is present).
  --model "NAME"      Model name (default: gemini-pro). Examples: gemini-1.5-flash, gemini-1.5-pro.
  --api "URL"         Override API endpoint (default built from model).
  --system "text"     Optional system instruction.
  --timeout N         HTTP timeout in seconds (default: 30).
  --json              Print the raw JSON response instead of extracted text.

Notes:
  - Exits with code 1 for usage errors, 2 for HTTP/parse failures.
TXT;
    fwrite(STDERR, $u . PHP_EOL);
}

function build_url(string $model, ?string $api): string {
    if ($api !== null && $api !== '') return $api;
    // Default Gemini endpoint:
    return "https://generativelanguage.googleapis.com/v1beta/models/{$model}:generateContent";
}

function ai_call(string $prompt, ?string $system, string $url, string $api_key, int $timeout, bool $want_json) {
    $body = [
        "contents" => [[
            // role can be omitted; Gemini accepts plain contents
            "parts" => [["text" => $prompt]],
        ]],
    ];
    if ($system !== null && $system !== '') {
        $body["systemInstruction"] = [
            "parts" => [["text" => $system]],
        ];
    }

    $qs = strpos($url, '?') === false ? '?' : '&';
    $final_url = $url . $qs . "key=" . urlencode($api_key);

    $ch = curl_init($final_url);
    curl_setopt_array($ch, [
        CURLOPT_POST            => true,
        CURLOPT_RETURNTRANSFER  => true,
        CURLOPT_HTTPHEADER      => ["Content-Type: application/json"],
        CURLOPT_POSTFIELDS      => json_encode($body, JSON_UNESCAPED_UNICODE),
        CURLOPT_TIMEOUT         => $timeout,
    ]);
    $response = curl_exec($ch);
    $curl_err = curl_error($ch);
    $http_code = curl_getinfo($ch, CURLINFO_HTTP_CODE);
    curl_close($ch);

    if ($curl_err || $response === false) {
        fwrite(STDERR, "AI call failed (network/cURL): {$curl_err}\n");
        exit(2);
    }
    if ($http_code >= 400) {
        $snippet = mb_substr($response, 0, 400);
        fwrite(STDERR, "AI call failed (HTTP {$http_code}). Server said:\n{$snippet}\n");
        exit(2);
    }

    $json = json_decode($response, true);
    if ($json === null) {
        fwrite(STDERR, "AI call failed: response was not valid JSON.\n");
        exit(2);
    }

    if ($want_json) {
        echo $response . PHP_EOL;
        return;
    }

    // Extract first text candidate safely
    $text = extract_text($json);
    if ($text === null) {
        // Some safety blocks return no text but include safetyFeedback
        $why = isset($json['promptFeedback']) ? json_encode($json['promptFeedback']) : 'no text candidates present';
        fwrite(STDERR, "AI call returned no text: {$why}\n");
        exit(2);
    }
    echo $text . PHP_EOL;
}

function extract_text(array $json): ?string {
    if (!isset($json['candidates']) || !is_array($json['candidates']) || count($json['candidates']) === 0) {
        return null;
    }
    $c = $json['candidates'][0] ?? null;
    if (!is_array($c)) return null;

    // Primary path: candidates[0].content.parts[*].text
    if (isset($c['content']['parts']) && is_array($c['content']['parts'])) {
        $parts = $c['content']['parts'];
        $texts = [];
        foreach ($parts as $p) {
            if (isset($p['text']) && is_string($p['text'])) {
                $texts[] = $p['text'];
            }
        }
        if ($texts) return implode("\n", $texts);
    }
    // Some variants may use 'text' directly on candidate (rare)
    if (isset($c['text']) && is_string($c['text'])) {
        return $c['text'];
    }
    return null;
}

// ----------------- CLI parse -----------------
$args = array_slice($argv, 1);
$prompt = null;
$search = null;
$auth = null;
$use_env = false;
$model = "gemini-pro";
$api = null;
$system = null;
$timeout = 30;
$want_json = false;

for ($i = 0; $i < count($args); $i++) {
    $arg = $args[$i];
    switch ($arg) {
        case "--ask":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $prompt = $args[++$i];
            break;
        case "--search":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $search = $args[++$i];
            break;
        case "--auth":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $auth = $args[++$i];
            break;
        case "--env":
            $use_env = true;
            break;
        case "--model":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $model = $args[++$i];
            break;
        case "--api":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $api = $args[++$i];
            break;
        case "--system":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $system = $args[++$i];
            break;
        case "--timeout":
            if (!isset($args[$i+1])) { usage(); exit(1); }
            $timeout = max(1, (int)$args[++$i]);
            break;
        case "--json":
            $want_json = true;
            break;
        case "-h":
        case "--help":
            usage(); exit(0);
        default:
            fwrite(STDERR, "Unknown flag: {$arg}\n");
            usage(); exit(1);
    }
}

// enforce contract: must have --ask or --search
if ($prompt === null && $search === null) {
    fwrite(STDERR, "Please provide a prompt using --ask, a search query using --search, or both.\n");
    usage(); exit(1);
}

// resolve API key
if ($auth === null && $use_env) {
    $env = getenv("GEMINI_API_KEY");
    if ($env !== false && $env !== "") $auth = $env;
}
if ($auth === null) {
    fwrite(STDERR, "Missing API key. Use --auth KEY or --env with GEMINI_API_KEY set.\n");
    exit(1);
}

$url = build_url($model, $api);

$final_outputs = [];

if ($prompt !== null) {
    echo "AI reply:\n";
    ai_call($prompt, $system, $url, $auth, $timeout, $want_json);
    echo PHP_EOL;
}

if ($search !== null) {
    echo "AI Search Results:\n";
    $search_prompt = "Using your general knowledge and current reasoning, synthesize a concise answer for: {$search}";
    ai_call($search_prompt, $system, $url, $auth, $timeout, $want_json);
    echo PHP_EOL;
}