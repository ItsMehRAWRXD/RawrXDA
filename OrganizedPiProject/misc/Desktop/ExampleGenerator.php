#!/usr/bin/env php
<?php
/**
 * PHP Deterministic Example Generator
 * 
 * This self-contained PHP script provides a deterministic example generator 
 * for various languages and formats, mirroring the functionality of the 
 * Java and Python versions. It uses a cryptographic hash of the language 
 * name to consistently select examples, ensuring the same output every 
 * time for a given language and format.
 * 
 * Usage:
 *   php ExampleGenerator.php --lang java --format markdown
 *   php ExampleGenerator.php --lang java --format json
 *   php ExampleGenerator.php --lang java --format csv
 *   php ExampleGenerator.php --file --output markdown
 *   php ExampleGenerator.php --ask "Explain recursion" --api https://api.openai.com/v1/chat/completions --auth "Bearer sk-..."
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

// Code snippets organized by language and task name
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
        'fibonacci' => 'def fib(n): return n if n<2 else fib(n-1)+fib(n-2)',
        'reverse_string' => 's=input(); print(s[::-1])',
        'binary_search' => 'def bs(a,x,l=0,r=None): ...',
        'merge_sort' => 'def ms(a): if len(a)<2: return a ...'
    ],
    'go' => [
        'hello_world' => 'package main' . "\n" . 'import "fmt"' . "\n" . 'func main() { fmt.Println("Hello, world!") }',
        'fibonacci' => 'func fib(n int) int { if n<2 { return n }; return fib(n-1)+fib(n-2) }',
        'reverse_string' => 'func rev(s string) string { r:=[]rune(s); for i,j:=0,len(r)-1; i<j; i,j=i+1,j-1 { r[i],r[j]=r[j],r[i] }; return string(r) }',
        'binary_search' => 'func bs(a []int, x int) int { ... }',
        'merge_sort' => 'func ms(a []int) []int { if len(a)<2 { return a }; ... }'
    ],
    'rust' => [
        'hello_world' => 'fn main() { println!("Hello, world!"); }',
        'fibonacci' => 'fn fib(n: u64) -> u64 { if n<2 { n } else { fib(n-1)+fib(n-2) } }',
        'reverse_string' => 'fn rev(s: &str) -> String { s.chars().rev().collect() }',
        'binary_search' => 'fn bs(arr: &[i32], x: i32) -> Option<usize> { ... }',
        'merge_sort' => 'fn ms(mut arr: Vec<i32>) -> Vec<i32> { ... }'
    ]
];

// --- Helper Functions ---

/**
 * Deterministically picks a number of examples based on the language.
 * Uses SHA-256 hashing to ensure consistent selection.
 *
 * @param string $lang Language name
 * @param int $n Number of examples to select
 * @param array $tasks Available tasks
 * @return array Selected tasks
 */
function pick_examples(string $lang, int $n, array $tasks): array {
    // Use SHA-256 hash of the language name as seed
    $hash = hash('sha256', $lang);
    $h = hexdec(substr($hash, 0, 16)); // Take first 16 hex chars for 64-bit integer
    
    $seen = [];
    $result = [];
    
    // Calculate indices using the same algorithm as Java/Python versions
    $indices = [
        abs($h % count($tasks)),
        abs(intdiv($h, 7) % count($tasks)),
        abs(intdiv($h, 13) % count($tasks))
    ];
    
    // Add unique indices
    foreach ($indices as $idx) {
        if (!isset($seen[$idx])) {
            $seen[$idx] = true;
            $result[] = $tasks[$idx];
        }
        if (count($result) === $n) {
            break;
        }
    }
    
    // Fill remaining slots if needed (linear probe)
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
 *
 * @param string $lang Language name
 * @param array $tasks Selected tasks
 * @param array $snippets Code snippets
 * @return string Markdown formatted output
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
 *
 * @param string $lang Language name
 * @param array $tasks Selected tasks
 * @param array $snippets Code snippets
 * @return string JSON formatted output
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
 *
 * @param string $lang Language name
 * @param array $tasks Selected tasks
 * @param array $snippets Code snippets
 * @return string CSV formatted output
 */
function to_csv(string $lang, array $tasks, array $snippets): string {
    $output = "id,name,difficulty,snippet\n";
    
    foreach ($tasks as $task) {
        $snippet = $snippets[$lang][$task->name] ?? 'Snippet not found.';
        // Escape CSV fields
        $snippet = '"' . str_replace('"', '""', $snippet) . '"';
        $output .= "{$task->id},{$task->name},{$task->difficulty},{$snippet}\n";
    }
    
    return $output;
}

/**
 * Saves content to a file.
 *
 * @param string $filename File name
 * @param string $content Content to write
 * @return bool Success status
 */
function save_to_file(string $filename, string $content): bool {
    try {
        return file_put_contents($filename, $content) !== false;
    } catch (Exception $e) {
        return false;
    }
}

/**
 * Ask AI via HTTP POST request.
 *
 * @param string $prompt User prompt
 * @param string $url API endpoint URL
 * @param string|null $auth Authorization header
 * @return string AI response or error message
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

// --- Command-line argument handling ---
$options = getopt('', ['lang:', 'format:', 'file', 'output:', 'n:', 'ask:', 'api:', 'auth:']);
$lang = $options['lang'] ?? 'java';
$format = $options['format'] ?? 'markdown';
$saveToFile = isset($options['file']);
$outputFormat = $options['output'] ?? 'both';
$n = (int)($options['n'] ?? 3);
$prompt = $options['ask'] ?? null;
$apiUrl = $options['api'] ?? 'https://api.openai.com/v1/chat/completions';
$auth = $options['auth'] ?? null;

// Validate language
if (!isset($snippets[$lang])) {
    fwrite(STDERR, "Error: Unsupported language '{$lang}'. Available: " . implode(', ', array_keys($snippets)) . "\n");
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
        if (save_to_file($lang . '_examples.md', $markdownContent)) {
            echo "Saved to: {$lang}_examples.md\n";
        } else {
            echo "Error saving markdown file\n";
            $success = false;
        }
    }
    
    if ($outputFormat === 'json' || $outputFormat === 'both') {
        $jsonContent = to_json($lang, $examples, $snippets);
        if (save_to_file($lang . '_examples.json', $jsonContent)) {
            echo "Saved to: {$lang}_examples.json\n";
        } else {
            echo "Error saving JSON file\n";
            $success = false;
        }
    }
    
    if ($outputFormat === 'csv' || $outputFormat === 'both') {
        $csvContent = to_csv($lang, $examples, $snippets);
        if (save_to_file($lang . '_examples.csv', $csvContent)) {
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
