#!/usr/bin/env php
<?php
/**
 * Ultra‑Turbo AI CLI & Microservice (single file, PHP + cURL only)
 * -----------------------------------------------------------------
 * Concrete, production‑minded features:
 *  - CLI modes: --ask / --search / --stdin / --file / --batch
 *  - Microservice mode via PHP built‑in server (see "Serve mode" below)
 *  - Robust Gemini response parsing; retries + exponential backoff
 *  - Parallel batch (curl_multi) with --parallel N
 *  - Optional token streaming to stdout (--stream)
 *  - Disk cache with SHA‑256 keys; --no-cache to disable
 *  - Deterministic example fallback (--examples <lang>) on AI failure
 *  - Output formats: text (default), json, markdown, html, csv, yaml*
 *  - Integrity options: --sign <secret> (HMAC‑SHA256)
 *  - Encryption options: --encrypt <key-hex> [--iv <iv-hex>] (AES-256-CBC)
 *  - Env/config resolution for API key and defaults
 *
 *  (*) YAML printing is minimal and does not require extensions.
 *
 *  Tested with PHP 8.1+ (requires cURL). No external libraries.
 */

// ------------------------------- Constants -------------------------------
const EXIT_USAGE = 1;
const EXIT_FAIL  = 2;

// ------------------------------- TTY Colors -------------------------------
function supports_color(): bool {
    // crude but fine for local use
    return function_exists('posix_isatty') && @posix_isatty(STDERR);
}
function color($s,$code){return supports_color()?"\033[{$code}m{$s}\033[0m":$s;}
function red($s){return color($s,'31');}
function yellow($s){return color($s,'33');}
function green($s){return color($s,'32');}
function cyan($s){return color($s,'36');}

// ------------------------------- Usage -----------------------------------
function usage(): void {
    $u = <<<TXT
Ultra‑Turbo AI CLI & Microservice

CLI:
  php ai_cli.php \
    [--ask "text" | --search "query" | --stdin | --file path | --batch file.{jsonl|tsv}] \
    [--auth KEY | --env | --config cfg.json] [--model NAME] [--api URL] [--system TXT] \
    [--temperature F] [--topK N] [--maxTokens N] [--timeout SEC] \
    [--json | --format text|json|markdown|html|csv|yaml] \
    [--cache DIR | --no-cache] [--retries N] [--retry-base MS] [--proxy URL] \
    [--stream] [--parallel N] [--examples LANG] [--sign SECRET] \
    [--encrypt KEY_HEX [--iv IV_HEX]]

Serve mode (HTTP microservice):
  php -S 0.0.0.0:8080 ai_cli.php
  # POST JSON to /api with fields like {"ask":"...","auth":"..."}

Examples:
  php ai_cli.php --ask "Explain SIMD" --env
  php ai_cli.php --batch prompts.jsonl --parallel 8 --env --json
  echo "What is diffusion?" | php ai_cli.php --stdin --stream --env
  php -S 127.0.0.1:8080 ai_cli.php  # then curl -XPOST localhost:8080/api -d '{"ask":"hi","auth":"KEY"}'

Exit codes: 1 usage | 2 http/parse failures
TXT;
    fwrite(STDERR, $u . PHP_EOL);
}

// ------------------------------- Helpers ---------------------------------
function read_file_trim(string $path): string {
    $c = @file_get_contents($path);
    if ($c === false) { fwrite(STDERR, red("Cannot read file: {$path}").PHP_EOL); exit(EXIT_FAIL);} 
    return trim($c);
}
function load_config(?string $path): array {
    if (!$path) return [];
    if (!is_file($path)) { fwrite(STDERR, yellow("Config not found: {$path}").PHP_EOL); return []; }
    $j = json_decode((string)file_get_contents($path), true);
    return is_array($j) ? $j : [];
}
function build_url(string $model, ?string $api): string {
    if ($api) return $api;
    return "https://generativelanguage.googleapis.com/v1beta/models/{$model}:generateContent";
}
function sha_key(string $s): string { return substr(hash('sha256',$s),0,32); }
function cache_dir_default(): ?string { $home=getenv('HOME'); return $home? rtrim($home,'/').'/.ai_cli_cache':null; }
function cache_path(string $dir, string $key): string { if(!is_dir($dir)) @mkdir($dir,0700,true); return rtrim($dir,'/')."/{$key}.json"; }
function maybe_cache_read(?string $dir,string $key):?array{ if(!$dir)return null; $p=cache_path($dir,$key); if(is_file($p)){ $j=json_decode((string)file_get_contents($p),true); if(is_array($j)) return $j; } return null; }
function cache_write(?string $dir,string $key,array $json):void{ if(!$dir)return; $p=cache_path($dir,$key); @file_put_contents($p,json_encode($json,JSON_UNESCAPED_UNICODE)); }

