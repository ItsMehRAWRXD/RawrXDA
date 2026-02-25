#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "Hello from RawrXD CLI!" << std::endl;
    if (argc > 1) {
        std::cout << "Received argument: " << argv[1] << std::endl;
    }
    return 0;
}
