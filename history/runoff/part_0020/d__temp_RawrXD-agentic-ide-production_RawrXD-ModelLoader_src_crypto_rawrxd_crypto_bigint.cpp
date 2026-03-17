/**
 * Big Integer Implementation for RSA/ECC Operations
 */

#include "rawrxd_crypto.h"
#include <algorithm>
#include <stdexcept>
#include <sstream>
#include <iomanip>

namespace RawrXD {
namespace Crypto {

// ============================================================
// BIG INTEGER IMPLEMENTATION
// ============================================================

BigInt::BigInt() : negative_(false) {
    digits_.push_back(0);
}

BigInt::BigInt(uint64_t value) : negative_(false) {
    if (value == 0) {
        digits_.push_back(0);
    } else {
        digits_.push_back(value & 0xFFFFFFFF);
        if (value > 0xFFFFFFFF) {
            digits_.push_back(value >> 32);
        }
    }
}

BigInt::BigInt(const std::vector<uint8_t>& bytes) : BigInt(bytes.data(), bytes.size()) {}

BigInt::BigInt(const uint8_t* bytes, size_t len) : negative_(false) {
    if (len == 0) {
        digits_.push_back(0);
        return;
    }
    
    // Big-endian bytes to little-endian uint32_t digits
    digits_.resize((len + 3) / 4, 0);
    
    for (size_t i = 0; i < len; i++) {
        size_t digitIdx = (len - 1 - i) / 4;
        size_t byteIdx = (len - 1 - i) % 4;
        digits_[digitIdx] |= (uint32_t)bytes[i] << (byteIdx * 8);
    }
    
    normalize();
}

void BigInt::normalize() {
    while (digits_.size() > 1 && digits_.back() == 0) {
        digits_.pop_back();
    }
    if (digits_.size() == 1 && digits_[0] == 0) {
        negative_ = false;
    }
}

bool BigInt::isZero() const {
    return digits_.size() == 1 && digits_[0] == 0;
}

bool BigInt::isOne() const {
    return !negative_ && digits_.size() == 1 && digits_[0] == 1;
}

size_t BigInt::bitLength() const {
    if (isZero()) return 0;
    
    size_t bits = (digits_.size() - 1) * 32;
    uint32_t top = digits_.back();
    
    while (top) {
        bits++;
        top >>= 1;
    }
    
    return bits;
}

std::vector<uint8_t> BigInt::toBytes() const {
    if (isZero()) return {0};
    
    size_t numBytes = (bitLength() + 7) / 8;
    std::vector<uint8_t> result(numBytes);
    
    for (size_t i = 0; i < numBytes; i++) {
        size_t digitIdx = i / 4;
        size_t byteIdx = i % 4;
        if (digitIdx < digits_.size()) {
            result[numBytes - 1 - i] = (digits_[digitIdx] >> (byteIdx * 8)) & 0xFF;
        }
    }
    
    return result;
}

std::string BigInt::toHex() const {
    auto bytes = toBytes();
    std::ostringstream oss;
    for (uint8_t b : bytes) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)b;
    }
    return oss.str();
}

BigInt BigInt::fromHex(const std::string& hex) {
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2) {
        std::string byteStr = hex.substr(i, 2);
        bytes.push_back((uint8_t)std::stoul(byteStr, nullptr, 16));
    }
    return BigInt(bytes);
}

int BigInt::compareMagnitude(const std::vector<uint32_t>& a,
                             const std::vector<uint32_t>& b) {
    if (a.size() != b.size()) {
        return a.size() > b.size() ? 1 : -1;
    }
    
    for (size_t i = a.size(); i > 0; i--) {
        if (a[i - 1] != b[i - 1]) {
            return a[i - 1] > b[i - 1] ? 1 : -1;
        }
    }
    
    return 0;
}

bool BigInt::operator==(const BigInt& other) const {
    return negative_ == other.negative_ && digits_ == other.digits_;
}

bool BigInt::operator!=(const BigInt& other) const {
    return !(*this == other);
}

bool BigInt::operator<(const BigInt& other) const {
    if (negative_ != other.negative_) {
        return negative_;
    }
    
    int cmp = compareMagnitude(digits_, other.digits_);
    return negative_ ? (cmp > 0) : (cmp < 0);
}

bool BigInt::operator>(const BigInt& other) const {
    return other < *this;
}

bool BigInt::operator<=(const BigInt& other) const {
    return !(*this > other);
}

bool BigInt::operator>=(const BigInt& other) const {
    return !(*this < other);
}