// ----------------------- State management helpers ---------------------------
function state_path(): ?string {
    $home = getenv('HOME'); if(!$home) return null;
    return rtrim($home,'/').'/.ai_cli_state.json';
}
function load_state(): array {
    $p = state_path(); if(!$p || !is_file($p)) return [];
    $j = json_decode((string)file_get_contents($p), true);
    return is_array($j) ? $j : [];
}
function save_state(array $s): void {
    $p = state_path(); if(!$p) return;
    @file_put_contents($p, json_encode($s, JSON_UNESCAPED_UNICODE|JSON_PRETTY_PRINT));
}

// Minimal YAML printer (scalars, arrays, assoc)
function to_yaml($data, int $indent=0): string {
    $sp = str_repeat('  ',$indent);
    if (is_array($data)) {
        $isAssoc = array_keys($data)!==range(0,count($data)-1);
        $out='';
        foreach($data as $k=>$v){
            if($isAssoc){ $out .= $sp.$k.':'; if(is_array($v)){$out.="\n".to_yaml($v,$indent+1);} else {$out.=' '.(is_string($v)?json_encode($v):var_export($v,true))."\n";} }
            else { $out .= $sp.'- '; if(is_array($v)){$out.="\n".to_yaml($v,$indent+1);} else {$out.=(is_string($v)?json_encode($v):var_export($v,true))."\n";} }
        }
        return $out;
    }
    return $sp.(is_string($data)?json_encode($data):var_export($data,true))."\n";
}

// ----------------------- Deterministic examples ---------------------------
function deterministic_examples(string $lang, int $n=3): array {
    $TASKS=[
        ['id'=>1,'name'=>'hello_world','difficulty'=>1],
        ['id'=>2,'name'=>'fibonacci','difficulty'=>2],
        ['id'=>3,'name'=>'reverse_string','difficulty'=>2],
        ['id'=>4,'name'=>'binary_search','difficulty'=>3],
        ['id'=>5,'name'=>'merge_sort','difficulty'=>4],
    ];
    $SNIPPETS=[
        'java'=>[
            'hello_world'=>'public class Hello{public static void main(String[]a){System.out.println("Hello, world!");}}',
            'fibonacci'=>'public static int fib(int n){if(n<2)return n;return fib(n-1)+fib(n-2);}',
            'reverse_string'=>'public static String reverse(String s){return new StringBuilder(s).reverse().toString();}',
            'binary_search'=>'public static int bs(int[]a,int x){int l=0,r=a.length-1;while(l<=r){int m=l+(r-l)/2;if(a[m]==x)return m;if(a[m]<x)l=m+1;else r=m-1;}return -1;}',
            'merge_sort'=>'public static void ms(int[]a){if(a.length<2)return;int m=a.length/2;int[]L=java.util.Arrays.copyOfRange(a,0,m);int[]R=java.util.Arrays.copyOfRange(a,m,a.length);ms(L);ms(R);/* merge omitted */}',
        ],
        'php'=>[
            'hello_world'=>'<?php echo "Hello, world!"; ?>',
            'fibonacci'=>'function fib($n){return $n<2?$n:fib($n-1)+fib($n-2);}',
            'reverse_string'=>'echo strrev($s);',
            'binary_search'=>'function bs($a,$x){$l=0;$r=count($a)-1;while($l<=$r){$m=intval($l+($r-$l)/2);if($a[$m]==$x)return $m;if($a[$m]<$x)$l=$m+1;else $r=$m-1;}return -1;}',
            'merge_sort'=>'function ms($a){$n=count($a);if($n<2)return $a;$m=intval($n/2);return merge(ms(array_slice($a,0,$m)),ms(array_slice($a,$m)));}',
        ],
    ];
    $hash = hash('sha256',$lang,true);
    $s = 0; for($i=0;$i<8;$i++) $s = ($s<<8) | ord($hash[$i]);
    $size=count($TASKS);
    $indices=[abs(intdiv($s,1))%$size, abs(intdiv($s,7))%$size, abs(intdiv($s,13))%$size];
    $seen=[]; $picked=[];
    foreach($indices as $idx){ if(!isset($seen[$idx])){ $seen[$idx]=1; $picked[]=$TASKS[$idx]; } if(count($picked)==$n) break;}
    // extend if collisions
    for($k=1;count($picked)<$n;$k++){ $idx = (int)(($s + 31*$k) % $size); if(!isset($seen[$idx])){ $seen[$idx]=1; $picked[]=$TASKS[$idx]; } }
    // attach snippet
    foreach($picked as &$t){ $t['snippet']=$SNIPPETS[$lang][$t['name']] ?? 'Snippet not found.'; }
    return $picked;
}

