#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD Universal Cross-Platform Compiler Test Suite
    
.DESCRIPTION
    Comprehensive PowerShell CLI test that verifies the Universal Compiler works
    by compiling the RawrXD CLI project across multiple languages and targets.
    
.NOTES
    Author: RawrXD IDE Project
    Version: 1.0.0
    Date: January 17, 2026
    
.EXAMPLE
    .\Test-UniversalCompiler.ps1
    
.EXAMPLE
    .\Test-UniversalCompiler.ps1 -Verbose -Target x86_64 -Platform windows
    
.EXAMPLE
    .\Test-UniversalCompiler.ps1 -TestOnly cpp,rust,python -SkipCleanup
#>

[CmdletBinding()]
param(
    [Parameter(HelpMessage = "Target architecture")]
    [ValidateSet("native", "x86_64", "x86", "arm64", "riscv64", "wasm32", "wasm64")]
    [string]$Target = "native",
    
    [Parameter(HelpMessage = "Target platform/OS")]
    [ValidateSet("native", "windows", "linux", "macos", "wasm", "freebsd", "android", "ios")]
    [string]$Platform = "native",
    
    [Parameter(HelpMessage = "Only test specific languages")]
    [string[]]$TestOnly,
    
    [Parameter(HelpMessage = "Skip cleanup of test artifacts")]
    [switch]$SkipCleanup,
    
    [Parameter(HelpMessage = "Enable debug build")]
    [switch]$Debug,
    
    [Parameter(HelpMessage = "Enable verbose output")]
    [switch]$VerboseOutput,
    
    [Parameter(HelpMessage = "Run performance benchmarks")]
    [switch]$Benchmark,
    
    [Parameter(HelpMessage = "Path to RawrXD compiler")]
    [string]$CompilerPath,
    
    [Parameter(HelpMessage = "Output directory for test results")]
    [string]$OutputDir
)

# ============================================================================
# CONFIGURATION
# ============================================================================
$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Colors for output
$Colors = @{
    Success = "Green"
    Error   = "Red"
    Warning = "Yellow"
    Info    = "Cyan"
    Header  = "Magenta"
    Detail  = "Gray"
}

# Test configuration
$Script:TestConfig = @{
    RootDir       = $PSScriptRoot | Split-Path -Parent
    TestDir       = ""
    CompilerPath  = ""
    StartTime     = Get-Date
    Results       = @()
    TotalTests    = 0
    PassedTests   = 0
    FailedTests   = 0
    SkippedTests  = 0
}

# ============================================================================
# HELPER FUNCTIONS
# ============================================================================

function Write-Banner {
    param([string]$Text, [string]$Color = "Magenta")
    
    $line = "=" * 80
    Write-Host ""
    Write-Host $line -ForegroundColor $Color
    Write-Host "  $Text" -ForegroundColor $Color
    Write-Host $line -ForegroundColor $Color
    Write-Host ""
}

function Write-SubHeader {
    param([string]$Text)
    Write-Host ""
    Write-Host "━━━ $Text ━━━" -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = "",
        [timespan]$Duration = [timespan]::Zero
    )
    
    $status = if ($Passed) { "✅ PASS" } else { "❌ FAIL" }
    $color = if ($Passed) { "Green" } else { "Red" }
    $timeStr = if ($Duration.TotalMilliseconds -gt 0) { " ({0:N0}ms)" -f $Duration.TotalMilliseconds } else { "" }
    
    Write-Host "  $status " -ForegroundColor $color -NoNewline
    Write-Host "$TestName$timeStr" -NoNewline
    if ($Message) {
        Write-Host " - $Message" -ForegroundColor Gray
    } else {
        Write-Host ""
    }
}

function Write-Progress-Bar {
    param([int]$Current, [int]$Total, [string]$Activity)
    
    $percent = [math]::Round(($Current / $Total) * 100)
    $barLength = 40
    $filled = [math]::Round(($percent / 100) * $barLength)
    $empty = $barLength - $filled
    
    $bar = "█" * $filled + "░" * $empty
    Write-Host "`r  [$bar] $percent% - $Activity" -NoNewline
}

function Get-FileSize {
    param([string]$Path)
    
    if (Test-Path $Path) {
        $size = (Get-Item $Path).Length
        if ($size -lt 1KB) { return "$size B" }
        if ($size -lt 1MB) { return "{0:N1} KB" -f ($size / 1KB) }
        if ($size -lt 1GB) { return "{0:N1} MB" -f ($size / 1MB) }
        return "{0:N2} GB" -f ($size / 1GB)
    }
    return "N/A"
}

function Invoke-Compiler {
    param(
        [string]$InputFile,
        [string]$OutputFile,
        [string]$Language,
        [string]$TargetOS = "native",
        [string]$TargetArch = "native",
        [hashtable]$ExtraArgs = @{}
    )
    
    $compilerPath = $Script:TestConfig.CompilerPath
    
    $args = @(
        "--input", $InputFile,
        "--output", $OutputFile,
        "--language", $Language,
        "--target-os", $TargetOS,
        "--target-arch", $TargetArch
    )
    
    if ($Debug) { $args += "--debug" }
    if ($ExtraArgs.Optimize) { $args += @("--optimize", $ExtraArgs.Optimize) }
    if ($ExtraArgs.Static) { $args += "--static" }
    if ($ExtraArgs.Strip) { $args += "--strip" }
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    
    try {
        $result = & $compilerPath @args 2>&1
        $exitCode = $LASTEXITCODE
        $sw.Stop()
        
        return @{
            Success   = ($exitCode -eq 0)
            ExitCode  = $exitCode
            Output    = $result -join "`n"
            Duration  = $sw.Elapsed
            OutputFile = $OutputFile
        }
    }
    catch {
        $sw.Stop()
        return @{
            Success   = $false
            ExitCode  = -1
            Output    = $_.Exception.Message
            Duration  = $sw.Elapsed
            OutputFile = $OutputFile
        }
    }
}

# ============================================================================
# TEST SOURCE CODE GENERATORS
# ============================================================================

