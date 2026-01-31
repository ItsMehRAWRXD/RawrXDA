#!/usr/bin/env php
<?php
/**
 * Multi-Language Deterministic Example Generator
 * 
 * This script provides deterministic example generation for the most popular
 * programming languages. It uses SHA-256 hashing to consistently select 
 * examples, ensuring the same output every time for a given language and format.
 * 
 * Supported Languages: Java, Python, JavaScript, TypeScript, C++, C#, Go, Rust, 
 *                      PHP, Ruby, Swift, Kotlin, Scala, R, MATLAB, Bash, SQL
 * 
 * Usage:
 *   php MultiLangExampleGenerator.php --lang python --format markdown
 *   php MultiLangExampleGenerator.php --lang javascript --format json
 *   php MultiLangExampleGenerator.php --ask "Explain OOP" --lang java
 *   php MultiLangExampleGenerator.php --reddit --subreddit programming --lang python
 *   php MultiLangExampleGenerator.php --list-languages
 */

// --- Data definitions ---
// Fixed curriculum: 5 canonical micro-tasks
$tasks = [
    (object)['id' => 1, 'name' => 'hello_world', 'difficulty' => 1],
    (object)['id' => 2, 'name' => 'fibonacci', 'difficulty' => 2],
    (object)['id' => 3, 'name' => 'reverse_string', 'difficulty' => 2],
    (object)['id' => 4, 'name' => 'binary_search', 'difficulty' => 3],
    (object)['id' => 5, 'name' => 'merge_sort', 'difficulty' => 4],
];