function examples_as_text(string $lang): string {
    $ex = deterministic_examples($lang);
    $b = "# ".strtoupper($lang)." examples\n\n";
    foreach($ex as $t){
        $b .= "## {$t['name']} (difficulty {$t['difficulty']})\n```{$lang}\n{$t['snippet']}\n```\n\n";
    }
    return $b;
}

// ------------------------------ AI plumbing -------------------------------
function make_body(string $prompt, ?string $system, float $temperature, int $topK, int $maxTokens): array {
    $body=[
        'contents' => [[ 'parts' => [[ 'text' => $prompt ]] ]],
        'generationConfig' => [ 'temperature'=>$temperature, 'topK'=>$topK, 'maxOutputTokens'=>$maxTokens ],
    ];
    if ($system) $body['systemInstruction']=['parts'=>[['text'=>$system]]];
    return $body;
}
function extract_text(array $json): ?string {
    if (!isset($json['candidates'][0])) return null;
    $c=$json['candidates'][0];
    if (isset($c['content']['parts']) && is_array($c['content']['parts'])){
        $out=[]; foreach($c['content']['parts'] as $p){ if(isset($p['text'])) $out[]=$p['text']; }
        if ($out) return implode("\n", $out);
    }
    if (isset($c['text']) && is_string($c['text'])) return $c['text'];
    return null;
}
function http_post_json(string $url, string $api_key, array $body, int $timeout, ?string $proxy, ?callable $streamCb): array {
    $qs = (strpos($url,'?')===false)?'?':'&';
    $final = $url.$qs.'key='.urlencode($api_key);
    $ch = curl_init($final);
    $headers=['Content-Type: application/json'];
    curl_setopt_array($ch,[
        CURLOPT_POST=>true, CURLOPT_RETURNTRANSFER=>!$streamCb, CURLOPT_HTTPHEADER=>$headers,
        CURLOPT_POSTFIELDS=>json_encode($body,JSON_UNESCAPED_UNICODE), CURLOPT_TIMEOUT=>$timeout,
    ]);
    if ($proxy) curl_setopt($ch, CURLOPT_PROXY, $proxy);
    if ($streamCb) {
        curl_setopt($ch, CURLOPT_WRITEFUNCTION, function($ch,$data) use($streamCb){ $streamCb($data); return strlen($data); });
    }
    $resp = curl_exec($ch);
    $err  = curl_error($ch);
    $code = curl_getinfo($ch,CURLINFO_HTTP_CODE);
    curl_close($ch);
    return [$code,$err,$resp];
}

