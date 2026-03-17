fn factorial(n: u64) -> u64 {
    (1..=n).product()
}

fn fibonacci(n: u32) -> u64 {
    let (mut a, mut b) = (0u64, 1u64);
    for _ in 0..n { let t = a + b; a = b; b = t; }
    a
}

fn main() {
    println!("RawrXD Universal Compiler - Rust Test");
    println!("======================================\n");
    
    println!("Factorial(10) = {}", factorial(10));
    println!("Fibonacci(20) = {}", fibonacci(20));
    
    let nums: Vec<i32> = (1..=10).collect();
    let sum: i32 = nums.iter().sum();
    println!("Sum of 1-10: {}", sum);
    
    println!("\nAll Rust tests passed!");
}
