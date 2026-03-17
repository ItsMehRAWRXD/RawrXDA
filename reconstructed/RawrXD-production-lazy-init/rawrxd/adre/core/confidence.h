#pragma once
#include <cstdint>

struct Confidence {
    uint8_t value; // 0–100

    void increase(int delta) {
        value = (value + delta > 100) ? 100 : value + delta;
    }
    void decrease(int delta) {
        value = (delta > value) ? 0 : value - delta;
    }
};