function ai_request_once(
    string $prompt, ?string $system, string $url, string $api_key,
    float $temperature, int $topK, int $maxTokens,
    int $timeout, int $retries, int $retry_base_ms, ?string $proxy,
    ?string $cache_dir, bool $want_json, bool $stream, ?string $examplesLang
) {
    $body = make_body($prompt,$system,$temperature,$topK,$maxTokens);
    $cache_key = sha_key($url.'|'.json_encode($body));

    if(!$stream && ($j = maybe_cache_read($cache_dir,$cache_key))){
        if($want_json){ echo json_encode($j,JSON_UNESCAPED_UNICODE).PHP_EOL; return; }
        $t=extract_text($j); if($t!==null){ echo $t.PHP_EOL; return; }
    }

    $attempt=0; $lastBody=null;
    while(true){
        $streamCb = $stream ? function($chunk){ echo $chunk; flush(); } : null;
        [$code,$err,$resp] = http_post_json($url,$api_key,$body,$timeout,$proxy,$streamCb);
        if($stream){
            // In streaming mode we assume the server streamed the payload; still emit newline
            if($code>=400 || $err){ fwrite(STDERR, "\n".red("[stream] HTTP {$code} ".$err)."\n"); }
            echo PHP_EOL; return;
        }
        if($resp===false || $err){
            if($attempt<$retries){ usleep((int)(($retry_base_ms*pow(2,$attempt))*1000)); $attempt++; continue; }
            // Fallback to deterministic examples if requested
            if($examplesLang){ fwrite(STDERR, yellow("AI failed; emitting deterministic examples").PHP_EOL); echo examples_as_text($examplesLang).PHP_EOL; return; }
            fwrite(STDERR, red("Network error: {$err}").PHP_EOL); exit(EXIT_FAIL);
        }
        if($code>=500 || $code==429){
            if($attempt<$retries){ usleep((int)(($retry_base_ms*pow(2,$attempt))*1000)); $attempt++; continue; }
        }
        if($code>=400){
            if($examplesLang){ fwrite(STDERR, yellow("HTTP {$code}; deterministic examples").PHP_EOL); echo examples_as_text($examplesLang).PHP_EOL; return; }
            $snippet = mb_substr((string)$resp,0,400);
            fwrite(STDERR, red("HTTP {$code} error\n{$snippet}").PHP_EOL); exit(EXIT_FAIL);
        }
        $json = json_decode((string)$resp,true);
        if(!is_array($json)){ if($examplesLang){ echo examples_as_text($examplesLang).PHP_EOL; return;} fwrite(STDERR, red("Invalid JSON").PHP_EOL); exit(EXIT_FAIL);} 
        cache_write($cache_dir,$cache_key,$json);
        if($want_json){ echo json_encode($json,JSON_UNESCAPED_UNICODE).PHP_EOL; return; }
        $text = extract_text($json);
        if($text===null){ if($examplesLang){ echo examples_as_text($examplesLang).PHP_EOL; return;} fwrite(STDERR,yellow("No text in response").PHP_EOL); exit(EXIT_FAIL);} 
        echo $text.PHP_EOL; return;
    }
}

// Parallel batch using curl_multi
function ai_request_batch(array $prompts, array $ctx): array {
    $mh = curl_multi_init();
    $handles=[]; $responses=[];
    foreach($prompts as $i=>$p){
        $body = make_body($p['prompt'],$p['system'],$ctx['temperature'],$ctx['topK'],$ctx['maxTokens']);
        $qs = (strpos($ctx['url'],'?')===false)?'?':'&';
        $final = $ctx['url'].$qs.'key='.urlencode($ctx['api_key']);
        $ch = curl_init($final);
        curl_setopt_array($ch,[CURLOPT_POST=>true,CURLOPT_RETURNTRANSFER=>true,CURLOPT_HTTPHEADER=>['Content-Type: application/json'],CURLOPT_POSTFIELDS=>json_encode($body,JSON_UNESCAPED_UNICODE),CURLOPT_TIMEOUT=>$ctx['timeout']]);
        if($ctx['proxy']) curl_setopt($ch,CURLOPT_PROXY,$ctx['proxy']);
        $handles[$i]=$ch; curl_multi_add_handle($mh,$ch);
    }
    do { $status = curl_multi_exec($mh,$active); curl_multi_select($mh,1.0); } while ($active && $status==CURLM_OK);
    foreach($handles as $i=>$ch){
        $code = curl_getinfo($ch,CURLINFO_HTTP_CODE);
        $err  = curl_error($ch);
        $resp = curl_multi_getcontent($ch);
        curl_multi_remove_handle($mh,$ch); curl_close($ch);
        $responses[$i]=[$code,$err,$resp];
    }
    curl_multi_close($mh);
    return $responses;
}