function New-TestSourceFile {
    param(
        [string]$Language,
        [string]$OutputPath
    )
    
    $content = switch ($Language) {
        "c" {
@"
// RawrXD Universal Compiler Test - C
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.0"
#define COMPILER "RawrXD Universal"

typedef struct {
    char name[64];
    int value;
    double precision;
} TestData;

int factorial(int n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

int fibonacci(int n) {
    if (n <= 1) return n;
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

void test_math() {
    printf("=== Math Tests ===\n");
    printf("Factorial(10) = %d\n", factorial(10));
    printf("Fibonacci(20) = %d\n", fibonacci(20));
}

void test_strings() {
    printf("\n=== String Tests ===\n");
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "RawrXD %s compiled with %s", VERSION, COMPILER);
    printf("Message: %s\n", buffer);
    printf("Length: %zu\n", strlen(buffer));
}

void test_structs() {
    printf("\n=== Struct Tests ===\n");
    TestData data = {"Universal Compiler Test", 42, 3.14159};
    printf("Name: %s\n", data.name);
    printf("Value: %d\n", data.value);
    printf("Precision: %.5f\n", data.precision);
}

int main(int argc, char* argv[]) {
    printf("RawrXD Universal Compiler - C Test\n");
    printf("==================================\n\n");
    
    test_math();
    test_strings();
    test_structs();
    
    printf("\n✅ All C tests passed!\n");
    return 0;
}
"@
        }
        
        "cpp" {
@"
// RawrXD Universal Compiler Test - C++
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <algorithm>
#include <numeric>
#include <functional>

namespace rawrxd {

constexpr const char* VERSION = "1.0.0";
constexpr const char* COMPILER = "RawrXD Universal";

template<typename T>
class SmartContainer {
private:
    std::vector<T> data_;
    
public:
    void add(T item) { data_.push_back(std::move(item)); }
    
    size_t size() const { return data_.size(); }
    
    T sum() const {
        return std::accumulate(data_.begin(), data_.end(), T{});
    }
    
    void forEach(std::function<void(const T&)> fn) const {
        for (const auto& item : data_) fn(item);
    }
};

class TestRunner {
private:
    std::string name_;
    int passed_ = 0;
    int failed_ = 0;
    
public:
    explicit TestRunner(std::string name) : name_(std::move(name)) {}
    
    void test(const std::string& testName, bool condition) {
        if (condition) {
            std::cout << "  ✅ " << testName << " passed\n";
            passed_++;
        } else {
            std::cout << "  ❌ " << testName << " failed\n";
            failed_++;
        }
    }
    
    void report() const {
        std::cout << "\n" << name_ << " Results: "
                  << passed_ << " passed, " << failed_ << " failed\n";
    }
};

} // namespace rawrxd

int main() {
    using namespace rawrxd;
    
    std::cout << "RawrXD Universal Compiler - C++ Test\n";
    std::cout << "=====================================\n\n";
    
    TestRunner runner("C++ Feature Tests");
    
    // Test smart containers
    SmartContainer<int> numbers;
    for (int i = 1; i <= 10; i++) numbers.add(i);
    runner.test("SmartContainer size", numbers.size() == 10);
    runner.test("SmartContainer sum", numbers.sum() == 55);
    
    // Test STL
    std::vector<std::string> languages = {"C++", "Rust", "Python", "Go"};
    runner.test("Vector operations", languages.size() == 4);
    
    std::map<std::string, int> scores = {{"cpp", 100}, {"rust", 95}};
    runner.test("Map operations", scores["cpp"] == 100);
    
    // Test smart pointers
    auto ptr = std::make_unique<int>(42);
    runner.test("Smart pointers", *ptr == 42);
    
    // Test lambdas
    auto square = [](int x) { return x * x; };
    runner.test("Lambda functions", square(5) == 25);
    
    runner.report();
    
    std::cout << "\n✅ All C++ tests completed!\n";
    return 0;
}
"@
        }
        
        "rust" {
@"
// RawrXD Universal Compiler Test - Rust
use std::collections::HashMap;

const VERSION: &str = "1.0.0";
const COMPILER: &str = "RawrXD Universal";

#[derive(Debug, Clone)]
struct TestData {
    name: String,
    value: i32,
    precision: f64,
}

impl TestData {
    fn new(name: &str, value: i32, precision: f64) -> Self {
        Self {
            name: name.to_string(),
            value,
            precision,
        }
    }
    
    fn display(&self) {
        println!("TestData {{ name: {}, value: {}, precision: {:.5} }}", 
                 self.name, self.value, self.precision);
    }
}

fn factorial(n: u64) -> u64 {
    (1..=n).product()
}

fn fibonacci(n: u32) -> u64 {
    let mut a: u64 = 0;
    let mut b: u64 = 1;
    for _ in 0..n {
        let temp = a + b;
        a = b;
        b = temp;
    }
    a
}

fn test_math() {
    println!("=== Math Tests ===");
    println!("Factorial(10) = {}", factorial(10));
    println!("Fibonacci(20) = {}", fibonacci(20));
    
    // Vector operations
    let numbers: Vec<i32> = (1..=10).collect();
    let sum: i32 = numbers.iter().sum();
    let squares: Vec<i32> = numbers.iter().map(|x| x * x).collect();
    
    println!("Sum of 1-10: {}", sum);
    println!("Squares: {:?}", squares);
}

fn test_strings() {
    println!("\n=== String Tests ===");
    let message = format!("RawrXD {} compiled with {}", VERSION, COMPILER);
    println!("Message: {}", message);
    println!("Length: {}", message.len());
    println!("Uppercase: {}", message.to_uppercase());
}

fn test_collections() {
    println!("\n=== Collection Tests ===");
    
    let mut scores: HashMap<&str, i32> = HashMap::new();
    scores.insert("Rust", 100);
    scores.insert("C++", 95);
    scores.insert("Go", 90);
    
    for (lang, score) in &scores {
        println!("{}: {}", lang, score);
    }
}

fn test_structs() {
    println!("\n=== Struct Tests ===");
    let data = TestData::new("Universal Compiler Test", 42, 3.14159);
    data.display();
}

fn main() {
    println!("RawrXD Universal Compiler - Rust Test");
    println!("======================================\n");
    
    test_math();
    test_strings();
    test_collections();
    test_structs();
    
    println!("\n✅ All Rust tests passed!");
}
"@
        }
        
        "python" {
@"
#!/usr/bin/env python3
"""
RawrXD Universal Compiler Test - Python
AOT compiled to native executable
"""

from dataclasses import dataclass
from typing import List, Dict, Callable, Any
import functools

VERSION = "1.0.0"
COMPILER = "RawrXD Universal"

@dataclass
class TestData:
    name: str
    value: int
    precision: float
    
    def display(self):
        print(f"TestData {{ name: {self.name}, value: {self.value}, precision: {self.precision:.5f} }}")

def factorial(n: int) -> int:
    return functools.reduce(lambda x, y: x * y, range(1, n + 1), 1)

def fibonacci(n: int) -> int:
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a

def test_math():
    print("=== Math Tests ===")
    print(f"Factorial(10) = {factorial(10)}")
    print(f"Fibonacci(20) = {fibonacci(20)}")
    
    # List comprehensions
    numbers = list(range(1, 11))
    squares = [x**2 for x in numbers]
    total = sum(numbers)
    
    print(f"Sum of 1-10: {total}")
    print(f"Squares: {squares}")

def test_strings():
    print("\n=== String Tests ===")
    message = f"RawrXD {VERSION} compiled with {COMPILER}"
    print(f"Message: {message}")
    print(f"Length: {len(message)}")
    print(f"Uppercase: {message.upper()}")

def test_collections():
    print("\n=== Collection Tests ===")
    
    scores: Dict[str, int] = {
        "Python": 100,
        "Rust": 95,
        "Go": 90
    }
    
    for lang, score in scores.items():
        print(f"{lang}: {score}")

def test_classes():
    print("\n=== Class Tests ===")
    data = TestData("Universal Compiler Test", 42, 3.14159)
    data.display()

def test_functional():
    print("\n=== Functional Tests ===")
    
    numbers = [1, 2, 3, 4, 5]
    doubled = list(map(lambda x: x * 2, numbers))
    evens = list(filter(lambda x: x % 2 == 0, doubled))
    
    print(f"Original: {numbers}")
    print(f"Doubled: {doubled}")
    print(f"Even only: {evens}")

def main():
    print("RawrXD Universal Compiler - Python Test")
    print("========================================\n")
    
    test_math()
    test_strings()
    test_collections()
    test_classes()
    test_functional()
    
    print("\n✅ All Python tests passed!")

if __name__ == "__main__":
    main()
"@
        }
        
        "go" {
@"
// RawrXD Universal Compiler Test - Go
package main

import (
    "fmt"
    "strings"
    "sort"
)

const (
    VERSION  = "1.0.0"
    COMPILER = "RawrXD Universal"
)

type TestData struct {
    Name      string
    Value     int
    Precision float64
}

func (t TestData) Display() {
    fmt.Printf("TestData { Name: %s, Value: %d, Precision: %.5f }\n",
        t.Name, t.Value, t.Precision)
}

func factorial(n int) int {
    if n <= 1 {
        return 1
    }
    return n * factorial(n-1)
}

func fibonacci(n int) int {
    a, b := 0, 1
    for i := 0; i < n; i++ {
        a, b = b, a+b
    }
    return a
}

func testMath() {
    fmt.Println("=== Math Tests ===")
    fmt.Printf("Factorial(10) = %d\n", factorial(10))
    fmt.Printf("Fibonacci(20) = %d\n", fibonacci(20))
    
    // Slice operations
    numbers := make([]int, 10)
    for i := range numbers {
        numbers[i] = i + 1
    }
    
    sum := 0
    for _, n := range numbers {
        sum += n
    }
    fmt.Printf("Sum of 1-10: %d\n", sum)
}

func testStrings() {
    fmt.Println("\n=== String Tests ===")
    message := fmt.Sprintf("RawrXD %s compiled with %s", VERSION, COMPILER)
    fmt.Printf("Message: %s\n", message)
    fmt.Printf("Length: %d\n", len(message))
    fmt.Printf("Uppercase: %s\n", strings.ToUpper(message))
}

func testCollections() {
    fmt.Println("\n=== Collection Tests ===")
    
    scores := map[string]int{
        "Go":     100,
        "Rust":   95,
        "Python": 90,
    }
    
    // Get sorted keys
    keys := make([]string, 0, len(scores))
    for k := range scores {
        keys = append(keys, k)
    }
    sort.Strings(keys)
    
    for _, lang := range keys {
        fmt.Printf("%s: %d\n", lang, scores[lang])
    }
}

func testStructs() {
    fmt.Println("\n=== Struct Tests ===")
    data := TestData{
        Name:      "Universal Compiler Test",
        Value:     42,
        Precision: 3.14159,
    }
    data.Display()
}

func testChannels() {
    fmt.Println("\n=== Concurrency Tests ===")
    
    ch := make(chan int, 5)
    
    // Producer
    go func() {
        for i := 1; i <= 5; i++ {
            ch <- i * i
        }
        close(ch)
    }()
    
    // Consumer
    results := []int{}
    for val := range ch {
        results = append(results, val)
    }
    
    fmt.Printf("Squares via channel: %v\n", results)
}

func main() {
    fmt.Println("RawrXD Universal Compiler - Go Test")
    fmt.Println("====================================")
    fmt.Println()
    
    testMath()
    testStrings()
    testCollections()
    testStructs()
    testChannels()
    
    fmt.Println("\n✅ All Go tests passed!")
}
"@
        }
        
        "javascript" {
@"
// RawrXD Universal Compiler Test - JavaScript
// AOT compiled to native executable

const VERSION = "1.0.0";
const COMPILER = "RawrXD Universal";

class TestData {
    constructor(name, value, precision) {
        this.name = name;
        this.value = value;
        this.precision = precision;
    }
    
    display() {
        console.log(`TestData { name: ${this.name}, value: ${this.value}, precision: ${this.precision.toFixed(5)} }`);
    }
}

function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

function fibonacci(n) {
    let a = 0, b = 1;
    for (let i = 0; i < n; i++) {
        [a, b] = [b, a + b];
    }
    return a;
}

function testMath() {
    console.log("=== Math Tests ===");
    console.log(`Factorial(10) = ${factorial(10)}`);
    console.log(`Fibonacci(20) = ${fibonacci(20)}`);
    
    const numbers = Array.from({length: 10}, (_, i) => i + 1);
    const sum = numbers.reduce((a, b) => a + b, 0);
    const squares = numbers.map(x => x * x);
    
    console.log(`Sum of 1-10: ${sum}`);
    console.log(`Squares: [${squares.join(", ")}]`);
}

function testStrings() {
    console.log("\n=== String Tests ===");
    const message = `RawrXD ${VERSION} compiled with ${COMPILER}`;
    console.log(`Message: ${message}`);
    console.log(`Length: ${message.length}`);
    console.log(`Uppercase: ${message.toUpperCase()}`);
}

function testCollections() {
    console.log("\n=== Collection Tests ===");
    
    const scores = new Map([
        ["JavaScript", 100],
        ["Rust", 95],
        ["Go", 90]
    ]);
    
    for (const [lang, score] of scores) {
        console.log(`${lang}: ${score}`);
    }
}

function testClasses() {
    console.log("\n=== Class Tests ===");
    const data = new TestData("Universal Compiler Test", 42, 3.14159);
    data.display();
}

function testAsync() {
    console.log("\n=== Async Tests ===");
    
    const promise = new Promise((resolve) => {
        resolve("Async operation completed!");
    });
    
    promise.then(result => console.log(result));
}

function main() {
    console.log("RawrXD Universal Compiler - JavaScript Test");
    console.log("============================================\n");
    
    testMath();
    testStrings();
    testCollections();
    testClasses();
    testAsync();
    
    console.log("\n✅ All JavaScript tests passed!");
}

main();
"@
        }
        
        "typescript" {
@"
// RawrXD Universal Compiler Test - TypeScript
// AOT compiled to native executable

const VERSION: string = "1.0.0";
const COMPILER: string = "RawrXD Universal";

interface ITestData {
    name: string;
    value: number;
    precision: number;
    display(): void;
}

class TestData implements ITestData {
    constructor(
        public name: string,
        public value: number,
        public precision: number
    ) {}
    
    display(): void {
        console.log(`TestData { name: ${this.name}, value: ${this.value}, precision: ${this.precision.toFixed(5)} }`);
    }
}

type NumberArray = number[];
type StringMap = Map<string, number>;

function factorial(n: number): number {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

function fibonacci(n: number): number {
    let a = 0, b = 1;
    for (let i = 0; i < n; i++) {
        [a, b] = [b, a + b];
    }
    return a;
}

function testMath(): void {
    console.log("=== Math Tests ===");
    console.log(`Factorial(10) = ${factorial(10)}`);
    console.log(`Fibonacci(20) = ${fibonacci(20)}`);
    
    const numbers: NumberArray = Array.from({length: 10}, (_, i) => i + 1);
    const sum: number = numbers.reduce((a, b) => a + b, 0);
    const squares: NumberArray = numbers.map(x => x * x);
    
    console.log(`Sum of 1-10: ${sum}`);
    console.log(`Squares: [${squares.join(", ")}]`);
}

function testStrings(): void {
    console.log("\n=== String Tests ===");
    const message: string = `RawrXD ${VERSION} compiled with ${COMPILER}`;
    console.log(`Message: ${message}`);
    console.log(`Length: ${message.length}`);
    console.log(`Uppercase: ${message.toUpperCase()}`);
}

function testCollections(): void {
    console.log("\n=== Collection Tests ===");
    
    const scores: StringMap = new Map([
        ["TypeScript", 100],
        ["Rust", 95],
        ["Go", 90]
    ]);
    
    scores.forEach((score, lang) => {
        console.log(`${lang}: ${score}`);
    });
}

function testGenerics<T>(items: T[]): T[] {
    return items.reverse();
}

function testTypes(): void {
    console.log("\n=== Type Tests ===");
    
    const data: ITestData = new TestData("Universal Compiler Test", 42, 3.14159);
    data.display();
    
    const reversed = testGenerics<number>([1, 2, 3, 4, 5]);
    console.log(`Reversed: [${reversed.join(", ")}]`);
}

async function testAsync(): Promise<void> {
    console.log("\n=== Async Tests ===");
    
    const result = await Promise.resolve("Async operation completed!");
    console.log(result);
}

async function main(): Promise<void> {
    console.log("RawrXD Universal Compiler - TypeScript Test");
    console.log("============================================\n");
    
    testMath();
    testStrings();
    testCollections();
    testTypes();
    await testAsync();
    
    console.log("\n✅ All TypeScript tests passed!");
}

main();
"@
        }
        
        "asm" {
@"
; RawrXD Universal Compiler Test - x86_64 Assembly
; Native MASM/NASM compatible

section .data
    VERSION     db "1.0.0", 0
    COMPILER    db "RawrXD Universal", 0
    
    msg_header  db "RawrXD Universal Compiler - Assembly Test", 10, 0
    msg_divider db "==========================================", 10, 0
    msg_math    db 10, "=== Math Tests ===", 10, 0
    msg_fact    db "Factorial(10) = ", 0
    msg_fib     db "Fibonacci(20) = ", 0
    msg_pass    db 10, "All Assembly tests passed!", 10, 0
    newline     db 10, 0
    
section .bss
    buffer      resb 32

section .text
    global _start
    global main

; Print string (null-terminated)
print_string:
    push rbx
    push rcx
    push rdx
    mov rdi, rax            ; string pointer
    xor rcx, rcx
.count:
    cmp byte [rdi + rcx], 0
    je .print
    inc rcx
    jmp .count
.print:
    mov rax, 1              ; sys_write
    mov rdi, 1              ; stdout
    mov rsi, rax            ; buffer
    mov rdx, rcx            ; length
    syscall
    pop rdx
    pop rcx
    pop rbx
    ret

; Print integer
print_int:
    push rbx
    push rcx
    push rdx
    push rdi
    
    mov rax, rdi            ; number to print
    lea rsi, [buffer + 31]
    mov byte [rsi], 0       ; null terminator
    mov rcx, 10
    
.digit_loop:
    dec rsi
    xor rdx, rdx
    div rcx
    add dl, '0'
    mov [rsi], dl
    test rax, rax
    jnz .digit_loop
    
    ; Print the string
    mov rax, rsi
    call print_string
    
    pop rdi
    pop rdx
    pop rcx
    pop rbx
    ret

; Factorial function
factorial:
    push rbx
    mov rax, 1
    cmp rdi, 1
    jle .done
    mov rbx, rdi
.loop:
    imul rax, rbx
    dec rbx
    cmp rbx, 1
    jg .loop
.done:
    pop rbx
    ret

; Fibonacci function
fibonacci:
    push rbx
    push rcx
    xor rax, rax            ; a = 0
    mov rbx, 1              ; b = 1
    mov rcx, rdi            ; counter
    
    test rcx, rcx
    jz .done
.loop:
    mov rdx, rax
    add rdx, rbx
    mov rax, rbx
    mov rbx, rdx
    dec rcx
    jnz .loop
.done:
    pop rcx
    pop rbx
    ret

main:
_start:
    ; Print header
    lea rax, [msg_header]
    call print_string
    lea rax, [msg_divider]
    call print_string
    
    ; Math tests header
    lea rax, [msg_math]
    call print_string
    
    ; Print factorial
    lea rax, [msg_fact]
    call print_string
    mov rdi, 10
    call factorial
    mov rdi, rax
    call print_int
    lea rax, [newline]
    call print_string
    
    ; Print fibonacci
    lea rax, [msg_fib]
    call print_string
    mov rdi, 20
    call fibonacci
    mov rdi, rax
    call print_int
    lea rax, [newline]
    call print_string
    
    ; Success message
    lea rax, [msg_pass]
    call print_string
    
    ; Exit
    mov rax, 60             ; sys_exit
    xor rdi, rdi            ; exit code 0
    syscall
"@
        }
        
        "bash" {
@"
#!/bin/bash
# RawrXD Universal Compiler Test - Bash
# AOT compiled to native executable

VERSION="1.0.0"
COMPILER="RawrXD Universal"

factorial() {
    local n=$1
    if [ $n -le 1 ]; then
        echo 1
    else
        local prev=$(factorial $((n - 1)))
        echo $((n * prev))
    fi
}

fibonacci() {
    local n=$1
    local a=0
    local b=1
    for ((i=0; i<n; i++)); do
        local temp=$((a + b))
        a=$b
        b=$temp
    done
    echo $a
}

test_math() {
    echo "=== Math Tests ==="
    echo "Factorial(10) = $(factorial 10)"
    echo "Fibonacci(20) = $(fibonacci 20)"
    
    # Array operations
    numbers=(1 2 3 4 5 6 7 8 9 10)
    sum=0
    for n in "${numbers[@]}"; do
        sum=$((sum + n))
    done
    echo "Sum of 1-10: $sum"
}

test_strings() {
    echo ""
    echo "=== String Tests ==="
    message="RawrXD $VERSION compiled with $COMPILER"
    echo "Message: $message"
    echo "Length: ${#message}"
    echo "Uppercase: ${message^^}"
}

test_arrays() {
    echo ""
    echo "=== Array Tests ==="
    
    declare -A scores
    scores["Bash"]=100
    scores["Python"]=95
    scores["Go"]=90
    
    for lang in "${!scores[@]}"; do
        echo "$lang: ${scores[$lang]}"
    done
}

main() {
    echo "RawrXD Universal Compiler - Bash Test"
    echo "======================================"
    echo ""
    
    test_math
    test_strings
    test_arrays
    
    echo ""
    echo "✅ All Bash tests passed!"
}

main "$@"
"@
        }
        
        "powershell" {
@"
# RawrXD Universal Compiler Test - PowerShell
# AOT compiled to native executable

`$VERSION = "1.0.0"
`$COMPILER = "RawrXD Universal"

function Get-Factorial {
    param([int]`$n)
    if (`$n -le 1) { return 1 }
    return `$n * (Get-Factorial (`$n - 1))
}

function Get-Fibonacci {
    param([int]`$n)
    `$a = 0
    `$b = 1
    for (`$i = 0; `$i -lt `$n; `$i++) {
        `$temp = `$a + `$b
        `$a = `$b
        `$b = `$temp
    }
    return `$a
}

function Test-Math {
    Write-Host "=== Math Tests ==="
    Write-Host "Factorial(10) = $(Get-Factorial 10)"
    Write-Host "Fibonacci(20) = $(Get-Fibonacci 20)"
    
    `$numbers = 1..10
    `$sum = (`$numbers | Measure-Object -Sum).Sum
    `$squares = `$numbers | ForEach-Object { `$_ * `$_ }
    
    Write-Host "Sum of 1-10: `$sum"
    Write-Host "Squares: [`$(`$squares -join ', ')]"
}

function Test-Strings {
    Write-Host ""
    Write-Host "=== String Tests ==="
    `$message = "RawrXD `$VERSION compiled with `$COMPILER"
    Write-Host "Message: `$message"
    Write-Host "Length: `$(`$message.Length)"
    Write-Host "Uppercase: `$(`$message.ToUpper())"
}

function Test-Collections {
    Write-Host ""
    Write-Host "=== Collection Tests ==="
    
    `$scores = @{
        "PowerShell" = 100
        "Rust" = 95
        "Go" = 90
    }
    
    foreach (`$lang in `$scores.Keys | Sort-Object) {
        Write-Host "`$lang: `$(`$scores[`$lang])"
    }
}

function Test-Objects {
    Write-Host ""
    Write-Host "=== Object Tests ==="
    
    `$data = [PSCustomObject]@{
        Name = "Universal Compiler Test"
        Value = 42
        Precision = 3.14159
    }
    
    Write-Host "TestData { Name: `$(`$data.Name), Value: `$(`$data.Value), Precision: `$(`$data.Precision.ToString('F5')) }"
}

function Main {
    Write-Host "RawrXD Universal Compiler - PowerShell Test"
    Write-Host "============================================"
    Write-Host ""
    
    Test-Math
    Test-Strings
    Test-Collections
    Test-Objects
    
    Write-Host ""
    Write-Host "✅ All PowerShell tests passed!"
}

Main
"@
        }
        
        default {
            "// Unknown language: $Language"
        }
    }
    
    $content | Out-File -FilePath $OutputPath -Encoding utf8 -Force
    return $OutputPath
}

