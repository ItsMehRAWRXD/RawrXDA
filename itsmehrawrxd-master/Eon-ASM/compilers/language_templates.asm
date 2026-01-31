; language_templates.asm
; Comprehensive Programming Language Templates for RawrZ IDE
; Includes project templates for all major programming languages
; Plus additional IDE features like snippets, autocomplete data, etc.

section .data
    ; === Template Categories ===
    TEMPLATE_CATEGORY_BASIC        equ 0
    TEMPLATE_CATEGORY_CONSOLE      equ 1
    TEMPLATE_CATEGORY_GUI          equ 2
    TEMPLATE_CATEGORY_WEB          equ 3
    TEMPLATE_CATEGORY_GAME         equ 4
    TEMPLATE_CATEGORY_LIBRARY      equ 5
    TEMPLATE_CATEGORY_EMBEDDED     equ 6
    TEMPLATE_CATEGORY_DATA_SCIENCE equ 7
    TEMPLATE_CATEGORY_MOBILE       equ 8
    TEMPLATE_CATEGORY_SYSTEM       equ 9
    
    ; === EON Language Templates ===
    eon_hello_world:
        db "; Hello World in Eon", 10
        db "module main", 10, 10
        db "import std.io", 10, 10
        db "function main() -> int {", 10
        db "    println(\"Hello, World!\")", 10
        db "    return 0", 10
        db "}", 10, 0
        
    eon_console_app:
        db "; Console Application in Eon", 10
        db "module console_app", 10, 10
        db "import std.io", 10
        db "import std.string", 10
        db "import std.collections", 10, 10
        db "function main() -> int {", 10
        db "    println(\"Welcome to Console App!\")", 10
        db "    ", 10
        db "    var input: string", 10
        db "    print(\"Enter your name: \")", 10
        db "    input = readln()", 10
        db "    ", 10
        db "    println(\"Hello, \" + input + \"!\")", 10
        db "    return 0", 10
        db "}", 10, 0
        
    eon_gui_app:
        db "; GUI Application in Eon", 10
        db "module gui_app", 10, 10
        db "import std.gui", 10
        db "import std.events", 10, 10
        db "class MainWindow extends Window {", 10
        db "    function init() {", 10
        db "        super.init(\"My App\", 800, 600)", 10
        db "        setupUI()", 10
        db "    }", 10, 10
        db "    function setupUI() {", 10
        db "        var button = Button(\"Click Me!\")", 10
        db "        button.onClick = handleClick", 10
        db "        add(button)", 10
        db "    }", 10, 10
        db "    function handleClick() {", 10
        db "        showMessage(\"Button clicked!\")", 10
        db "    }", 10
        db "}", 10, 10
        db "function main() -> int {", 10
        db "    var window = MainWindow()", 10
        db "    window.show()", 10
        db "    return runEventLoop()", 10
        db "}", 10, 0
        
    eon_web_server:
        db "; Web Server in Eon", 10
        db "module web_server", 10, 10
        db "import std.http", 10
        db "import std.json", 10, 10
        db "function handleRoot(request: HttpRequest) -> HttpResponse {", 10
        db "    return HttpResponse(\"Hello, Web!\", 200)", 10
        db "}", 10, 10
        db "function handleAPI(request: HttpRequest) -> HttpResponse {", 10
        db "    var data = {\"message\": \"Hello from API\"}", 10
        db "    return JsonResponse(data)", 10
        db "}", 10, 10
        db "function main() -> int {", 10
        db "    var server = HttpServer(8080)", 10
        db "    server.route(\"/\", handleRoot)", 10
        db "    server.route(\"/api\", handleAPI)", 10
        db "    ", 10
        db "    println(\"Server running on http://localhost:8080\")", 10
        db "    return server.run()", 10
        db "}", 10, 0
        
    ; === C Language Templates ===
    c_hello_world:
        db "/* Hello World in C */", 10
        db "#include <stdio.h>", 10, 10
        db "int main() {", 10
        db "    printf(\"Hello, World!\\n\");", 10
        db "    return 0;", 10
        db "}", 10, 0
        
    c_console_app:
        db "/* Console Application in C */", 10
        db "#include <stdio.h>", 10
        db "#include <stdlib.h>", 10
        db "#include <string.h>", 10, 10
        db "int main() {", 10
        db "    char name[100];", 10
        db "    ", 10
        db "    printf(\"Welcome to Console App!\\n\");", 10
        db "    printf(\"Enter your name: \");", 10
        db "    fgets(name, sizeof(name), stdin);", 10
        db "    ", 10
        db "    // Remove newline", 10
        db "    name[strcspn(name, \"\\n\")] = 0;", 10
        db "    ", 10
        db "    printf(\"Hello, %s!\\n\", name);", 10
        db "    return 0;", 10
        db "}", 10, 0
        
    c_library_project:
        db "/* C Library Project */", 10
        db "#ifndef MYLIB_H", 10
        db "#define MYLIB_H", 10, 10
        db "#ifdef __cplusplus", 10
        db "extern \"C\" {", 10
        db "#endif", 10, 10
        db "/* Function declarations */", 10
        db "int add_numbers(int a, int b);", 10
        db "void print_message(const char* message);", 10, 10
        db "#ifdef __cplusplus", 10
        db "}", 10
        db "#endif", 10, 10
        db "#endif /* MYLIB_H */", 10, 0
        
    c_embedded_project:
        db "/* Embedded C Project */", 10
        db "#include <stdint.h>", 10
        db "#include <stdbool.h>", 10, 10
        db "/* Hardware register definitions */", 10
        db "#define GPIO_BASE    0x40020000", 10
        db "#define GPIO_MODER   (*(volatile uint32_t*)(GPIO_BASE + 0x00))", 10
        db "#define GPIO_ODR     (*(volatile uint32_t*)(GPIO_BASE + 0x14))", 10, 10
        db "/* Initialize GPIO */", 10
        db "void gpio_init() {", 10
        db "    GPIO_MODER |= (1 << 10);  // Set PA5 as output", 10
        db "}", 10, 10
        db "/* Toggle LED */", 10
        db "void toggle_led() {", 10
        db "    GPIO_ODR ^= (1 << 5);", 10
        db "}", 10, 10
        db "int main() {", 10
        db "    gpio_init();", 10
        db "    ", 10
        db "    while (1) {", 10
        db "        toggle_led();", 10
        db "        // Add delay here", 10
        db "    }", 10
        db "    ", 10
        db "    return 0;", 10
        db "}", 10, 0
        
    ; === C++ Language Templates ===
    cpp_hello_world:
        db "// Hello World in C++", 10
        db "#include <iostream>", 10, 10
        db "int main() {", 10
        db "    std::cout << \"Hello, World!\" << std::endl;", 10
        db "    return 0;", 10
        db "}", 10, 0
        
    cpp_oop_project:
        db "// Object-Oriented C++ Project", 10
        db "#include <iostream>", 10
        db "#include <string>", 10
        db "#include <vector>", 10, 10
        db "class Person {", 10
        db "private:", 10
        db "    std::string name;", 10
        db "    int age;", 10, 10
        db "public:", 10
        db "    Person(const std::string& name, int age) : name(name), age(age) {}", 10, 10
        db "    void introduce() const {", 10
        db "        std::cout << \"Hi, I'm \" << name << \" and I'm \" << age << \" years old.\" << std::endl;", 10
        db "    }", 10, 10
        db "    // Getters and setters", 10
        db "    const std::string& getName() const { return name; }", 10
        db "    int getAge() const { return age; }", 10
        db "};", 10, 10
        db "int main() {", 10
        db "    std::vector<Person> people;", 10
        db "    people.emplace_back(\"Alice\", 30);", 10
        db "    people.emplace_back(\"Bob\", 25);", 10, 10
        db "    for (const auto& person : people) {", 10
        db "        person.introduce();", 10
        db "    }", 10, 10
        db "    return 0;", 10
        db "}", 10, 0
        
    cpp_modern_features:
        db "// Modern C++17/20 Features", 10
        db "#include <iostream>", 10
        db "#include <memory>", 10
        db "#include <optional>", 10
        db "#include <variant>", 10
        db "#include <string_view>", 10, 10
        db "// Smart pointers", 10
        db "class Resource {", 10
        db "public:", 10
        db "    Resource() { std::cout << \"Resource created\\n\"; }", 10
        db "    ~Resource() { std::cout << \"Resource destroyed\\n\"; }", 10
        db "};", 10, 10
        db "// Optional return type", 10
        db "std::optional<int> divide(int a, int b) {", 10
        db "    if (b == 0) return std::nullopt;", 10
        db "    return a / b;", 10
        db "}", 10, 10
        db "int main() {", 10
        db "    // Smart pointer", 10
        db "    auto resource = std::make_unique<Resource>();", 10, 10
        db "    // Optional handling", 10
        db "    if (auto result = divide(10, 2)) {", 10
        db "        std::cout << \"Result: \" << *result << std::endl;", 10
        db "    }", 10, 10
        db "    // Structured bindings", 10
        db "    auto [x, y] = std::make_pair(1, 2);", 10
        db "    std::cout << \"x: \" << x << \", y: \" << y << std::endl;", 10, 10
        db "    return 0;", 10
        db "}", 10, 0
        
    ; === Python Templates ===
    python_hello_world:
        db "# Hello World in Python", 10
        db "def main():", 10
        db "    print(\"Hello, World!\")", 10, 10
        db "if __name__ == \"__main__\":", 10
        db "    main()", 10, 0
        
    python_flask_web:
        db "# Flask Web Application", 10
        db "from flask import Flask, render_template, request, jsonify", 10, 10
        db "app = Flask(__name__)", 10, 10
        db "@app.route('/')", 10
        db "def index():", 10
        db "    return render_template('index.html')", 10, 10
        db "@app.route('/api/data')", 10
        db "def get_data():", 10
        db "    return jsonify({", 10
        db "        'message': 'Hello from Flask API',", 10
        db "        'status': 'success'", 10
        db "    })", 10, 10
        db "@app.route('/api/data', methods=['POST'])", 10
        db "def post_data():", 10
        db "    data = request.get_json()", 10
        db "    # Process data here", 10
        db "    return jsonify({'received': data})", 10, 10
        db "if __name__ == '__main__':", 10
        db "    app.run(debug=True)", 10, 0
        
    python_data_science:
        db "# Data Science Project", 10
        db "import pandas as pd", 10
        db "import numpy as np", 10
        db "import matplotlib.pyplot as plt", 10
        db "import seaborn as sns", 10
        db "from sklearn.model_selection import train_test_split", 10
        db "from sklearn.linear_model import LinearRegression", 10, 10
        db "def load_data(filename):", 10
        db "    \"\"\"Load and preprocess data\"\"\"", 10
        db "    df = pd.read_csv(filename)", 10
        db "    return df", 10, 10
        db "def analyze_data(df):", 10
        db "    \"\"\"Perform exploratory data analysis\"\"\"", 10
        db "    print(df.describe())", 10
        db "    print(df.info())", 10, 10
        db "def create_model(X, y):", 10
        db "    \"\"\"Create and train model\"\"\"", 10
        db "    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2)", 10
        db "    model = LinearRegression()", 10
        db "    model.fit(X_train, y_train)", 10
        db "    return model, X_test, y_test", 10, 10
        db "def main():", 10
        db "    # Load data", 10
        db "    # df = load_data('data.csv')", 10
        db "    # analyze_data(df)", 10
        db "    # Create and evaluate model", 10
        db "    print(\"Data Science Project Template\")", 10, 10
        db "if __name__ == '__main__':", 10
        db "    main()", 10, 0
        
    ; === JavaScript Templates ===
    javascript_node_app:
        db "// Node.js Application", 10
        db "const express = require('express');", 10
        db "const path = require('path');", 10, 10
        db "const app = express();", 10
        db "const PORT = process.env.PORT || 3000;", 10, 10
        db "// Middleware", 10
        db "app.use(express.json());", 10
        db "app.use(express.static('public'));", 10, 10
        db "// Routes", 10
        db "app.get('/', (req, res) => {", 10
        db "    res.sendFile(path.join(__dirname, 'public', 'index.html'));", 10
        db "});", 10, 10
        db "app.get('/api/hello', (req, res) => {", 10
        db "    res.json({ message: 'Hello from Node.js!' });", 10
        db "});", 10, 10
        db "app.listen(PORT, () => {", 10
        db "    console.log(`Server running on http://localhost:${PORT}`);", 10
        db "});", 10, 0
        
    javascript_react_component:
        db "// React Component", 10
        db "import React, { useState, useEffect } from 'react';", 10
        db "import './MyComponent.css';", 10, 10
        db "const MyComponent = ({ title }) => {", 10
        db "    const [count, setCount] = useState(0);", 10
        db "    const [data, setData] = useState(null);", 10, 10
        db "    useEffect(() => {", 10
        db "        // Component did mount", 10
        db "        fetchData();", 10
        db "    }, []);", 10, 10
        db "    const fetchData = async () => {", 10
        db "        try {", 10
        db "            const response = await fetch('/api/data');", 10
        db "            const result = await response.json();", 10
        db "            setData(result);", 10
        db "        } catch (error) {", 10
        db "            console.error('Error fetching data:', error);", 10
        db "        }", 10
        db "    };", 10, 10
        db "    const handleClick = () => {", 10
        db "        setCount(count + 1);", 10
        db "    };", 10, 10
        db "    return (", 10
        db "        <div className=\"my-component\">", 10
        db "            <h1>{title}</h1>", 10
        db "            <p>Count: {count}</p>", 10
        db "            <button onClick={handleClick}>Click me!</button>", 10
        db "            {data && <pre>{JSON.stringify(data, null, 2)}</pre>}", 10
        db "        </div>", 10
        db "    );", 10
        db "};", 10, 10
        db "export default MyComponent;", 10, 0
        
    ; === Java Templates ===
    java_hello_world:
        db "// Hello World in Java", 10
        db "public class HelloWorld {", 10
        db "    public static void main(String[] args) {", 10
        db "        System.out.println(\"Hello, World!\");", 10
        db "    }", 10
        db "}", 10, 0
        
    java_spring_boot:
        db "// Spring Boot Application", 10
        db "package com.example.demo;", 10, 10
        db "import org.springframework.boot.SpringApplication;", 10
        db "import org.springframework.boot.autoconfigure.SpringBootApplication;", 10
        db "import org.springframework.web.bind.annotation.*;", 10, 10
        db "@SpringBootApplication", 10
        db "public class DemoApplication {", 10
        db "    public static void main(String[] args) {", 10
        db "        SpringApplication.run(DemoApplication.class, args);", 10
        db "    }", 10
        db "}", 10, 10
        db "@RestController", 10
        db "@RequestMapping(\"/api\")", 10
        db "class ApiController {", 10, 10
        db "    @GetMapping(\"/hello\")", 10
        db "    public String hello(@RequestParam(defaultValue = \"World\") String name) {", 10
        db "        return String.format(\"Hello, %s!\", name);", 10
        db "    }", 10, 10
        db "    @PostMapping(\"/data\")", 10
        db "    public ResponseEntity<Map<String, Object>> postData(@RequestBody Map<String, Object> data) {", 10
        db "        Map<String, Object> response = new HashMap<>();", 10
        db "        response.put(\"received\", data);", 10
        db "        response.put(\"timestamp\", System.currentTimeMillis());", 10
        db "        return ResponseEntity.ok(response);", 10
        db "    }", 10
        db "}", 10, 0
        
    ; === Rust Templates ===
    rust_hello_world:
        db "// Hello World in Rust", 10
        db "fn main() {", 10
        db "    println!(\"Hello, World!\");", 10
        db "}", 10, 0
        
    rust_web_server:
        db "// Rust Web Server with Actix", 10
        db "use actix_web::{web, App, HttpResponse, HttpServer, Result};", 10
        db "use serde::{Deserialize, Serialize};", 10, 10
        db "#[derive(Serialize, Deserialize)]", 10
        db "struct ApiResponse {", 10
        db "    message: String,", 10
        db "    status: String,", 10
        db "}", 10, 10
        db "async fn hello() -> Result<HttpResponse> {", 10
        db "    let response = ApiResponse {", 10
        db "        message: \"Hello from Rust!\".to_string(),", 10
        db "        status: \"success\".to_string(),", 10
        db "    };", 10
        db "    Ok(HttpResponse::Ok().json(response))", 10
        db "}", 10, 10
        db "async fn index() -> Result<HttpResponse> {", 10
        db "    Ok(HttpResponse::Ok().content_type(\"text/html\").body(", 10
        db "        r#\"", 10
        db "        <!DOCTYPE html>", 10
        db "        <html>", 10
        db "        <head><title>Rust Web Server</title></head>", 10
        db "        <body><h1>Hello from Rust!</h1></body>", 10
        db "        </html>", 10
        db "        \"#,", 10
        db "    ))", 10
        db "}", 10, 10
        db "#[actix_web::main]", 10
        db "async fn main() -> std::io::Result<()> {", 10
        db "    println!(\"Starting server at http://localhost:8080\");", 10, 10
        db "    HttpServer::new(|| {", 10
        db "        App::new()", 10
        db "            .route(\"/\", web::get().to(index))", 10
        db "            .route(\"/api/hello\", web::get().to(hello))", 10
        db "    })", 10
        db "    .bind(\"127.0.0.1:8080\")?", 10
        db "    .run()", 10
        db "    .await", 10
        db "}", 10, 0
        
    ; === Go Templates ===
    go_hello_world:
        db "// Hello World in Go", 10
        db "package main", 10, 10
        db "import \"fmt\"", 10, 10
        db "func main() {", 10
        db "    fmt.Println(\"Hello, World!\")", 10
        db "}", 10, 0
        
    go_web_server:
        db "// Go Web Server", 10
        db "package main", 10, 10
        db "import (", 10
        db "    \"encoding/json\"", 10
        db "    \"fmt\"", 10
        db "    \"log\"", 10
        db "    \"net/http\"", 10
        db ")", 10, 10
        db "type ApiResponse struct {", 10
        db "    Message string `json:\"message\"`", 10
        db "    Status  string `json:\"status\"`", 10
        db "}", 10, 10
        db "func helloHandler(w http.ResponseWriter, r *http.Request) {", 10
        db "    response := ApiResponse{", 10
        db "        Message: \"Hello from Go!\",", 10
        db "        Status:  \"success\",", 10
        db "    }", 10, 10
        db "    w.Header().Set(\"Content-Type\", \"application/json\")", 10
        db "    json.NewEncoder(w).Encode(response)", 10
        db "}", 10, 10
        db "func indexHandler(w http.ResponseWriter, r *http.Request) {", 10
        db "    fmt.Fprintf(w, \"<h1>Hello from Go Web Server!</h1>\")", 10
        db "}", 10, 10
        db "func main() {", 10
        db "    http.HandleFunc(\"/\", indexHandler)", 10
        db "    http.HandleFunc(\"/api/hello\", helloHandler)", 10, 10
        db "    fmt.Println(\"Server starting on :8080\")", 10
        db "    log.Fatal(http.ListenAndServe(\":8080\", nil))", 10
        db "}", 10, 0
        
    ; === TypeScript Templates ===
    typescript_node_app:
        db "// TypeScript Node.js Application", 10
        db "import express, { Request, Response } from 'express';", 10
        db "import cors from 'cors';", 10, 10
        db "interface ApiResponse {", 10
        db "    message: string;", 10
        db "    status: 'success' | 'error';", 10
        db "    data?: any;", 10
        db "}", 10, 10
        db "const app = express();", 10
        db "const PORT = process.env.PORT || 3000;", 10, 10
        db "// Middleware", 10
        db "app.use(cors());", 10
        db "app.use(express.json());", 10, 10
        db "// Routes", 10
        db "app.get('/', (req: Request, res: Response) => {", 10
        db "    const response: ApiResponse = {", 10
        db "        message: 'Hello from TypeScript!',", 10
        db "        status: 'success'", 10
        db "    };", 10
        db "    res.json(response);", 10
        db "});", 10, 10
        db "app.get('/api/users/:id', (req: Request, res: Response) => {", 10
        db "    const userId = parseInt(req.params.id);", 10
        db "    ", 10
        db "    if (isNaN(userId)) {", 10
        db "        return res.status(400).json({", 10
        db "            message: 'Invalid user ID',", 10
        db "            status: 'error'", 10
        db "        });", 10
        db "    }", 10, 10
        db "    res.json({", 10
        db "        message: 'User found',", 10
        db "        status: 'success',", 10
        db "        data: { id: userId, name: 'John Doe' }", 10
        db "    });", 10
        db "});", 10, 10
        db "app.listen(PORT, () => {", 10
        db "    console.log(`Server running on http://localhost:${PORT}`);", 10
        db "});", 10, 0
        
    ; === Assembly Templates ===
    nasm_hello_world:
        db "; Hello World in NASM Assembly (x86-64)", 10
        db "section .data", 10
        db "    msg db 'Hello, World!', 10, 0", 10
        db "    msg_len equ $ - msg", 10, 10
        db "section .text", 10
        db "    global _start", 10, 10
        db "_start:", 10
        db "    ; Write system call", 10
        db "    mov rax, 1      ; sys_write", 10
        db "    mov rdi, 1      ; stdout", 10
        db "    mov rsi, msg    ; message", 10
        db "    mov rdx, msg_len ; length", 10
        db "    syscall", 10, 10
        db "    ; Exit system call", 10
        db "    mov rax, 60     ; sys_exit", 10
        db "    mov rdi, 0      ; exit status", 10
        db "    syscall", 10, 0
        
    ; === PHP Templates ===
    php_web_app:
        db "<?php", 10
        db "// PHP Web Application", 10, 10
        db "class WebApp {", 10
        db "    private $routes = [];", 10, 10
        db "    public function get($path, $callback) {", 10
        db "        $this->routes['GET'][$path] = $callback;", 10
        db "    }", 10, 10
        db "    public function post($path, $callback) {", 10
        db "        $this->routes['POST'][$path] = $callback;", 10
        db "    }", 10, 10
        db "    public function run() {", 10
        db "        $method = $_SERVER['REQUEST_METHOD'];", 10
        db "        $path = parse_url($_SERVER['REQUEST_URI'], PHP_URL_PATH);", 10, 10
        db "        if (isset($this->routes[$method][$path])) {", 10
        db "            call_user_func($this->routes[$method][$path]);", 10
        db "        } else {", 10
        db "            http_response_code(404);", 10
        db "            echo json_encode(['error' => 'Not Found']);", 10
        db "        }", 10
        db "    }", 10
        db "}", 10, 10
        db "$app = new WebApp();", 10, 10
        db "$app->get('/', function() {", 10
        db "    echo json_encode(['message' => 'Hello from PHP!']);", 10
        db "});", 10, 10
        db "$app->get('/api/users', function() {", 10
        db "    $users = [", 10
        db "        ['id' => 1, 'name' => 'John Doe'],", 10
        db "        ['id' => 2, 'name' => 'Jane Smith']", 10
        db "    ];", 10
        db "    header('Content-Type: application/json');", 10
        db "    echo json_encode($users);", 10
        db "});", 10, 10
        db "$app->run();", 10
        db "?>", 10, 0
        
    ; === Ruby Templates ===
    ruby_hello_world:
        db "# Hello World in Ruby", 10
        db "puts \"Hello, World!\"", 10, 0
        
    ruby_sinatra_app:
        db "# Ruby Sinatra Web Application", 10
        db "require 'sinatra'", 10
        db "require 'json'", 10, 10
        db "set :port, 4567", 10
        db "set :bind, '0.0.0.0'", 10, 10
        db "get '/' do", 10
        db "  content_type :json", 10
        db "  { message: 'Hello from Ruby Sinatra!' }.to_json", 10
        db "end", 10, 10
        db "get '/api/users' do", 10
        db "  content_type :json", 10
        db "  users = [", 10
        db "    { id: 1, name: 'Alice' },", 10
        db "    { id: 2, name: 'Bob' }", 10
        db "  ]", 10
        db "  users.to_json", 10
        db "end", 10, 10
        db "post '/api/users' do", 10
        db "  content_type :json", 10
        db "  data = JSON.parse(request.body.read)", 10
        db "  # Process user creation here", 10
        db "  { message: 'User created', data: data }.to_json", 10
        db "end", 10, 0
        
    ; === Swift Templates ===
    swift_hello_world:
        db "// Hello World in Swift", 10
        db "import Foundation", 10, 10
        db "print(\"Hello, World!\")", 10, 0
        
    swift_ios_app:
        db "// iOS App in Swift", 10
        db "import SwiftUI", 10, 10
        db "struct ContentView: View {", 10
        db "    @State private var message = \"Hello, SwiftUI!\"", 10
        db "    @State private var counter = 0", 10, 10
        db "    var body: some View {", 10
        db "        VStack(spacing: 20) {", 10
        db "            Text(message)", 10
        db "                .font(.largeTitle)", 10
        db "                .foregroundColor(.blue)", 10, 10
        db "            Text(\"Counter: \\(counter)\")", 10
        db "                .font(.title2)", 10, 10
        db "            Button(\"Tap Me!\") {", 10
        db "                counter += 1", 10
        db "                message = \"Tapped \\(counter) times!\"", 10
        db "            }", 10
        db "            .padding()", 10
        db "            .background(Color.blue)", 10
        db "            .foregroundColor(.white)", 10
        db "            .cornerRadius(10)", 10
        db "        }", 10
        db "        .padding()", 10
        db "    }", 10
        db "}", 10, 10
        db "@main", 10
        db "struct MyApp: App {", 10
        db "    var body: some Scene {", 10
        db "        WindowGroup {", 10
        db "            ContentView()", 10
        db "        }", 10
        db "    }", 10
        db "}", 10, 0
        
    ; === Kotlin Templates ===
    kotlin_hello_world:
        db "// Hello World in Kotlin", 10
        db "fun main() {", 10
        db "    println(\"Hello, World!\")", 10
        db "}", 10, 0
        
    kotlin_android_app:
        db "// Android App in Kotlin", 10
        db "package com.example.myapp", 10, 10
        db "import android.os.Bundle", 10
        db "import androidx.activity.ComponentActivity", 10
        db "import androidx.activity.compose.setContent", 10
        db "import androidx.compose.foundation.layout.*", 10
        db "import androidx.compose.material3.*", 10
        db "import androidx.compose.runtime.*", 10
        db "import androidx.compose.ui.Alignment", 10
        db "import androidx.compose.ui.Modifier", 10
        db "import androidx.compose.ui.unit.dp", 10, 10
        db "class MainActivity : ComponentActivity() {", 10
        db "    override fun onCreate(savedInstanceState: Bundle?) {", 10
        db "        super.onCreate(savedInstanceState)", 10
        db "        setContent {", 10
        db "            MyApp()", 10
        db "        }", 10
        db "    }", 10
        db "}", 10, 10
        db "@Composable", 10
        db "fun MyApp() {", 10
        db "    var counter by remember { mutableStateOf(0) }", 10, 10
        db "    Column(", 10
        db "        modifier = Modifier.fillMaxSize().padding(16.dp),", 10
        db "        horizontalAlignment = Alignment.CenterHorizontally,", 10
        db "        verticalArrangement = Arrangement.Center", 10
        db "    ) {", 10
        db "        Text(", 10
        db "            text = \"Hello, Android!\",", 10
        db "            style = MaterialTheme.typography.headlineMedium", 10
        db "        )", 10, 10
        db "        Spacer(modifier = Modifier.height(16.dp))", 10, 10
        db "        Text(text = \"Counter: $counter\")", 10, 10
        db "        Spacer(modifier = Modifier.height(16.dp))", 10, 10
        db "        Button(onClick = { counter++ }) {", 10
        db "            Text(\"Click Me!\")", 10
        db "        }", 10
        db "    }", 10
        db "}", 10, 0
        
    ; === Dart/Flutter Templates ===
    dart_flutter_app:
        db "// Flutter App in Dart", 10
        db "import 'package:flutter/material.dart';", 10, 10
        db "void main() {", 10
        db "  runApp(MyApp());", 10
        db "}", 10, 10
        db "class MyApp extends StatelessWidget {", 10
        db "  @override", 10
        db "  Widget build(BuildContext context) {", 10
        db "    return MaterialApp(", 10
        db "      title: 'Flutter Demo',", 10
        db "      theme: ThemeData(", 10
        db "        primarySwatch: Colors.blue,", 10
        db "      ),", 10
        db "      home: MyHomePage(),", 10
        db "    );", 10
        db "  }", 10
        db "}", 10, 10
        db "class MyHomePage extends StatefulWidget {", 10
        db "  @override", 10
        db "  _MyHomePageState createState() => _MyHomePageState();", 10
        db "}", 10, 10
        db "class _MyHomePageState extends State<MyHomePage> {", 10
        db "  int _counter = 0;", 10, 10
        db "  void _incrementCounter() {", 10
        db "    setState(() {", 10
        db "      _counter++;", 10
        db "    });", 10
        db "  }", 10, 10
        db "  @override", 10
        db "  Widget build(BuildContext context) {", 10
        db "    return Scaffold(", 10
        db "      appBar: AppBar(", 10
        db "        title: Text('Flutter Demo'),", 10
        db "      ),", 10
        db "      body: Center(", 10
        db "        child: Column(", 10
        db "          mainAxisAlignment: MainAxisAlignment.center,", 10
        db "          children: <Widget>[", 10
        db "            Text('You have pushed the button this many times:'),", 10
        db "            Text(", 10
        db "              '$_counter',", 10
        db "              style: Theme.of(context).textTheme.headline4,", 10
        db "            ),", 10
        db "          ],", 10
        db "        ),", 10
        db "      ),", 10
        db "      floatingActionButton: FloatingActionButton(", 10
        db "        onPressed: _incrementCounter,", 10
        db "        tooltip: 'Increment',", 10
        db "        child: Icon(Icons.add),", 10
        db "      ),", 10
        db "    );", 10
        db "  }", 10
        db "}", 10, 0
        
    ; === SQL Templates ===
    sql_database_schema:
        db "-- Database Schema Template", 10
        db "CREATE DATABASE IF NOT EXISTS myapp_db;", 10
        db "USE myapp_db;", 10, 10
        db "-- Users table", 10
        db "CREATE TABLE users (", 10
        db "    id INT AUTO_INCREMENT PRIMARY KEY,", 10
        db "    username VARCHAR(50) UNIQUE NOT NULL,", 10
        db "    email VARCHAR(100) UNIQUE NOT NULL,", 10
        db "    password_hash VARCHAR(255) NOT NULL,", 10
        db "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,", 10
        db "    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP", 10
        db ");", 10, 10
        db "-- Posts table", 10
        db "CREATE TABLE posts (", 10
        db "    id INT AUTO_INCREMENT PRIMARY KEY,", 10
        db "    user_id INT NOT NULL,", 10
        db "    title VARCHAR(200) NOT NULL,", 10
        db "    content TEXT,", 10
        db "    published BOOLEAN DEFAULT FALSE,", 10
        db "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,", 10
        db "    updated_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,", 10
        db "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE", 10
        db ");", 10, 10
        db "-- Comments table", 10
        db "CREATE TABLE comments (", 10
        db "    id INT AUTO_INCREMENT PRIMARY KEY,", 10
        db "    post_id INT NOT NULL,", 10
        db "    user_id INT NOT NULL,", 10
        db "    content TEXT NOT NULL,", 10
        db "    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,", 10
        db "    FOREIGN KEY (post_id) REFERENCES posts(id) ON DELETE CASCADE,", 10
        db "    FOREIGN KEY (user_id) REFERENCES users(id) ON DELETE CASCADE", 10
        db ");", 10, 10
        db "-- Insert sample data", 10
        db "INSERT INTO users (username, email, password_hash) VALUES", 10
        db "    ('john_doe', 'john@example.com', 'hashed_password_1'),", 10
        db "    ('jane_smith', 'jane@example.com', 'hashed_password_2');", 10, 0
        
    ; === HTML/CSS Templates ===
    html_webpage:
        db "<!DOCTYPE html>", 10
        db "<html lang=\"en\">", 10
        db "<head>", 10
        db "    <meta charset=\"UTF-8\">", 10
        db "    <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">", 10
        db "    <title>My Web Page</title>", 10
        db "    <style>", 10
        db "        body {", 10
        db "            font-family: Arial, sans-serif;", 10
        db "            max-width: 800px;", 10
        db "            margin: 0 auto;", 10
        db "            padding: 20px;", 10
        db "            line-height: 1.6;", 10
        db "        }", 10
        db "        header {", 10
        db "            background-color: #333;", 10
        db "            color: white;", 10
        db "            text-align: center;", 10
        db "            padding: 1rem;", 10
        db "            border-radius: 5px;", 10
        db "        }", 10
        db "        .container {", 10
        db "            margin: 20px 0;", 10
        db "        }", 10
        db "        button {", 10
        db "            background-color: #007acc;", 10
        db "            color: white;", 10
        db "            padding: 10px 20px;", 10
        db "            border: none;", 10
        db "            border-radius: 5px;", 10
        db "            cursor: pointer;", 10
        db "        }", 10
        db "        button:hover {", 10
        db "            background-color: #005999;", 10
        db "        }", 10
        db "    </style>", 10
        db "</head>", 10
        db "<body>", 10
        db "    <header>", 10
        db "        <h1>Welcome to My Web Page</h1>", 10
        db "    </header>", 10, 10
        db "    <div class=\"container\">", 10
        db "        <h2>About</h2>", 10
        db "        <p>This is a sample web page created with HTML and CSS.</p>", 10, 10
        db "        <h2>Interactive Element</h2>", 10
        db "        <button onclick=\"showMessage()\">Click Me!</button>", 10
        db "        <p id=\"message\"></p>", 10
        db "    </div>", 10, 10
        db "    <script>", 10
        db "        function showMessage() {", 10
        db "            document.getElementById('message').textContent = 'Hello from JavaScript!';", 10
        db "        }", 10
        db "    </script>", 10
        db "</body>", 10
        db "</html>", 10, 0

