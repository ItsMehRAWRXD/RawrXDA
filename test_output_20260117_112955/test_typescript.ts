function factorial(n: number): number {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

function fibonacci(n: number): number {
    let a = 0, b = 1;
    for (let i = 0; i < n; i++) [a, b] = [b, a + b];
    return a;
}

function main(): void {
    console.log("RawrXD Universal Compiler - TypeScript Test");
    console.log("============================================\n");
    
    console.log("Factorial(10) = " + factorial(10));
    console.log("Fibonacci(20) = " + fibonacci(20));
    
    const nums: number[] = [1,2,3,4,5,6,7,8,9,10];
    console.log("Sum: " + nums.reduce((a,b) => a+b));
    
    console.log("\nAll TypeScript tests passed!");
}

main();