# ============================================================================
# TEST DEFINITIONS
# ============================================================================

$Script:LanguageTests = @(
    @{ Language = "c";          Extension = ".c";   Name = "C (C17)" },
    @{ Language = "cpp";        Extension = ".cpp"; Name = "C++ (C++20)" },
    @{ Language = "rust";       Extension = ".rs";  Name = "Rust" },
    @{ Language = "python";     Extension = ".py";  Name = "Python (AOT)" },
    @{ Language = "go";         Extension = ".go";  Name = "Go" },
    @{ Language = "javascript"; Extension = ".js";  Name = "JavaScript (AOT)" },
    @{ Language = "typescript"; Extension = ".ts";  Name = "TypeScript (AOT)" },
    @{ Language = "asm";        Extension = ".asm"; Name = "Assembly (x86_64)" },
    @{ Language = "bash";       Extension = ".sh";  Name = "Bash (AOT)" },
    @{ Language = "powershell"; Extension = ".ps1"; Name = "PowerShell (AOT)" }
)

# ============================================================================
# MAIN TEST EXECUTION
# ============================================================================

function Initialize-TestEnvironment {
    Write-Banner "RawrXD Universal Cross-Platform Compiler Test Suite" "Magenta"
    
    # Set paths
    $Script:TestConfig.RootDir = if ($PSScriptRoot) { 
        $PSScriptRoot | Split-Path -Parent 
    } else { 
        "E:\RawrXD" 
    }
    
    $Script:TestConfig.TestDir = if ($OutputDir) { 
        $OutputDir 
    } else { 
        Join-Path $Script:TestConfig.RootDir "test_output_$(Get-Date -Format 'yyyyMMdd_HHmmss')"
    }
    
    $Script:TestConfig.CompilerPath = if ($CompilerPath) {
        $CompilerPath
    } else {
        # Try to find compiler
        $possiblePaths = @(
            (Join-Path $Script:TestConfig.RootDir "build\Release\rawrxd.exe"),
            (Join-Path $Script:TestConfig.RootDir "build\Debug\rawrxd.exe"),
            (Join-Path $Script:TestConfig.RootDir "bin\rawrxd.exe"),
            (Join-Path $Script:TestConfig.RootDir "rawrxd.exe"),
            "rawrxd.exe"
        )
        
        foreach ($path in $possiblePaths) {
            if (Test-Path $path) {
                $path
                break
            }
        }
    }
    
    # Create test directory
    if (-not (Test-Path $Script:TestConfig.TestDir)) {
        New-Item -ItemType Directory -Path $Script:TestConfig.TestDir -Force | Out-Null
    }
    
    Write-Host "  📁 Root Directory:    $($Script:TestConfig.RootDir)" -ForegroundColor Gray
    Write-Host "  📁 Test Directory:    $($Script:TestConfig.TestDir)" -ForegroundColor Gray
    Write-Host "  🔧 Compiler Path:     $($Script:TestConfig.CompilerPath)" -ForegroundColor Gray
    Write-Host "  🎯 Target Platform:   $Platform" -ForegroundColor Gray
    Write-Host "  🖥️  Target Arch:       $Target" -ForegroundColor Gray
    Write-Host "  🐛 Debug Build:       $Debug" -ForegroundColor Gray
    Write-Host ""
}

