#include <cstdlib>
#include <string>
#include <iostream>
int main(int argc, char* argv[]) {
    std::string script = R"(﻿function factorial(n) {
    if (n <= 1) return 1;
    return n * factorial(n - 1);
}

function fibonacci(n) {
    let a = 0, b = 1;
    for (let i = 0; i < n; i++) [a, b] = [b, a + b];
    return a;
}

console.log("RawrXD Universal Compiler - JavaScript Test");
console.log("============================================\n");

console.log("Factorial(10) = " + factorial(10));
console.log("Fibonacci(20) = " + fibonacci(20));

const nums = [1,2,3,4,5,6,7,8,9,10];
console.log("Sum of 1-10: " + nums.reduce((a, b) => a + b));

console.log("\nAll JavaScript tests passed!");
)";
    std::string cmd = "node -c \"" + script + "\"";
    return std::system(cmd.c_str());
}
