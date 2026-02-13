#pragma once
// sign_binary.hpp – Qt-free binary signing (C++20 / Win32)
#include <string>

bool signBinary(const std::string& exePath);