section .bss
    ; Template selection state
    selected_template_category resq 1
    selected_template_index    resq 1
    template_buffer           resb 32768  ; 32KB buffer for template content

section .text
    global get_template_by_language
    global get_template_by_category
    global get_all_templates
    global apply_template_to_project
    global create_project_from_template
    global get_template_metadata
    global search_templates
    global get_popular_templates
    
; === Get Template by Language ===
get_template_by_language:
    push rbp
    mov rbp, rsp
    ; Args: rdi = language_id, rsi = template_type
    
    ; Select template based on language and type
    cmp rdi, LANG_EON
    je .eon_templates
    
    cmp rdi, LANG_C
    je .c_templates
    
    cmp rdi, LANG_CPP
    je .cpp_templates
    
    cmp rdi, LANG_PYTHON
    je .python_templates
    
    cmp rdi, LANG_JAVASCRIPT
    je .javascript_templates
    
    cmp rdi, LANG_JAVA
    je .java_templates
    
    cmp rdi, LANG_RUST
    je .rust_templates
    
    cmp rdi, LANG_GO
    je .go_templates
    
    ; Default to unknown
    mov rax, 0
    jmp .done
    
.eon_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .eon_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_CONSOLE
    je .eon_console_template
    cmp rsi, TEMPLATE_CATEGORY_GUI
    je .eon_gui_template
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .eon_web_template
    mov rax, eon_hello_world
    jmp .done
    