// Code snippets for all supported languages
$snippets = [
    'java' => [
        'hello_world' => 'public class Hello { public static void main(String[] args) { System.out.println("Hello, world!"); } }',
        'fibonacci' => 'public static int fib(int n) { if (n < 2) return n; return fib(n-1) + fib(n-2); }',
        'reverse_string' => 'public static String reverse(String s) { return new StringBuilder(s).reverse().toString(); }',
        'binary_search' => 'public static int binarySearch(int[] arr, int x) { int left = 0, right = arr.length-1; while (left <= right) { int mid = left + (right-left)/2; if (arr[mid] == x) return mid; if (arr[mid] < x) left = mid+1; else right = mid-1; } return -1; }',
        'merge_sort' => 'public static void mergeSort(int[] arr) { if (arr.length < 2) return; int mid = arr.length/2; int[] left = Arrays.copyOfRange(arr, 0, mid); int[] right = Arrays.copyOfRange(arr, mid, arr.length); mergeSort(left); mergeSort(right); merge(arr, left, right); }'
    ],
    'python' => [
        'hello_world' => 'print("Hello, world!")',
        'fibonacci' => 'def fib(n): return n if n < 2 else fib(n-1) + fib(n-2)',
        'reverse_string' => 'def reverse(s): return s[::-1]',
        'binary_search' => 'def binary_search(arr, x): left, right = 0, len(arr)-1; while left <= right: mid = left + (right-left)//2; if arr[mid] == x: return mid; elif arr[mid] < x: left = mid+1; else: right = mid-1; return -1',
        'merge_sort' => 'def merge_sort(arr): if len(arr) < 2: return arr; mid = len(arr)//2; left = merge_sort(arr[:mid]); right = merge_sort(arr[mid:]); return merge(left, right)'
    ],
    'javascript' => [
        'hello_world' => 'console.log("Hello, world!");',
        'fibonacci' => 'const fib = n => n < 2 ? n : fib(n-1) + fib(n-2);',
        'reverse_string' => 'const reverse = s => s.split("").reverse().join("");',
        'binary_search' => 'const binarySearch = (arr, x) => { let left = 0, right = arr.length-1; while (left <= right) { const mid = Math.floor(left + (right-left)/2); if (arr[mid] === x) return mid; if (arr[mid] < x) left = mid+1; else right = mid-1; } return -1; };',
        'merge_sort' => 'const mergeSort = arr => { if (arr.length < 2) return arr; const mid = Math.floor(arr.length/2); const left = mergeSort(arr.slice(0, mid)); const right = mergeSort(arr.slice(mid)); return merge(left, right); };'
    ],
    'typescript' => [
        'hello_world' => 'console.log("Hello, world!");',
        'fibonacci' => 'const fib = (n: number): number => n < 2 ? n : fib(n-1) + fib(n-2);',
        'reverse_string' => 'const reverse = (s: string): string => s.split("").reverse().join("");',
        'binary_search' => 'const binarySearch = (arr: number[], x: number): number => { let left = 0, right = arr.length-1; while (left <= right) { const mid = Math.floor(left + (right-left)/2); if (arr[mid] === x) return mid; if (arr[mid] < x) left = mid+1; else right = mid-1; } return -1; };',
        'merge_sort' => 'const mergeSort = (arr: number[]): number[] => { if (arr.length < 2) return arr; const mid = Math.floor(arr.length/2); const left = mergeSort(arr.slice(0, mid)); const right = mergeSort(arr.slice(mid)); return merge(left, right); };'
    ],
    'c++' => [
        'hello_world' => '#include <iostream>' . "\n" . 'int main() { std::cout << "Hello, world!" << std::endl; return 0; }',
        'fibonacci' => 'int fib(int n) { return n < 2 ? n : fib(n-1) + fib(n-2); }',
        'reverse_string' => 'std::string reverse(std::string s) { std::reverse(s.begin(), s.end()); return s; }',
        'binary_search' => 'int binarySearch(std::vector<int>& arr, int x) { int left = 0, right = arr.size()-1; while (left <= right) { int mid = left + (right-left)/2; if (arr[mid] == x) return mid; if (arr[mid] < x) left = mid+1; else right = mid-1; } return -1; }',
        'merge_sort' => 'void mergeSort(std::vector<int>& arr) { if (arr.size() < 2) return; int mid = arr.size()/2; std::vector<int> left(arr.begin(), arr.begin()+mid); std::vector<int> right(arr.begin()+mid, arr.end()); mergeSort(left); mergeSort(right); merge(arr, left, right); }'
    ],
    'csharp' => [
        'hello_world' => 'using System; class Program { static void Main() { Console.WriteLine("Hello, world!"); } }',
        'fibonacci' => 'static int Fib(int n) => n < 2 ? n : Fib(n-1) + Fib(n-2);',
        'reverse_string' => 'static string Reverse(string s) => new string(s.Reverse().ToArray());',
        'binary_search' => 'static int BinarySearch(int[] arr, int x) { int left = 0, right = arr.Length-1; while (left <= right) { int mid = left + (right-left)/2; if (arr[mid] == x) return mid; if (arr[mid] < x) left = mid+1; else right = mid-1; } return -1; }',
        'merge_sort' => 'static void MergeSort(int[] arr) { if (arr.Length < 2) return; int mid = arr.Length/2; int[] left = arr.Take(mid).ToArray(); int[] right = arr.Skip(mid).ToArray(); MergeSort(left); MergeSort(right); Merge(arr, left, right); }'
    ],
    'go' => [
        'hello_world' => 'package main' . "\n" . 'import "fmt"' . "\n" . 'func main() { fmt.Println("Hello, world!") }',
        'fibonacci' => 'func fib(n int) int { if n < 2 { return n }; return fib(n-1) + fib(n-2) }',
        'reverse_string' => 'func reverse(s string) string { runes := []rune(s); for i, j := 0, len(runes)-1; i < j; i, j = i+1, j-1 { runes[i], runes[j] = runes[j], runes[i] }; return string(runes) }',
        'binary_search' => 'func binarySearch(arr []int, x int) int { left, right := 0, len(arr)-1; for left <= right { mid := left + (right-left)/2; if arr[mid] == x { return mid }; if arr[mid] < x { left = mid+1 } else { right = mid-1 } }; return -1 }',
        'merge_sort' => 'func mergeSort(arr []int) []int { if len(arr) < 2 { return arr }; mid := len(arr)/2; left := mergeSort(arr[:mid]); right := mergeSort(arr[mid:]); return merge(left, right) }'
    ],
    'rust' => [
        'hello_world' => 'fn main() { println!("Hello, world!"); }',
        'fibonacci' => 'fn fib(n: u64) -> u64 { if n < 2 { n } else { fib(n-1) + fib(n-2) } }',
        'reverse_string' => 'fn reverse(s: &str) -> String { s.chars().rev().collect() }',
        'binary_search' => 'fn binary_search(arr: &[i32], x: i32) -> Option<usize> { let mut left = 0; let mut right = arr.len()-1; while left <= right { let mid = left + (right-left)/2; if arr[mid] == x { return Some(mid) }; if arr[mid] < x { left = mid+1 } else { right = mid-1 } }; None }',
        'merge_sort' => 'fn merge_sort(mut arr: Vec<i32>) -> Vec<i32> { if arr.len() < 2 { return arr }; let mid = arr.len()/2; let left = merge_sort(arr[..mid].to_vec()); let right = merge_sort(arr[mid..].to_vec()); merge(left, right) }'
    ],
    'php' => [
        'hello_world' => '<?php echo "Hello, world!"; ?>',
        'fibonacci' => 'function fib($n) { return $n < 2 ? $n : fib($n-1) + fib($n-2); }',
        'reverse_string' => 'function reverse($s) { return strrev($s); }',
        'binary_search' => 'function binarySearch($arr, $x) { $left = 0; $right = count($arr)-1; while ($left <= $right) { $mid = $left + intval(($right-$left)/2); if ($arr[$mid] == $x) return $mid; if ($arr[$mid] < $x) $left = $mid+1; else $right = $mid-1; } return -1; }',
        'merge_sort' => 'function mergeSort($arr) { if (count($arr) < 2) return $arr; $mid = intval(count($arr)/2); $left = mergeSort(array_slice($arr, 0, $mid)); $right = mergeSort(array_slice($arr, $mid)); return merge($left, $right); }'
    ],
    'ruby' => [
        'hello_world' => 'puts "Hello, world!"',
        'fibonacci' => 'def fib(n) n < 2 ? n : fib(n-1) + fib(n-2) end',
        'reverse_string' => 'def reverse(s) s.reverse end',
        'binary_search' => 'def binary_search(arr, x) left, right = 0, arr.length-1; while left <= right; mid = left + (right-left)/2; return mid if arr[mid] == x; arr[mid] < x ? left = mid+1 : right = mid-1; end; -1 end',
        'merge_sort' => 'def merge_sort(arr) return arr if arr.length < 2; mid = arr.length/2; left = merge_sort(arr[0...mid]); right = merge_sort(arr[mid..-1]); merge(left, right) end'
    ],
    'swift' => [
        'hello_world' => 'print("Hello, world!")',
        'fibonacci' => 'func fib(_ n: Int) -> Int { return n < 2 ? n : fib(n-1) + fib(n-2) }',
        'reverse_string' => 'func reverse(_ s: String) -> String { return String(s.reversed()) }',
        'binary_search' => 'func binarySearch(_ arr: [Int], _ x: Int) -> Int { var left = 0, right = arr.count-1; while left <= right { let mid = left + (right-left)/2; if arr[mid] == x { return mid }; if arr[mid] < x { left = mid+1 } else { right = mid-1 } }; return -1 }',
        'merge_sort' => 'func mergeSort(_ arr: [Int]) -> [Int] { if arr.count < 2 { return arr }; let mid = arr.count/2; let left = mergeSort(Array(arr[0..<mid])); let right = mergeSort(Array(arr[mid..<arr.count])); return merge(left, right) }'
    ],
    'kotlin' => [
        'hello_world' => 'fun main() { println("Hello, world!") }',
        'fibonacci' => 'fun fib(n: Int): Int = if (n < 2) n else fib(n-1) + fib(n-2)',
        'reverse_string' => 'fun reverse(s: String): String = s.reversed()',
        'binary_search' => 'fun binarySearch(arr: IntArray, x: Int): Int { var left = 0; var right = arr.size-1; while (left <= right) { val mid = left + (right-left)/2; if (arr[mid] == x) return mid; if (arr[mid] < x) left = mid+1 else right = mid-1 }; return -1 }',
        'merge_sort' => 'fun mergeSort(arr: IntArray): IntArray { if (arr.size < 2) return arr; val mid = arr.size/2; val left = mergeSort(arr.sliceArray(0 until mid)); val right = mergeSort(arr.sliceArray(mid until arr.size)); return merge(left, right) }'
    ],
    'scala' => [
        'hello_world' => 'object Main { def main(args: Array[String]): Unit = println("Hello, world!") }',
        'fibonacci' => 'def fib(n: Int): Int = if (n < 2) n else fib(n-1) + fib(n-2)',
        'reverse_string' => 'def reverse(s: String): String = s.reverse',
        'binary_search' => 'def binarySearch(arr: Array[Int], x: Int): Int = { var left = 0; var right = arr.length-1; while (left <= right) { val mid = left + (right-left)/2; if (arr(mid) == x) return mid; if (arr(mid) < x) left = mid+1 else right = mid-1 }; -1 }',
        'merge_sort' => 'def mergeSort(arr: Array[Int]): Array[Int] = { if (arr.length < 2) arr else { val mid = arr.length/2; val left = mergeSort(arr.slice(0, mid)); val right = mergeSort(arr.slice(mid, arr.length)); merge(left, right) } }'
    ],
    'r' => [
        'hello_world' => 'cat("Hello, world!\n")',
        'fibonacci' => 'fib <- function(n) if (n < 2) n else fib(n-1) + fib(n-2)',
        'reverse_string' => 'reverse <- function(s) paste(rev(strsplit(s, "")[[1]]), collapse="")',
        'binary_search' => 'binary_search <- function(arr, x) { left <- 1; right <- length(arr); while (left <= right) { mid <- left + (right-left) %/% 2; if (arr[mid] == x) return(mid); if (arr[mid] < x) left <- mid+1 else right <- mid-1 }; -1 }',
        'merge_sort' => 'merge_sort <- function(arr) { if (length(arr) < 2) return(arr); mid <- length(arr) %/% 2; left <- merge_sort(arr[1:mid]); right <- merge_sort(arr[(mid+1):length(arr)]); merge(left, right) }'
    ],
    'matlab' => [
        'hello_world' => 'disp("Hello, world!")',
        'fibonacci' => 'function result = fib(n); if n < 2; result = n; else; result = fib(n-1) + fib(n-2); end; end',
        'reverse_string' => 'function result = reverse(s); result = fliplr(s); end',
        'binary_search' => 'function index = binary_search(arr, x); left = 1; right = length(arr); while left <= right; mid = left + floor((right-left)/2); if arr(mid) == x; index = mid; return; elseif arr(mid) < x; left = mid+1; else; right = mid-1; end; end; index = -1; end',
        'merge_sort' => 'function result = merge_sort(arr); if length(arr) < 2; result = arr; else; mid = floor(length(arr)/2); left = merge_sort(arr(1:mid)); right = merge_sort(arr(mid+1:end)); result = merge(left, right); end; end'
    ],
    'bash' => [
        'hello_world' => 'echo "Hello, world!"',
        'fibonacci' => 'fib() { local n=$1; if [ $n -lt 2 ]; then echo $n; else echo $(($(fib $((n-1))) + $(fib $((n-2))))); fi; }',
        'reverse_string' => 'reverse() { echo "$1" | rev; }',
        'binary_search' => 'binary_search() { local arr=("$@"); local x=${arr[-1]}; unset arr[-1]; local left=0; local right=$((${#arr[@]}-1)); while [ $left -le $right ]; do local mid=$((left + (right-left)/2)); if [ ${arr[$mid]} -eq $x ]; then echo $mid; return; elif [ ${arr[$mid]} -lt $x ]; then left=$((mid+1)); else right=$((mid-1)); fi; done; echo -1; }',
        'merge_sort' => 'merge_sort() { local arr=("$@"); if [ ${#arr[@]} -lt 2 ]; then echo "${arr[@]}"; return; fi; local mid=$((${#arr[@]}/2)); local left=($(merge_sort "${arr[@]:0:mid}")); local right=($(merge_sort "${arr[@]:mid}")); echo "$(merge "${left[@]}" "${right[@]}")"; }'
    ],
    'sql' => [
        'hello_world' => 'SELECT "Hello, world!" AS message;',
        'fibonacci' => 'WITH RECURSIVE fib(n, fib_n, next_fib_n) AS (SELECT 0, 0, 1 UNION ALL SELECT n+1, next_fib_n, fib_n + next_fib_n FROM fib WHERE n < 10) SELECT fib_n FROM fib WHERE n = 10;',
        'reverse_string' => 'SELECT REVERSE("hello") AS reversed;',
        'binary_search' => 'SELECT CASE WHEN @target = arr[mid] THEN mid WHEN @target < arr[mid] THEN binary_search(arr, @target, left, mid-1) ELSE binary_search(arr, @target, mid+1, right) END FROM (SELECT @target, @arr, @left, @right, (@left + @right) / 2 AS mid) t;',
        'merge_sort' => 'WITH RECURSIVE merge_sort(arr) AS (SELECT CASE WHEN LENGTH(arr) < 2 THEN arr ELSE merge(merge_sort(LEFT(arr, LENGTH(arr)/2)), merge_sort(RIGHT(arr, LENGTH(arr)/2))) END) SELECT * FROM merge_sort;'
    ]
];