// ------------------------------ Output utils ------------------------------
function sign_if(string $out, ?string $secret): array {
    $res=['data'=>$out];
    if($secret!==null){ $res['signature']=hash_hmac('sha256',$out,$secret); }
    return $res;
}
function encrypt_if(string $out, ?string $keyHex, ?string $ivHex): array {
    if($keyHex===null) return ['ciphertext'=>null,'iv'=>null,'data'=>$out];
    $key = hex2bin($keyHex); if($key===false || strlen($key)!==32){ fwrite(STDERR, red("--encrypt expects 32‑byte key hex (64 chars)").PHP_EOL); exit(EXIT_USAGE);} 
    $iv = $ivHex? hex2bin($ivHex): random_bytes(16); if($iv===false || strlen($iv)!==16){ fwrite(STDERR, red("--iv must be 16‑byte hex (32 chars)").PHP_EOL); exit(EXIT_USAGE);} 
    $cipher = openssl_encrypt($out,'aes-256-cbc',$key,OPENSSL_ZERO_PADDING,$iv);
    if($cipher===false){ fwrite(STDERR, red("OpenSSL encrypt failed").PHP_EOL); exit(EXIT_FAIL);} 
    return ['ciphertext'=>base64_encode($cipher),'iv'=>bin2hex($iv)];
}
function emit_formatted($payload, string $format): void {
    switch($format){
        case 'json': echo json_encode($payload,JSON_UNESCAPED_UNICODE|JSON_PRETTY_PRINT).PHP_EOL; break;
        case 'markdown': echo (is_array($payload)&&isset($payload['data']))?$payload['data']: (string)$payload; break;
        case 'html': echo '<pre>'.htmlspecialchars(is_array($payload)&&isset($payload['data'])?$payload['data']:(string)$payload,ENT_QUOTES|ENT_SUBSTITUTE,'UTF-8')."</pre>\n"; break;
        case 'csv':
            $f = fopen('php://output','w');
            if(is_array($payload)) fputcsv($f,array_keys($payload));
            if(is_array($payload)) fputcsv($f,array_values($payload));
            fclose($f); break;
        case 'yaml': echo to_yaml($payload); break;
        default: echo (is_array($payload)&&isset($payload['data']))?$payload['data']: (string)$payload; if(substr((string)$payload,-1)!=="\n") echo "\n"; break;
    }
}