void BigInt::addMagnitude(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b,
                          std::vector<uint32_t>& result) {
    size_t maxSize = std::max(a.size(), b.size());
    result.resize(maxSize + 1, 0);
    
    uint64_t carry = 0;
    for (size_t i = 0; i < maxSize; i++) {
        uint64_t sum = carry;
        if (i < a.size()) sum += a[i];
        if (i < b.size()) sum += b[i];
        
        result[i] = sum & 0xFFFFFFFF;
        carry = sum >> 32;
    }
    result[maxSize] = carry;
}

void BigInt::subMagnitude(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b,
                          std::vector<uint32_t>& result) {
    // Assumes a >= b
    result.resize(a.size(), 0);
    
    int64_t borrow = 0;
    for (size_t i = 0; i < a.size(); i++) {
        int64_t diff = (int64_t)a[i] - borrow;
        if (i < b.size()) diff -= b[i];
        
        if (diff < 0) {
            diff += 0x100000000LL;
            borrow = 1;
        } else {
            borrow = 0;
        }
        
        result[i] = (uint32_t)diff;
    }
}

BigInt BigInt::operator+(const BigInt& other) const {
    BigInt result;
    
    if (negative_ == other.negative_) {
        addMagnitude(digits_, other.digits_, result.digits_);
        result.negative_ = negative_;
    } else {
        int cmp = compareMagnitude(digits_, other.digits_);
        if (cmp >= 0) {
            subMagnitude(digits_, other.digits_, result.digits_);
            result.negative_ = negative_;
        } else {
            subMagnitude(other.digits_, digits_, result.digits_);
            result.negative_ = other.negative_;
        }
    }
    
    result.normalize();
    return result;
}

BigInt BigInt::operator-(const BigInt& other) const {
    BigInt negOther = other;
    negOther.negative_ = !negOther.negative_;
    return *this + negOther;
}

void BigInt::mulMagnitude(const std::vector<uint32_t>& a,
                          const std::vector<uint32_t>& b,
                          std::vector<uint32_t>& result) {
    result.assign(a.size() + b.size(), 0);
    
    for (size_t i = 0; i < a.size(); i++) {
        uint64_t carry = 0;
        for (size_t j = 0; j < b.size(); j++) {
            uint64_t prod = (uint64_t)a[i] * b[j] + result[i + j] + carry;
            result[i + j] = prod & 0xFFFFFFFF;
            carry = prod >> 32;
        }
        result[i + b.size()] = carry;
    }
}

BigInt BigInt::operator*(const BigInt& other) const {
    BigInt result;
    mulMagnitude(digits_, other.digits_, result.digits_);
    result.negative_ = (negative_ != other.negative_);
    result.normalize();
    return result;
}

void BigInt::divMagnitude(const std::vector<uint32_t>& dividend,
                          const std::vector<uint32_t>& divisor,
                          std::vector<uint32_t>& quotient,
                          std::vector<uint32_t>& remainder) {
    if (divisor.size() == 1 && divisor[0] == 0) {
        throw std::domain_error("Division by zero");
    }
    
    int cmp = compareMagnitude(dividend, divisor);
    if (cmp < 0) {
        quotient = {0};
        remainder = dividend;
        return;
    }
    if (cmp == 0) {
        quotient = {1};
        remainder = {0};
        return;
    }
    
    // Long division algorithm
    remainder = dividend;
    quotient.assign(dividend.size(), 0);
    
    for (int i = (int)dividend.size() * 32 - 1; i >= 0; i--) {
        // Shift remainder left by 1
        uint32_t carry = 0;
        for (size_t j = 0; j < remainder.size(); j++) {
            uint64_t temp = ((uint64_t)remainder[j] << 1) | carry;
            remainder[j] = temp & 0xFFFFFFFF;
            carry = temp >> 32;
        }
        if (carry) remainder.push_back(carry);
        
        // Set bit i of remainder from dividend
        size_t wordIdx = i / 32;
        size_t bitIdx = i % 32;
        if ((dividend[wordIdx] >> bitIdx) & 1) {
            remainder[0] |= 1;
        }
        
        // If remainder >= divisor, subtract and set quotient bit
        if (compareMagnitude(remainder, divisor) >= 0) {
            std::vector<uint32_t> temp;
            subMagnitude(remainder, divisor, temp);
            remainder = temp;
            quotient[wordIdx] |= (1U << bitIdx);
        }
    }
}

BigInt BigInt::operator/(const BigInt& other) const {
    BigInt result;
    std::vector<uint32_t> remainder;
    divMagnitude(digits_, other.digits_, result.digits_, remainder);
    result.negative_ = (negative_ != other.negative_);
    result.normalize();
    return result;
}

BigInt BigInt::operator%(const BigInt& other) const {
    BigInt result;
    std::vector<uint32_t> quotient;
    divMagnitude(digits_, other.digits_, quotient, result.digits_);
    result.negative_ = negative_;
    result.normalize();
    return result;
}