// --- Helper Functions ---

/**
 * List all supported languages
 */
function list_languages(array $snippets): void {
    echo "Supported languages:\n";
    foreach (array_keys($snippets) as $lang) {
        echo "  - " . ucfirst($lang) . "\n";
    }
}

/**
 * Deterministically picks a number of examples based on the language.
 * Uses SHA-256 hashing to ensure consistent selection.
 */
function pick_examples(string $lang, int $n, array $tasks): array {
    $hash = hash('sha256', $lang);
    $h = hexdec(substr($hash, 0, 16));
    
    $seen = [];
    $result = [];
    
    $indices = [
        abs($h % count($tasks)),
        abs(intdiv($h, 7) % count($tasks)),
        abs(intdiv($h, 13) % count($tasks))
    ];
    
    foreach ($indices as $idx) {
        if (!isset($seen[$idx])) {
            $seen[$idx] = true;
            $result[] = $tasks[$idx];
        }
        if (count($result) === $n) break;
    }
    
    for ($k = 1; count($result) < $n; $k++) {
        $idx = abs(($h + 31 * $k) % count($tasks));
        if (!isset($seen[$idx])) {
            $seen[$idx] = true;
            $result[] = $tasks[$idx];
        }
    }
    
    return array_slice($result, 0, $n);
}