// ------------------------------- HTTP mode --------------------------------
if (php_sapi_name()==='cli-server') {
    // Simple microservice: POST /api with JSON body { ask | search | prompts[] | options }
    $path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);
    if ($path==='/api' && $_SERVER['REQUEST_METHOD']==='POST'){
        $input = json_decode(file_get_contents('php://input'), true) ?? [];
        $auth = $input['auth'] ?? getenv('GEMINI_API_KEY');
        if(!$auth){ http_response_code(400); echo json_encode(['error'=>'Missing API key']); return; }
        $model = $input['model'] ?? 'gemini-pro';
        $url   = build_url($model, $input['api'] ?? null);
        $system= $input['system'] ?? null;
        $temperature = (float)($input['temperature'] ?? 0.7);
        $topK        = (int)  ($input['topK'] ?? 40);
        $maxTokens   = (int)  ($input['maxTokens'] ?? 1024);
        $timeout     = (int)  ($input['timeout'] ?? 30);
        $retries     = (int)  ($input['retries'] ?? 2);
        $retry_base  = (int)  ($input['retryBase'] ?? 400);
        $proxy       = $input['proxy'] ?? null;
        $want_json   = true; // HTTP always JSON
        $stream      = false;
        $examplesLang= $input['examples'] ?? null;

        header('Content-Type: application/json');
        ob_start();
        $prompt = $input['ask'] ?? ($input['search'] ?? null);
        if ($prompt===null && isset($input['prompts']) && is_array($input['prompts'])){
            // batch mode
            $prompts=[]; foreach($input['prompts'] as $p){ $prompts[]=['prompt'=>$p,'system'=>$system]; }
            $ctx=['url'=>$url,'api_key'=>$auth,'temperature'=>$temperature,'topK'=>$topK,'maxTokens'=>$maxTokens,'timeout'=>$timeout,'proxy'=>$proxy];
            $res = ai_request_batch($prompts,$ctx);
            $out=[]; foreach($res as $i=>[$code,$err,$resp]){ $j=json_decode((string)$resp,true); $out[$i]=['code'=>$code,'err'=>$err,'text'=>extract_text($j??[])]; }
            echo json_encode(['results'=>$out],JSON_UNESCAPED_UNICODE);
        } else {
            ai_request_once($prompt,$system,$url,$auth,$temperature,$topK,$maxTokens,$timeout,$retries,$retry_base,$proxy,null,$want_json,$stream,$examplesLang);
        }
        $raw = ob_get_clean();
        echo $raw; return; // done
    }
    // Anything else → tiny index
    header('Content-Type: text/plain');
    echo "Ultra‑Turbo AI microservice is running. POST JSON to /api"; return;
}

// ------------------------------- CLI parse --------------------------------
$args = array_slice($argv,1);
$flags = [
    'ask'=>null,'search'=>null,'stdin'=>false,'file'=>null,'batch'=>null,
    'auth'=>null,'env'=>false,'config'=>null,
    'model'=>'gemini-pro','api'=>null,'system'=>null,
    'temperature'=>0.7,'topK'=>40,'maxTokens'=>1024,
    'timeout'=>30,'json'=>false,'format'=>'text',
    'cache'=>cache_dir_default(),'no-cache'=>false,'retries'=>2,'retry-base'=>400,'proxy'=>null,
    'stream'=>false,'parallel'=>4,'examples'=>null,'sign'=>null,'encrypt'=>null,'iv'=>null,
    'mode'=>'online', // online|offline|local
    'set-mode'=>null,
];
for($i=0;$i<count($args);$i++){
    $a=$args[$i];
    switch($a){
        case '--ask': $flags['ask']=$args[++$i]??null; break;
        case '--search': $flags['search']=$args[++$i]??null; break;
        case '--stdin': $flags['stdin']=true; break;
        case '--file': $flags['file']=$args[++$i]??null; break;
        case '--batch': $flags['batch']=$args[++$i]??null; break;
        case '--auth': $flags['auth']=$args[++$i]??null; break;
        case '--env': $flags['env']=true; break;
        case '--config': $flags['config']=$args[++$i]??null; break;
        case '--model': $flags['model']=$args[++$i]??'gemini-pro'; break;
        case '--api': $flags['api']=$args[++$i]??null; break;
        case '--system': $flags['system']=$args[++$i]??null; break;
        case '--temperature': $flags['temperature']=(float)($args[++$i]??0.7); break;
        case '--topK': $flags['topK']=(int)($args[++$i]??40); break;
        case '--maxTokens': $flags['maxTokens']=(int)($args[++$i]??1024); break;
        case '--timeout': $flags['timeout']=max(1,(int)($args[++$i]??30)); break;
        case '--json': $flags['json']=true; $flags['format']='json'; break;
        case '--format': $flags['format']=$args[++$i]??'text'; break;
        case '--cache': $flags['cache']=$args[++$i]??$flags['cache']; break;
        case '--no-cache': $flags['no-cache']=true; break;
        case '--retries': $flags['retries']=max(0,(int)($args[++$i]??2)); break;
        case '--retry-base': $flags['retry-base']=max(50,(int)($args[++$i]??400)); break;
        case '--proxy': $flags['proxy']=$args[++$i]??null; break;
        case '--stream': $flags['stream']=true; break;
        case '--parallel': $flags['parallel']=max(1,(int)($args[++$i]??4)); break;
        case '--examples': $flags['examples']=$args[++$i]??'java'; break;
        case '--sign': $flags['sign']=$args[++$i]??null; break;
        case '--encrypt': $flags['encrypt']=$args[++$i]??null; break;
        case '--iv': $flags['iv']=$args[++$i]??null; break;
        case '--mode': $flags['mode']=$args[++$i]??'online'; break;
        case '--set-mode': $flags['set-mode']=$args[++$i]??null; break;
        case '--help': case '-h': usage(); exit(0);
        default: fwrite(STDERR, red("Unknown flag: {$a}").PHP_EOL); usage(); exit(EXIT_USAGE);
    }
}

