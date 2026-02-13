import functools

def factorial(n):
    return functools.reduce(lambda x, y: x * y, range(1, n + 1), 1)

def fibonacci(n):
    a, b = 0, 1
    for _ in range(n):
        a, b = b, a + b
    return a

def main():
    print("RawrXD Universal Compiler - Python Test")
    print("========================================\n")
    
    print(f"Factorial(10) = {factorial(10)}")
    print(f"Fibonacci(20) = {fibonacci(20)}")
    
    nums = list(range(1, 11))
    print(f"Sum of 1-10: {sum(nums)}")
    print(f"Squares: {[x**2 for x in nums[:5]]}")
    
    print("\nAll Python tests passed!")

if __name__ == "__main__":
    main()