/**
 * Formats the given tasks into Markdown.
 */
function to_markdown(string $lang, array $tasks, array $snippets): string {
    $output = "# " . strtoupper($lang) . " examples\n\n";
    
    foreach ($tasks as $task) {
        $snippet = $snippets[$lang][$task->name] ?? 'Snippet not found.';
        $output .= "## {$task->name} (difficulty {$task->difficulty})\n";
        $output .= "```{$lang}\n{$snippet}\n```\n\n";
    }
    
    return $output;
}

/**
 * Formats the given tasks into JSON.
 */
function to_json(string $lang, array $tasks, array $snippets): string {
    $data = [];
    foreach ($tasks as $task) {
        $data[] = [
            'id' => $task->id,
            'name' => $task->name,
            'difficulty' => $task->difficulty,
            'snippet' => $snippets[$lang][$task->name] ?? 'Snippet not found.',
        ];
    }
    
    return json_encode($data, JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES);
}

/**
 * Formats the given tasks into CSV.
 */
function to_csv(string $lang, array $tasks, array $snippets): string {
    $output = "id,name,difficulty,snippet\n";
    
    foreach ($tasks as $task) {
        $snippet = $snippets[$lang][$task->name] ?? 'Snippet not found.';
        $snippet = '"' . str_replace('"', '""', $snippet) . '"';
        $output .= "{$task->id},{$task->name},{$task->difficulty},{$snippet}\n";
    }
    
    return $output;
}