// Persisted mode override
if ($flags['set-mode'] !== null) {
    $m = strtolower($flags['set-mode']);
    if (!in_array($m, ['offline','online','local'], true)) {
        fwrite(STDERR, red("Invalid --set-mode (use offline|online|local)").PHP_EOL); exit(EXIT_USAGE);
    }
    $st = load_state(); $st['default_mode'] = $m; save_state($st);
    fwrite(STDOUT, "Default mode set to: {$m}\n");
    // continue; does not exit: you might also run a request now
}

$st = load_state();
$default_mode = $st['default_mode'] ?? 'online';
$mode = strtolower($flags['mode'] ?? $default_mode);
if (!in_array($mode, ['offline','online','local'], true)) $mode = 'online';

// Must have some input mode
$has_task = ($flags['ask']!==null)||($flags['search']!==null)||$flags['stdin']||($flags['file']!==null)||($flags['batch']!==null);
if(!$has_task){ fwrite(STDERR,yellow("Provide --ask/--search/--stdin/--file/--batch").PHP_EOL); usage(); exit(EXIT_USAGE);} 

// Resolve config + API key
$cfg = load_config($flags['config']);
$api_key = $flags['auth'] ?? ($cfg['apiKey'] ?? null);
if($api_key===null && $flags['env']){ $env=getenv('GEMINI_API_KEY'); if($env) $api_key=$env; }
if($api_key===null){ fwrite(STDERR, red("Missing API key. Use --auth or --env or --config").PHP_EOL); exit(EXIT_USAGE);} 

$model = $flags['model'] ?? ($cfg['model'] ?? 'gemini-pro');
$url   = build_url($model, $flags['api'] ?? ($cfg['api'] ?? null));
$system= $flags['system'] ?? ($cfg['system'] ?? null);

// Mode routing
if ($mode === 'local') {
    // Prefer explicit flag/env over defaults
    $localApi = $flags['api'] ?? getenv('LOCAL_AI_API') ?: 'http://127.0.0.1:8080/api';
    $url = $localApi; // expects same JSON contract: generateContent-like endpoint or your microservice /api
}
$temperature=(float)$flags['temperature']; $topK=(int)$flags['topK']; $maxTokens=(int)$flags['maxTokens'];
$timeout=(int)$flags['timeout']; $retries=(int)$flags['retries']; $retry_base=(int)$flags['retry-base'];
$proxy=$flags['proxy']; $cache_dir=$flags['no-cache']?null:($flags['cache']??null);
$want_json = ($flags['format']==='json');

// Gather requests
$reqs=[];
if($flags['ask']!==null){ $reqs[]=['label'=>'AI reply','prompt'=>$flags['ask'],'system'=>$system]; }
if($flags['search']!==null){ $reqs[]=['label'=>'AI Search Results','prompt'=>'Using your knowledge, synthesize a concise answer for: '.$flags['search'],'system'=>$system]; }
if($flags['stdin']){ $stdin=stream_get_contents(STDIN); if($stdin!==false && trim($stdin)!=='') $reqs[]=['label'=>'AI reply (stdin)','prompt'=>trim($stdin),'system'=>$system]; }
if($flags['file']!==null){ $reqs[]=['label'=>'AI reply (file)','prompt'=>read_file_trim($flags['file']),'system'=>$system]; }

