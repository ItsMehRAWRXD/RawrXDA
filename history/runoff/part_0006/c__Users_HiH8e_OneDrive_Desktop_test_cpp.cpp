#include <iostream>
#include <vector>
#include <string>

int main()
{
  std::cout << "Hello, World!" << std::endl;
  std::cout << "Enter your name: ";
  std::string name;
  std::cin >> name;
  std::cout << "Hello, " << name << "!" << std::endl;

  // Test some C++ features
  std::vector<int> numbers;
  numbers.push_back(1);
  numbers.push_back(2);
  numbers.push_back(3);

  std::cout << "Numbers in vector:" << std::endl;
  for (int num : numbers)
  {
    std::cout << num << std::endl;
  }

  std::cout << "Math operations:" << std::endl;
  std::cout << "2 + 3 = " << (2 + 3) << std::endl;
  std::cout << "Size of vector: " << numbers.size() << std::endl;

  return 0;
}