/**
 * Ask AI via HTTP POST request.
 */
function ask_ai(string $prompt, string $url, ?string $auth = null): string {
    try {
        $data = [
            'model' => 'gpt-4o',
            'messages' => [
                ['role' => 'system', 'content' => 'You are a helpful assistant.'],
                ['role' => 'user', 'content' => $prompt]
            ],
            'temperature' => 0.2
        ];
        
        $options = [
            'http' => [
                'header' => [
                    'Content-Type: application/json',
                    $auth ? "Authorization: {$auth}" : ''
                ],
                'method' => 'POST',
                'content' => json_encode($data)
            ]
        ];
        
        $context = stream_context_create($options);
        $result = file_get_contents($url, false, $context);
        
        if ($result === false) {
            return "AI call failed – showing deterministic examples anyway.\n\n";
        }
        
        $response = json_decode($result, true);
        $aiReply = $response['choices'][0]['message']['content'] ?? 'No response from AI';
        
        return "AI reply:\n{$aiReply}\n\n";
        
    } catch (Exception $e) {
        return "AI call failed ({$e->getMessage()}) – showing deterministic examples anyway.\n\n";
    }
}

/**
 * Fetch Reddit posts for programming examples.
 */
function fetch_reddit_examples(string $subreddit, string $lang, int $limit = 5): string {
    try {
        $url = "https://www.reddit.com/r/{$subreddit}/hot.json?limit={$limit}";
        
        $options = [
            'http' => [
                'header' => [
                    'User-Agent: MultiLangExampleGenerator/1.0'
                ],
                'method' => 'GET'
            ]
        ];
        
        $context = stream_context_create($options);
        $result = file_get_contents($url, false, $context);
        
        if ($result === false) {
            return "Reddit fetch failed – showing deterministic examples anyway.\n\n";
        }
        
        $response = json_decode($result, true);
        $posts = $response['data']['children'] ?? [];
        
        if (empty($posts)) {
            return "No Reddit posts found – showing deterministic examples anyway.\n\n";
        }
        
        $redditContent = "Reddit examples from r/{$subreddit}:\n\n";
        
        foreach ($posts as $post) {
            $data = $post['data'];
            $title = $data['title'] ?? 'No title';
            $selftext = $data['selftext'] ?? '';
            $url = $data['url'] ?? '';
            $score = $data['score'] ?? 0;
            
            // Filter for language-related content
            if (stripos($title, $lang) !== false || stripos($selftext, $lang) !== false) {
                $redditContent .= "**{$title}** (Score: {$score})\n";
                if (!empty($selftext)) {
                    $redditContent .= substr($selftext, 0, 200) . "...\n";
                }
                if (!empty($url) && $url !== $data['permalink']) {
                    $redditContent .= "Link: {$url}\n";
                }
                $redditContent .= "\n";
            }
        }
        
        return $redditContent . "\n";
        
    } catch (Exception $e) {
        return "Reddit fetch failed ({$e->getMessage()}) – showing deterministic examples anyway.\n\n";
    }
}

