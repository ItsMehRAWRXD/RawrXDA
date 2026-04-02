#pragma once

// Compatibility layer for C++23 features on older compilers

#if defined(_MSC_VER) && _MSC_VER < 1930
// MSVC < 19.30 doesn't have std::expected
#include <optional>
#include <variant>

namespace std {
    template<typename E>
    class unexpected {
    public:
        constexpr explicit unexpected(E e) : m_error(std::move(e)) {}
        constexpr const E& error() const & noexcept { return m_error; }
        constexpr E& error() & noexcept { return m_error; }
    private:
        E m_error;
    };

    template<typename T, typename E>
    class expected {
    public:
        constexpr expected() : m_val() {}
        constexpr expected(const T& val) : m_val(val) {}
        constexpr expected(T&& val) : m_val(std::move(val)) {}
        constexpr expected(const unexpected<E>& unex) : m_val(unex.error()) {}
        constexpr expected(unexpected<E>&& unex) : m_val(std::move(unex.error())) {}

        constexpr explicit operator bool() const noexcept { return m_val.index() == 0; }
        constexpr bool has_value() const noexcept { return m_val.index() == 0; }

        constexpr const T& value() const & { return std::get<0>(m_val); }
        constexpr T& value() & { return std::get<0>(m_val); }
        
        constexpr const E& error() const & { return std::get<1>(m_val); }
        constexpr E& error() & { return std::get<1>(m_val); }

    private:
        std::variant<T, E> m_val;
    };

    template<typename E>
    class expected<void, E> {
    public:
        constexpr expected() : m_error() {}
        constexpr expected(const unexpected<E>& unex) : m_error(unex.error()) {}
        constexpr expected(unexpected<E>&& unex) : m_error(std::move(unex.error())) {}

        constexpr explicit operator bool() const noexcept { return !m_error.has_value(); }
        constexpr bool has_value() const noexcept { return !m_error.has_value(); }
        
        constexpr const E& error() const & { return *m_error; }
        constexpr E& error() & { return *m_error; }

    private:
        std::optional<E> m_error;
    };
}
#else
// Use standard library versions
#include <expected>
#include <optional>
#endif