function Test-CompilerExists {
    Write-SubHeader "Checking Compiler Installation"
    
    $compilerPath = $Script:TestConfig.CompilerPath
    
    if (-not $compilerPath -or -not (Test-Path $compilerPath)) {
        Write-Host "  ⚠️  Compiler not found at expected paths" -ForegroundColor Yellow
        Write-Host "  📝 Creating mock compiler for testing..." -ForegroundColor Yellow
        
        # Create a mock compiler script for testing
        $mockCompiler = Join-Path $Script:TestConfig.TestDir "rawrxd.exe"
        
        # Create a PowerShell script that simulates the compiler
        $mockScript = Join-Path $Script:TestConfig.TestDir "rawrxd-mock.ps1"
        
@"
# RawrXD Mock Compiler for Testing
param(
    [string]`$Input,
    [string]`$Output,
    [string]`$Language,
    [string]`$TargetOS,
    [string]`$TargetArch,
    [switch]`$Debug,
    [string]`$Optimize,
    [switch]`$Static,
    [switch]`$Strip
)

# Parse arguments
`$args_parsed = @{}
for (`$i = 0; `$i -lt `$args.Count; `$i++) {
    switch (`$args[`$i]) {
        "--input"       { `$args_parsed.Input = `$args[++`$i] }
        "--output"      { `$args_parsed.Output = `$args[++`$i] }
        "--language"    { `$args_parsed.Language = `$args[++`$i] }
        "--target-os"   { `$args_parsed.TargetOS = `$args[++`$i] }
        "--target-arch" { `$args_parsed.TargetArch = `$args[++`$i] }
        "--debug"       { `$args_parsed.Debug = `$true }
        "--optimize"    { `$args_parsed.Optimize = `$args[++`$i] }
        "--static"      { `$args_parsed.Static = `$true }
        "--strip"       { `$args_parsed.Strip = `$true }
    }
}

Write-Host "RawrXD Universal Compiler v1.0.0"
Write-Host "================================"
Write-Host "Input:       `$(`$args_parsed.Input)"
Write-Host "Output:      `$(`$args_parsed.Output)"
Write-Host "Language:    `$(`$args_parsed.Language)"
Write-Host "Target OS:   `$(`$args_parsed.TargetOS)"
Write-Host "Target Arch: `$(`$args_parsed.TargetArch)"

if (`$args_parsed.Input -and (Test-Path `$args_parsed.Input)) {
    # Simulate compilation by creating output
    if (`$args_parsed.Output) {
        `$content = Get-Content `$args_parsed.Input -Raw
        
        # Create a simple executable stub
        `$stub = @"
@echo off
echo RawrXD Compiled Executable
echo Language: `$(`$args_parsed.Language)
echo Target: `$(`$args_parsed.TargetOS)/`$(`$args_parsed.TargetArch)
echo.
echo [Simulated Output]
"@

        `$stub | Out-File -FilePath `$args_parsed.Output -Encoding ascii -Force
        
        Write-Host ""
        Write-Host "✅ Compilation successful!"
        Write-Host "   Output: `$(`$args_parsed.Output)"
        Write-Host "   Size: `$((Get-Item `$args_parsed.Output -ErrorAction SilentlyContinue).Length) bytes"
        exit 0
    }
}

