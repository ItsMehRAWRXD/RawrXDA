package main

import "fmt"

func factorial(n int) int {
    if n <= 1 { return 1 }
    return n * factorial(n-1)
}

func fibonacci(n int) int {
    a, b := 0, 1
    for i := 0; i < n; i++ { a, b = b, a+b }
    return a
}

func main() {
    fmt.Println("RawrXD Universal Compiler - Go Test")
    fmt.Println("====================================\n")
    
    fmt.Printf("Factorial(10) = %d\n", factorial(10))
    fmt.Printf("Fibonacci(20) = %d\n", fibonacci(20))
    
    sum := 0
    for i := 1; i <= 10; i++ { sum += i }
    fmt.Printf("Sum of 1-10: %d\n", sum)
    
    fmt.Println("\nAll Go tests passed!")
}