// Batch from file
if($flags['batch']!==null){
    $path=$flags['batch']; if(!is_file($path)){ fwrite(STDERR, red("Batch file not found: {$path}").PHP_EOL); exit(EXIT_FAIL);} 
    $ext=strtolower(pathinfo($path,PATHINFO_EXTENSION));
    $lines=preg_split("/(\r\n|\r|\n)/", (string)file_get_contents($path));
    foreach($lines as $line){ $line=trim($line); if($line==='') continue; if($ext==='jsonl'||$ext==='json'){ $row=json_decode($line,true); if(isset($row['prompt'])) $reqs[]=['label'=>'AI batch','prompt'=>$row['prompt'],'system'=>$row['system']??$system,'batch'=>true]; }
        else { $parts=explode("\t",$line,2); $p=$parts[0]??''; $s=$parts[1]??$system; if(trim($p)!=='') $reqs[]=['label'=>'AI batch','prompt'=>$p,'system'=>$s,'batch'=>true]; }
    }
}

// Execute
foreach($reqs as $idx=>$r){
    $is_batch = !empty($r['batch']);
    if(!$want_json && !$is_batch) fwrite(STDOUT, cyan($r['label'].':').PHP_EOL);
    
    if ($mode === 'offline') {
        $lang = $flags['examples'] ?? 'java';
        $out = examples_as_text($lang);
        echo $out.PHP_EOL;
        continue; // skip network work for this request
    }
    
    ai_request_once(
        $r['prompt'],$r['system'],$url,$api_key,
        $temperature,$topK,$maxTokens,
        $timeout,$retries,$retry_base,$proxy,
        $cache_dir,$want_json,$flags['stream'],$flags['examples']
    );
}

// If batching with many prompts and parallel desired, offer fast path
if($flags['batch']!==null){
    if ($mode === 'offline') {
        $lang = $flags['examples'] ?? 'java';
        $results = [];
        foreach($reqs as $r){ if(!empty($r['batch'])) $results[] = ['code'=>0,'err'=>null,'text'=>examples_as_text($lang)]; }
        $payload=['results'=>$results];
        if($flags['sign']!==null){ $payload['signature']=hash_hmac('sha256',json_encode($results,JSON_UNESCAPED_UNICODE),$flags['sign']); }
        if($flags['encrypt']!==null){ $enc=encrypt_if(json_encode($results,JSON_UNESCAPED_UNICODE),$flags['encrypt'],$flags['iv']); $payload=['ciphertext'=>$enc['ciphertext'],'iv'=>$enc['iv']]; }
        emit_formatted($payload, $flags['format']); 
        exit(0);
    }
    
    // regroup prompts (those marked as batch)
    $batchPrompts=[]; foreach($reqs as $r){ if(!empty($r['batch'])) $batchPrompts[]=['prompt'=>$r['prompt'],'system'=>$r['system']]; }
    $chunks=array_chunk($batchPrompts, max(1,(int)$flags['parallel']));
    $results=[]; foreach($chunks as $chunk){
        $ctx=['url'=>$url,'api_key'=>$api_key,'temperature'=>$temperature,'topK'=>$topK,'maxTokens'=>$maxTokens,'timeout'=>$timeout,'proxy'=>$proxy];
        $res=ai_request_batch($chunk,$ctx);
        foreach($res as [$code,$err,$resp]){ $j=json_decode((string)$resp,true); $results[]=['code'=>$code,'err'=>$err,'text'=>extract_text($j??[])]; }
    }
    // Post-process: signing/encryption/format
    $payload=['results'=>$results];
    if($flags['sign']!==null){ $payload['signature']=hash_hmac('sha256',json_encode($results,JSON_UNESCAPED_UNICODE),$flags['sign']); }
    if($flags['encrypt']!==null){ $enc=encrypt_if(json_encode($results,JSON_UNESCAPED_UNICODE),$flags['encrypt'],$flags['iv']); $payload=['ciphertext'=>$enc['ciphertext'],'iv'=>$enc['iv']]; }
    emit_formatted($payload, $flags['format']);
}

// For single requests, optional signing/encryption (applies to last emitted line only)
// This is best used when --json to capture exact payload. For text formats, prefer batch block above.
// (No-op here to keep stdout clean — signing/encryption demonstrated in batch payload.)

?>
