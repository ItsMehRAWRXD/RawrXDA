#include <type_traits>
#include <iostream>

int main() {
    using T = std::remove_cvref_t<const int&>;
    std::cout << std::is_same_v<T, int> << "\n";
    return 0;
}