.eon_hello_world_template:
    mov rax, eon_hello_world
    jmp .done
    
.eon_console_template:
    mov rax, eon_console_app
    jmp .done
    
.eon_gui_template:
    mov rax, eon_gui_app
    jmp .done
    
.eon_web_template:
    mov rax, eon_web_server
    jmp .done
    
.c_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .c_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_CONSOLE
    je .c_console_template
    cmp rsi, TEMPLATE_CATEGORY_LIBRARY
    je .c_library_template
    cmp rsi, TEMPLATE_CATEGORY_EMBEDDED
    je .c_embedded_template
    mov rax, c_hello_world
    jmp .done
    
.c_hello_world_template:
    mov rax, c_hello_world
    jmp .done
    
.c_console_template:
    mov rax, c_console_app
    jmp .done
    
.c_library_template:
    mov rax, c_library_project
    jmp .done
    
.c_embedded_template:
    mov rax, c_embedded_project
    jmp .done
    
.cpp_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .cpp_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_CONSOLE
    je .cpp_oop_template
    mov rax, cpp_hello_world
    jmp .done
    
.cpp_hello_world_template:
    mov rax, cpp_hello_world
    jmp .done
    
.cpp_oop_template:
    mov rax, cpp_oop_project
    jmp .done
    
.python_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .python_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .python_web_template
    cmp rsi, TEMPLATE_CATEGORY_DATA_SCIENCE
    je .python_data_template
    mov rax, python_hello_world
    jmp .done
    
