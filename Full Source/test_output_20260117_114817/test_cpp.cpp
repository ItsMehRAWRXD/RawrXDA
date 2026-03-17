#include <iostream>
#include <vector>
#include <string>
#include <numeric>
#include <algorithm>

template<typename T>
T sum_vector(const std::vector<T>& v) {
    return std::accumulate(v.begin(), v.end(), T{});
}

int factorial(int n) {
    return n <= 1 ? 1 : n * factorial(n - 1);
}

int main() {
    std::cout << "RawrXD Universal Compiler - C++ Test\n";
    std::cout << "=====================================\n\n";
    
    std::vector<int> nums = {1,2,3,4,5,6,7,8,9,10};
    std::cout << "Sum of 1-10: " << sum_vector(nums) << "\n";
    std::cout << "Factorial(10): " << factorial(10) << "\n";
    
    auto sq = [](int x) { return x*x; };
    std::cout << "Lambda 5^2: " << sq(5) << "\n";
    
    std::cout << "\nAll C++ tests passed!\n";
    return 0;
}
