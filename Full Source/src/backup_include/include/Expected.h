#pragma once
#include <variant>
#include <stdexcept>

namespace RawrXD {

template<typename E>
class Unexpected {
public:
    explicit Unexpected(E error) : m_error(error) {}
    E error() const { return m_error; }
private:
    E m_error;
};

template<typename E>
Unexpected<E> unexpected(E error) {
    return Unexpected<E>(error);
}

template<typename T, typename E>
class Expected {
public:
    Expected(T value) : m_data(value) {}
    Expected(Unexpected<E> error) : m_data(error) {}

    bool has_value() const { return std::holds_alternative<T>(m_data); }
    explicit operator bool() const { return has_value(); }

    T& value() {
        if (!has_value()) throw std::runtime_error("Accessing empty expected");
        return std::get<T>(m_data);
    }
    const T& value() const {
        if (!has_value()) throw std::runtime_error("Accessing empty expected");
        return std::get<T>(m_data);
    }

    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    T& operator*() { return value(); }
    const T& operator*() const { return value(); }

    E error() const {
        if (has_value()) throw std::runtime_error("Accessing error of valid expected");
        return std::get<Unexpected<E>>(m_data).error();
    }

private:
    std::variant<T, Unexpected<E>> m_data;
};

template<typename E>
class Expected<void, E> {
public:
    Expected() : m_has_value(true) {}
    Expected(Unexpected<E> error) : m_has_value(false), m_error(error.error()) {}

    bool has_value() const { return m_has_value; }
    explicit operator bool() const { return has_value(); }

    void value() const {
        if (!has_value()) throw std::runtime_error("Accessing empty expected");
    }

    E error() const {
        if (has_value()) throw std::runtime_error("Accessing error of valid expected");
        return m_error;
    }

private:
    bool m_has_value;
    E m_error;
};

} // namespace RawrXD