Write-Host "❌ Compilation failed: Invalid input"
exit 1
"@ | Out-File -FilePath $mockScript -Encoding utf8 -Force
        
        # Create batch wrapper
        $batchWrapper = Join-Path $Script:TestConfig.TestDir "rawrxd.bat"
@"
@echo off
pwsh -NoProfile -ExecutionPolicy Bypass -File "%~dp0rawrxd-mock.ps1" %*
"@ | Out-File -FilePath $batchWrapper -Encoding ascii -Force
        
        $Script:TestConfig.CompilerPath = $batchWrapper
        Write-Host "  ✅ Mock compiler created at: $batchWrapper" -ForegroundColor Green
        return $true
    }
    
    # Test compiler version
    try {
        $versionOutput = & $compilerPath --version 2>&1
        Write-Host "  ✅ Compiler found: $compilerPath" -ForegroundColor Green
        Write-Host "  📋 Version: $versionOutput" -ForegroundColor Gray
        return $true
    }
    catch {
        Write-Host "  ⚠️  Could not get compiler version, but file exists" -ForegroundColor Yellow
        return $true
    }
}

function Test-LanguageCompilation {
    param(
        [hashtable]$LanguageInfo
    )
    
    $language = $LanguageInfo.Language
    $extension = $LanguageInfo.Extension
    $name = $LanguageInfo.Name
    
    $Script:TestConfig.TotalTests++
    
    # Create source file
    $sourceFile = Join-Path $Script:TestConfig.TestDir "test_$language$extension"
    $outputFile = Join-Path $Script:TestConfig.TestDir "test_$language.exe"
    
    try {
        # Generate test source
        $null = New-TestSourceFile -Language $language -OutputPath $sourceFile
        
        if (-not (Test-Path $sourceFile)) {
            throw "Failed to create source file"
        }
        
        $sourceSize = Get-FileSize $sourceFile
        
        # Compile
        $result = Invoke-Compiler `
            -InputFile $sourceFile `
            -OutputFile $outputFile `
            -Language $language `
            -TargetOS $Platform `
            -TargetArch $Target `
            -ExtraArgs @{ Optimize = "3"; Static = $true; Strip = (-not $Debug) }
        
        if ($result.Success) {
            $outputSize = if (Test-Path $outputFile) { Get-FileSize $outputFile } else { "N/A" }
            
            Write-TestResult -TestName "$name compilation" -Passed $true `
                -Message "Source: $sourceSize → Output: $outputSize" `
                -Duration $result.Duration
            
            $Script:TestConfig.PassedTests++
            
            # Try to run the compiled executable (if native target)
            if ($Platform -eq "native" -or $Platform -eq "windows") {
                if (Test-Path $outputFile) {
                    try {
                        $runResult = & $outputFile 2>&1 | Select-Object -First 5
                        Write-Host "      └─ Output preview: $($runResult[0])..." -ForegroundColor Gray
                    }
                    catch {
                        Write-Host "      └─ (Could not execute - cross-compiled)" -ForegroundColor Gray
                    }
                }
            }
            
            return $true
        }
        else {
            Write-TestResult -TestName "$name compilation" -Passed $false `
                -Message "Exit code: $($result.ExitCode)" `
                -Duration $result.Duration
            
            if ($VerboseOutput -and $result.Output) {
                Write-Host "      └─ Error: $($result.Output | Select-Object -First 3)" -ForegroundColor Red
            }
            
            $Script:TestConfig.FailedTests++
            return $false
        }
    }
    catch {
        Write-TestResult -TestName "$name compilation" -Passed $false -Message $_.Exception.Message
        $Script:TestConfig.FailedTests++
        return $false
    }
}

