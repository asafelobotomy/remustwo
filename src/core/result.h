#pragma once

#include <QString>
#include <utility>

namespace remustwo {

/**
 * Lightweight result type carrying a value or an error message.
 *
 * Similar to std::expected (C++23) but available in C++20.
 */
template <typename T> class Result {
    struct ErrorTag { };

public:
    static Result ok(T value) {
        return Result(std::move(value));
    }

    static Result fail(QString error) {
        return Result(ErrorTag { }, std::move(error));
    }

    explicit operator bool() const {
        return m_hasValue;
    }
    bool hasValue() const {
        return m_hasValue;
    }

    const T &value() const & {
        return m_value;
    }
    T &&value() && {
        return std::move(m_value);
    }
    const T &operator*() const & {
        return m_value;
    }
    T &&operator*() && {
        return std::move(m_value);
    }
    const T *operator->() const {
        return &m_value;
    }

    const QString &error() const {
        return m_error;
    }

    T valueOr(T fallback) const {
        return m_hasValue ? m_value : std::move(fallback);
    }

private:
    explicit Result(T value)
        : m_value(std::move(value))
        , m_hasValue(true) { }
    explicit Result(ErrorTag, QString error)
        : m_error(std::move(error))
        , m_hasValue(false) { }

    T m_value { };
    QString m_error;
    bool m_hasValue = false;
};

} // namespace remustwo

namespace remustwo {

template <> class Result<void> {
    struct ErrorTag { };

public:
    static Result ok() {
        return Result { };
    }

    static Result fail(QString error) {
        return Result(ErrorTag { }, std::move(error));
    }

    explicit operator bool() const {
        return m_hasValue;
    }
    bool hasValue() const {
        return m_hasValue;
    }

    const QString &error() const {
        return m_error;
    }

private:
    Result() = default;
    explicit Result(ErrorTag, QString error)
        : m_error(std::move(error))
        , m_hasValue(false) { }

    QString m_error;
    bool m_hasValue = true;
};

} // namespace remustwo