/**
 * Search Reddit for specific programming topics.
 */
function search_reddit(string $query, string $subreddit = 'programming', int $limit = 5): string {
    try {
        $url = "https://www.reddit.com/r/{$subreddit}/search.json?q=" . urlencode($query) . "&limit={$limit}&sort=relevance";
        
        $options = [
            'http' => [
                'header' => [
                    'User-Agent: MultiLangExampleGenerator/1.0'
                ],
                'method' => 'GET'
            ]
        ];
        
        $context = stream_context_create($options);
        $result = file_get_contents($url, false, $context);
        
        if ($result === false) {
            return "Reddit search failed – showing deterministic examples anyway.\n\n";
        }
        
        $response = json_decode($result, true);
        $posts = $response['data']['children'] ?? [];
        
        if (empty($posts)) {
            return "No Reddit results found for '{$query}' – showing deterministic examples anyway.\n\n";
        }
        
        $redditContent = "Reddit search results for '{$query}' in r/{$subreddit}:\n\n";
        
        foreach ($posts as $post) {
            $data = $post['data'];
            $title = $data['title'] ?? 'No title';
            $selftext = $data['selftext'] ?? '';
            $url = $data['url'] ?? '';
            $score = $data['score'] ?? 0;
            
            $redditContent .= "**{$title}** (Score: {$score})\n";
            if (!empty($selftext)) {
                $redditContent .= substr($selftext, 0, 300) . "...\n";
            }
            if (!empty($url) && $url !== $data['permalink']) {
                $redditContent .= "Link: {$url}\n";
            }
            $redditContent .= "\n";
        }
        
        return $redditContent . "\n";
        
    } catch (Exception $e) {
        return "Reddit search failed ({$e->getMessage()}) – showing deterministic examples anyway.\n\n";
    }
}

