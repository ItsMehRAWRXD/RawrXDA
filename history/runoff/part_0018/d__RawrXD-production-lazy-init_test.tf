fn fibonacci(n: i64) -> i64 {
    if n <= 1 {
        return n
    }
    return fibonacci(n - 1) + fibonacci(n - 2)
}

fn factorial(n: i64) -> i64 {
    if n <= 1 {
        return 1
    }
    return n * factorial(n - 1)
}

fn deep_loop() {
    let sum = 0
    while sum < 1000 {
        let i = 0
        while i < 100 {
            let j = 0
            while j < 100 {
                let k = 0
                while k < 10 {
                    let m = 0
                    while m < 10 {
                        sum = sum + 1
                        m = m + 1
                    }
                    k = k + 1
                }
                j = j + 1
            }
            i = i + 1
        }
    }
}

struct Vector3 {
    x: f64
    y: f64
    z: f64
}

fn alloc_vectors() {
    let v = alloc(Vector3, 10000)
    let w = alloc(Vector3, 10000)
    let result = alloc(Vector3, 10000)
}

fn main() {
    let fib_result = fibonacci(40)
    let fact_result = factorial(20)
    deep_loop()
    alloc_vectors()
    return 0
}