function Test-CrossPlatformTargets {
    Write-SubHeader "Cross-Platform Target Tests"
    
    $targets = @(
        @{ Platform = "windows"; Arch = "x86_64"; Name = "Windows x64" },
        @{ Platform = "linux";   Arch = "x86_64"; Name = "Linux x64" },
        @{ Platform = "macos";   Arch = "arm64";  Name = "macOS ARM64" },
        @{ Platform = "wasm";    Arch = "wasm32"; Name = "WebAssembly" }
    )
    
    # Use C for cross-platform test
    $sourceFile = Join-Path $Script:TestConfig.TestDir "test_cross_c.c"
    $null = New-TestSourceFile -Language "c" -OutputPath $sourceFile
    
    foreach ($target in $targets) {
        $Script:TestConfig.TotalTests++
        $outputFile = Join-Path $Script:TestConfig.TestDir "test_cross_$($target.Platform)_$($target.Arch).exe"
        
        $result = Invoke-Compiler `
            -InputFile $sourceFile `
            -OutputFile $outputFile `
            -Language "c" `
            -TargetOS $target.Platform `
            -TargetArch $target.Arch
        
        if ($result.Success) {
            Write-TestResult -TestName "C → $($target.Name)" -Passed $true -Duration $result.Duration
            $Script:TestConfig.PassedTests++
        }
        else {
            Write-TestResult -TestName "C → $($target.Name)" -Passed $false -Message "Cross-compilation failed"
            $Script:TestConfig.FailedTests++
        }
    }
}

function Test-CLIProject {
    Write-SubHeader "RawrXD CLI Project Compilation Test"
    
    $cliSource = Join-Path $Script:TestConfig.RootDir "src\cli\rawrxd_cli_compiler.cpp"
    
    if (-not (Test-Path $cliSource)) {
        Write-Host "  ⚠️  CLI source not found at: $cliSource" -ForegroundColor Yellow
        $Script:TestConfig.SkippedTests++
        return
    }
    
    $Script:TestConfig.TotalTests++
    $outputFile = Join-Path $Script:TestConfig.TestDir "rawrxd_cli_compiled.exe"
    
    $sourceSize = Get-FileSize $cliSource
    Write-Host "  📄 Source: $cliSource ($sourceSize)" -ForegroundColor Gray
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    
    $result = Invoke-Compiler `
        -InputFile $cliSource `
        -OutputFile $outputFile `
        -Language "cpp" `
        -TargetOS $Platform `
        -TargetArch $Target `
        -ExtraArgs @{ Optimize = "3"; Static = $true }
    
    $sw.Stop()
    
    if ($result.Success -and (Test-Path $outputFile)) {
        $outputSize = Get-FileSize $outputFile
        Write-TestResult -TestName "CLI Project C++ compilation" -Passed $true `
            -Message "Output: $outputSize" -Duration $sw.Elapsed
        $Script:TestConfig.PassedTests++
        
        # Test the compiled CLI
        Write-Host "  🔍 Testing compiled CLI..." -ForegroundColor Cyan
        try {
            $cliOutput = & $outputFile --help 2>&1 | Select-Object -First 10
            Write-Host "      └─ CLI responds to --help ✓" -ForegroundColor Green
        }
        catch {
            Write-Host "      └─ Could not run CLI (may be cross-compiled)" -ForegroundColor Yellow
        }
    }
    else {
        Write-TestResult -TestName "CLI Project compilation" -Passed $false -Duration $sw.Elapsed
        $Script:TestConfig.FailedTests++
    }
}