BigInt& BigInt::operator+=(const BigInt& other) {
    *this = *this + other;
    return *this;
}

BigInt& BigInt::operator-=(const BigInt& other) {
    *this = *this - other;
    return *this;
}

BigInt& BigInt::operator*=(const BigInt& other) {
    *this = *this * other;
    return *this;
}

BigInt BigInt::operator<<(size_t shift) const {
    if (isZero()) return *this;
    
    size_t wordShift = shift / 32;
    size_t bitShift = shift % 32;
    
    BigInt result;
    result.digits_.assign(wordShift, 0);
    
    if (bitShift == 0) {
        result.digits_.insert(result.digits_.end(), digits_.begin(), digits_.end());
    } else {
        uint32_t carry = 0;
        for (uint32_t digit : digits_) {
            uint64_t temp = ((uint64_t)digit << bitShift) | carry;
            result.digits_.push_back(temp & 0xFFFFFFFF);
            carry = temp >> 32;
        }
        if (carry) result.digits_.push_back(carry);
    }
    
    result.negative_ = negative_;
    result.normalize();
    return result;
}

BigInt BigInt::operator>>(size_t shift) const {
    if (isZero()) return *this;
    
    size_t wordShift = shift / 32;
    size_t bitShift = shift % 32;
    
    if (wordShift >= digits_.size()) {
        return BigInt(0);
    }
    
    BigInt result;
    result.digits_.clear();
    
    if (bitShift == 0) {
        result.digits_.assign(digits_.begin() + wordShift, digits_.end());
    } else {
        for (size_t i = wordShift; i < digits_.size(); i++) {
            uint32_t word = digits_[i] >> bitShift;
            if (i + 1 < digits_.size()) {
                word |= digits_[i + 1] << (32 - bitShift);
            }
            result.digits_.push_back(word);
        }
    }
    
    result.negative_ = negative_;
    result.normalize();
    return result;
}

BigInt BigInt::operator&(const BigInt& other) const {
    BigInt result;
    size_t minSize = std::min(digits_.size(), other.digits_.size());
    result.digits_.resize(minSize);
    
    for (size_t i = 0; i < minSize; i++) {
        result.digits_[i] = digits_[i] & other.digits_[i];
    }
    
    result.normalize();
    return result;
}

BigInt BigInt::operator|(const BigInt& other) const {
    BigInt result;
    size_t maxSize = std::max(digits_.size(), other.digits_.size());
    result.digits_.resize(maxSize);
    
    for (size_t i = 0; i < maxSize; i++) {
        uint32_t a = (i < digits_.size()) ? digits_[i] : 0;
        uint32_t b = (i < other.digits_.size()) ? other.digits_[i] : 0;
        result.digits_[i] = a | b;
    }
    
    result.normalize();
    return result;
}

BigInt BigInt::operator^(const BigInt& other) const {
    BigInt result;
    size_t maxSize = std::max(digits_.size(), other.digits_.size());
    result.digits_.resize(maxSize);
    
    for (size_t i = 0; i < maxSize; i++) {
        uint32_t a = (i < digits_.size()) ? digits_[i] : 0;
        uint32_t b = (i < other.digits_.size()) ? other.digits_[i] : 0;
        result.digits_[i] = a ^ b;
    }
    
    result.normalize();
    return result;
}

// Modular exponentiation: base^exp mod m
// Using right-to-left binary method with Montgomery reduction
BigInt BigInt::modPow(const BigInt& exponent, const BigInt& modulus) const {
    if (modulus.isZero() || modulus == BigInt(1)) {
        return BigInt(0);
    }
    
    BigInt result(1);
    BigInt base = *this % modulus;
    BigInt exp = exponent;
    
    while (!exp.isZero()) {
        if ((exp.digits_[0] & 1) == 1) {
            result = (result * base) % modulus;
        }
        base = (base * base) % modulus;
        exp = exp >> 1;
    }
    
    return result;
}

// Extended Euclidean Algorithm for modular inverse
BigInt BigInt::modInverse(const BigInt& modulus) const {
    BigInt a = *this % modulus;
    BigInt m = modulus;
    BigInt x0(0), x1(1);
    
    if (m == BigInt(1)) return BigInt(0);
    
    while (a > BigInt(1)) {
        BigInt q = a / m;
        BigInt t = m;
        
        m = a % m;
        a = t;
        
        t = x0;
        x0 = x1 - q * x0;
        x1 = t;
    }
    
    if (x1.negative_) {
        x1 = x1 + modulus;
    }
    
    return x1;
}

} // namespace Crypto
} // namespace RawrXD