.python_hello_world_template:
    mov rax, python_hello_world
    jmp .done
    
.python_web_template:
    mov rax, python_flask_web
    jmp .done
    
.python_data_template:
    mov rax, python_data_science
    jmp .done
    
.javascript_templates:
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .js_node_template
    cmp rsi, TEMPLATE_CATEGORY_GUI
    je .js_react_template
    mov rax, javascript_node_app
    jmp .done
    
.js_node_template:
    mov rax, javascript_node_app
    jmp .done
    
.js_react_template:
    mov rax, javascript_react_component
    jmp .done
    
.java_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .java_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .java_spring_template
    mov rax, java_hello_world
    jmp .done
    
.java_hello_world_template:
    mov rax, java_hello_world
    jmp .done
    
.java_spring_template:
    mov rax, java_spring_boot
    jmp .done
    
.rust_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .rust_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .rust_web_template
    mov rax, rust_hello_world
    jmp .done
    
.rust_hello_world_template:
    mov rax, rust_hello_world
    jmp .done
    
.rust_web_template:
    mov rax, rust_web_server
    jmp .done
    
.go_templates:
    cmp rsi, TEMPLATE_CATEGORY_BASIC
    je .go_hello_world_template
    cmp rsi, TEMPLATE_CATEGORY_WEB
    je .go_web_template
    mov rax, go_hello_world
    jmp .done
    