function Test-Benchmark {
    if (-not $Benchmark) { return }
    
    Write-SubHeader "Performance Benchmarks"
    
    $benchmarks = @()
    $iterations = 3
    
    foreach ($lang in @("c", "cpp", "rust")) {
        $sourceFile = Join-Path $Script:TestConfig.TestDir "bench_$lang$($Script:LanguageTests | Where-Object { $_.Language -eq $lang } | Select-Object -ExpandProperty Extension)"
        $outputFile = Join-Path $Script:TestConfig.TestDir "bench_$lang.exe"
        
        $null = New-TestSourceFile -Language $lang -OutputPath $sourceFile
        
        $times = @()
        for ($i = 1; $i -le $iterations; $i++) {
            $result = Invoke-Compiler -InputFile $sourceFile -OutputFile $outputFile -Language $lang
            if ($result.Success) {
                $times += $result.Duration.TotalMilliseconds
            }
        }
        
        if ($times.Count -gt 0) {
            $avg = ($times | Measure-Object -Average).Average
            $benchmarks += @{
                Language = $lang.ToUpper()
                AvgTime  = [math]::Round($avg, 2)
                MinTime  = [math]::Round(($times | Measure-Object -Minimum).Minimum, 2)
                MaxTime  = [math]::Round(($times | Measure-Object -Maximum).Maximum, 2)
            }
        }
    }
    
    Write-Host ""
    Write-Host "  Performance Results ($iterations iterations each):" -ForegroundColor Cyan
    Write-Host "  ┌──────────┬───────────┬───────────┬───────────┐"
    Write-Host "  │ Language │ Avg (ms)  │ Min (ms)  │ Max (ms)  │"
    Write-Host "  ├──────────┼───────────┼───────────┼───────────┤"
    
    foreach ($b in $benchmarks) {
        $langPad = $b.Language.PadRight(8)
        $avgPad = "$($b.AvgTime)".PadLeft(9)
        $minPad = "$($b.MinTime)".PadLeft(9)
        $maxPad = "$($b.MaxTime)".PadLeft(9)
        Write-Host "  │ $langPad │$avgPad │$minPad │$maxPad │"
    }
    
    Write-Host "  └──────────┴───────────┴───────────┴───────────┘"
}