// --- Command-line argument handling ---
$options = getopt('', ['lang:', 'format:', 'file', 'output:', 'n:', 'ask:', 'api:', 'auth:', 'list-languages', 'reddit', 'subreddit:', 'search:', 'limit:']);
$lang = $options['lang'] ?? 'java';
$format = $options['format'] ?? 'markdown';
$saveToFile = isset($options['file']);
$outputFormat = $options['output'] ?? 'both';
$n = (int)($options['n'] ?? 3);
$prompt = $options['ask'] ?? null;
$apiUrl = $options['api'] ?? 'https://api.openai.com/v1/chat/completions';
$auth = $options['auth'] ?? null;
$listLanguages = isset($options['list-languages']);
$useReddit = isset($options['reddit']);
$subreddit = $options['subreddit'] ?? 'programming';
$searchQuery = $options['search'] ?? null;
$redditLimit = (int)($options['limit'] ?? 5);

// List languages if requested
if ($listLanguages) {
    list_languages($snippets);
    exit(0);
}

// Validate language
if (!isset($snippets[$lang])) {
    fwrite(STDERR, "Error: Unsupported language '{$lang}'.\n");
    fwrite(STDERR, "Use --list-languages to see all supported languages.\n");
    exit(1);
}

// Validate format
$formatters = ['markdown' => 'to_markdown', 'json' => 'to_json', 'csv' => 'to_csv'];
if (!isset($formatters[$format])) {
    fwrite(STDERR, "Error: Unsupported format '{$format}'. Available: " . implode(', ', array_keys($formatters)) . "\n");
    exit(1);
}

// --- Main execution ---
// Ask AI first if prompt provided
if ($prompt !== null) {
    echo ask_ai($prompt, $apiUrl, $auth);
}

$examples = pick_examples($lang, $n, $tasks);

if ($saveToFile) {
    // Save to files
    $success = true;
    
    if ($outputFormat === 'markdown' || $outputFormat === 'both') {
        $markdownContent = to_markdown($lang, $examples, $snippets);
        if (file_put_contents($lang . '_examples.md', $markdownContent) !== false) {
            echo "Saved to: {$lang}_examples.md\n";
        } else {
            echo "Error saving markdown file\n";
            $success = false;
        }
    }
    
    if ($outputFormat === 'json' || $outputFormat === 'both') {
        $jsonContent = to_json($lang, $examples, $snippets);
        if (file_put_contents($lang . '_examples.json', $jsonContent) !== false) {
            echo "Saved to: {$lang}_examples.json\n";
        } else {
            echo "Error saving JSON file\n";
            $success = false;
        }
    }
    
    if ($outputFormat === 'csv' || $outputFormat === 'both') {
        $csvContent = to_csv($lang, $examples, $snippets);
        if (file_put_contents($lang . '_examples.csv', $csvContent) !== false) {
            echo "Saved to: {$lang}_examples.csv\n";
        } else {
            echo "Error saving CSV file\n";
            $success = false;
        }
    }
    
    if (!$success) {
        exit(1);
    }
} else {
    // Print to console
    $formatter = $formatters[$format];
    echo $formatter($lang, $examples, $snippets);
}

?>