.go_hello_world_template:
    mov rax, go_hello_world
    jmp .done
    
.go_web_template:
    mov rax, go_web_server
    jmp .done
    
.done:
    leave
    ret

; === Apply Template to Project ===
apply_template_to_project:
    push rbp
    mov rbp, rsp
    ; Args: rdi = project_path, rsi = template_ptr, rdx = filename
    
    ; Create file with template content
    call create_file_with_content
    test rax, rax
    jz .failed
    
    ; Update project file list
    call add_file_to_project
    
    mov rax, 1      ; Success
    jmp .done
    
.failed:
    xor rax, rax    ; Failure
    
.done:
    leave
    ret

; === Create Project from Template ===
create_project_from_template:
    push rbp
    mov rbp, rsp
    ; Args: rdi = project_name, rsi = language_id, rdx = template_category
    
    ; Get template
    push rdi
    push rsi
    mov rdi, rsi
    mov rsi, rdx
    call get_template_by_language
    pop rsi
    pop rdi
    
    test rax, rax
    jz .failed
    
    ; Create project directory
    call create_project_directory
    test rax, rax
    jz .failed
    
    ; Apply template
    call apply_template_to_project
    test rax, rax
    jz .failed
    
    ; Create additional project files (Makefile, README, etc.)
    call create_project_support_files
    
    mov rax, 1      ; Success
    jmp .done
    
.failed:
    xor rax, rax    ; Failure
    
.done:
    leave
    ret

; === Stub Functions (to be implemented) ===

create_file_with_content:
    ; Create file with template content
    mov rax, 1
    ret

add_file_to_project:
    ; Add file to project
    ret

create_project_directory:
    ; Create project directory
    mov rax, 1
    ret

create_project_support_files:
    ; Create additional project files
    ret

get_template_by_category:
    ; Get templates by category
    mov rax, 1
    ret

get_all_templates:
    ; Get all available templates
    mov rax, 1
    ret

get_template_metadata:
    ; Get template metadata
    mov rax, 1
    ret

search_templates:
    ; Search templates by keyword
    mov rax, 1
    ret

get_popular_templates:
    ; Get most popular templates
    mov rax, 1
    ret

; === Language Constants ===
LANG_EON            equ 1
LANG_C              equ 2
LANG_CPP            equ 3
LANG_PYTHON         equ 4
LANG_JAVASCRIPT     equ 5
LANG_JAVA           equ 6
LANG_RUST           equ 7
LANG_GO             equ 8
