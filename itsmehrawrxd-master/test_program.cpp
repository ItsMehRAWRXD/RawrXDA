#include <iostream>
#include <vector>
#include <string>

int main() {
    std::cout << "Hello from Real Working Compiler!" << std::endl;
    
    // Test some C++ features
    std::vector<int> numbers = {1, 2, 3, 4, 5};
    
    std::cout << "Numbers: ";
    for (int num : numbers) {
        std::cout << num << " ";
    }
    std::cout << std::endl;
    
    // Test string operations
    std::string message = "Compilation successful!";
    std::cout << "Message: " << message << std::endl;
    
    // Test arithmetic
    int a = 10;
    int b = 20;
    int sum = a + b;
    std::cout << "Sum: " << a << " + " << b << " = " << sum << std::endl;
    
    return 0;
}