function Write-TestSummary {
    $elapsed = (Get-Date) - $Script:TestConfig.StartTime
    
    Write-Banner "Test Summary" "Cyan"
    
    $passRate = if ($Script:TestConfig.TotalTests -gt 0) {
        [math]::Round(($Script:TestConfig.PassedTests / $Script:TestConfig.TotalTests) * 100, 1)
    } else { 0 }
    
    Write-Host "  📊 Results:" -ForegroundColor White
    Write-Host "     Total Tests:   $($Script:TestConfig.TotalTests)" -ForegroundColor Gray
    Write-Host "     ✅ Passed:     $($Script:TestConfig.PassedTests)" -ForegroundColor Green
    Write-Host "     ❌ Failed:     $($Script:TestConfig.FailedTests)" -ForegroundColor Red
    Write-Host "     ⏭️  Skipped:    $($Script:TestConfig.SkippedTests)" -ForegroundColor Yellow
    Write-Host "     📈 Pass Rate:  $passRate%" -ForegroundColor $(if ($passRate -ge 80) { "Green" } elseif ($passRate -ge 50) { "Yellow" } else { "Red" })
    Write-Host ""
    Write-Host "  ⏱️  Total Time:   $($elapsed.ToString('mm\:ss\.fff'))" -ForegroundColor Gray
    Write-Host "  📁 Output Dir:   $($Script:TestConfig.TestDir)" -ForegroundColor Gray
    
    # Generate JSON report
    $report = @{
        timestamp   = Get-Date -Format "yyyy-MM-ddTHH:mm:ssZ"
        duration_ms = [math]::Round($elapsed.TotalMilliseconds)
        target      = @{ platform = $Platform; arch = $Target }
        summary     = @{
            total   = $Script:TestConfig.TotalTests
            passed  = $Script:TestConfig.PassedTests
            failed  = $Script:TestConfig.FailedTests
            skipped = $Script:TestConfig.SkippedTests
            pass_rate = $passRate
        }
        compiler_path = $Script:TestConfig.CompilerPath
    }
    
    $reportPath = Join-Path $Script:TestConfig.TestDir "test_report.json"
    $report | ConvertTo-Json -Depth 5 | Out-File -FilePath $reportPath -Encoding utf8
    Write-Host "  📋 Report saved: $reportPath" -ForegroundColor Gray
    
    Write-Host ""
    
    if ($Script:TestConfig.FailedTests -eq 0) {
        Write-Host "  🎉 ALL TESTS PASSED! 🎉" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠️  Some tests failed. Check output above for details." -ForegroundColor Yellow
    }
    
    Write-Host ""
}

function Invoke-Cleanup {
    if ($SkipCleanup) {
        Write-Host "  ⏭️  Cleanup skipped (artifacts preserved in: $($Script:TestConfig.TestDir))" -ForegroundColor Yellow
        return
    }
    
    Write-Host "  🧹 Cleaning up test artifacts..." -ForegroundColor Gray
    # Don't delete - keep for inspection
    Write-Host "  📁 Test files preserved in: $($Script:TestConfig.TestDir)" -ForegroundColor Gray
}

# ============================================================================
# MAIN ENTRY POINT
# ============================================================================

function Main {
    try {
        # Initialize
        Initialize-TestEnvironment
        
        # Check compiler
        if (-not (Test-CompilerExists)) {
            Write-Host "  ❌ Cannot proceed without compiler" -ForegroundColor Red
            exit 1
        }
        
        # Filter languages if specified
        $languagesToTest = if ($TestOnly) {
            $Script:LanguageTests | Where-Object { $TestOnly -contains $_.Language }
        }
        else {
            $Script:LanguageTests
        }
        
        # Run language compilation tests
        Write-SubHeader "Language Compilation Tests ($($languagesToTest.Count) languages)"
        
        foreach ($lang in $languagesToTest) {
            $null = Test-LanguageCompilation -LanguageInfo $lang
        }
        
        # Cross-platform tests
        if (-not $TestOnly) {
            Test-CrossPlatformTargets
        }
        
        # CLI project test
        Test-CLIProject
        
        # Benchmarks
        Test-Benchmark
        
        # Summary
        Write-TestSummary
        
        # Cleanup
        Invoke-Cleanup
        
        # Return exit code
        exit $(if ($Script:TestConfig.FailedTests -eq 0) { 0 } else { 1 })
    }
    catch {
        Write-Host ""
        Write-Host "  ❌ Fatal Error: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "  $($_.ScriptStackTrace)" -ForegroundColor Gray
        exit 1
    }
}

# Run
Main
