/**
 * @file RawrXD_QtCompat.hpp
 * @brief Qt compatibility shims - Replace Qt dependencies with pure C++20
 * 
 * This header provides minimal compatibility between Qt code and pure C++20.
 * As a long-term goal, all Qt usage should be eliminated and replaced with
 * the pure C++20 implementations in RawrXD_*.hpp files.
 */

#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

// Disable Qt includes - use pure C++ instead
// Note: We provide compatibility shims below for any legacy Qt code
// Long-term goal: eliminate all Qt usage and use pure C++20

namespace RawrXD::QtCompat {

// QString replacement
using QString = std::string;

// QStringList replacement
using QStringList = std::vector<std::string>;

// QJsonObject replacement  
using QJsonObject = nlohmann::json;

// QJsonArray replacement
using QJsonArray = nlohmann::json::array_t;

// QVariant replacement - simple tagged union
class QVariant {
public:
    enum Type { Invalid, Bool, Int, Double, String, List, Map };

    QVariant() : type_(Invalid) {}
    explicit QVariant(bool b) : type_(Bool), bool_val_(b) {}
    explicit QVariant(int i) : type_(Int), int_val_(i) {}
    explicit QVariant(double d) : type_(Double), double_val_(d) {}
    explicit QVariant(const std::string& s) : type_(String), string_val_(s) {}

    Type type() const { return type_; }
    bool toBool() const { return bool_val_; }
    int toInt() const { return int_val_; }
    double toDouble() const { return double_val_; }
    std::string toString() const { return string_val_; }

private:
    Type type_;
    bool bool_val_ = false;
    int int_val_ = 0;
    double double_val_ = 0.0;
    std::string string_val_;
};

// QUuid replacement - simple UUID implementation
class QUuid {
public:
    static QUuid createUuid();
    std::string toString() const;

private:
    std::string uuid_str_;
};

// QHash replacement
template<typename K, typename V>
using QHash = std::map<K, V>;

// QList replacement
template<typename T>
using QList = std::vector<T>;

// QMutex replacement - use std::mutex
using QMutex = std::mutex;
template<typename T>
using QMutexLocker = std::lock_guard<T>;

// QDateTime replacement
class QDateTime {
public:
    static QDateTime currentDateTime();
    long long toMSecsSinceEpoch() const;

private:
    long long timestamp_ = 0;
};

// QTimer replacement - simple timer callback
class QTimer {
public:
    void setInterval(int ms) { interval_ms_ = ms; }
    void start();
    void stop();
    void setSingleShot(bool single) { single_shot_ = single; }

    std::function<void()> on_timeout;

private:
    int interval_ms_ = 0;
    bool single_shot_ = false;
};

// QObject replacement - base class without signals/slots
class QObject {
public:
    virtual ~QObject() = default;
    
    template<typename T>
    void connect(T* sender, const char* signal, T* receiver, const char* slot) {
        // No-op in production - use callbacks instead
    }
};

// Signal/slot replacements - use std::function callbacks instead
#define signals
#define slots
#define emit
#define Q_OBJECT
#define Q_DISABLE_COPY(Class)
#define Q_PROPERTY(...)
#define Q_INVOKABLE
#define Q_ENUM(...)
#define qDebug() std::cerr << "[DEBUG] "
#define qWarning() std::cerr << "[WARNING] "
#define qCritical() std::cerr << "[CRITICAL] "
#define qInfo() std::cout << "[INFO] "

} // namespace RawrXD::QtCompat

// For convenience in Qt compatibility code
using QString = RawrXD::QtCompat::QString;
using QStringList = RawrXD::QtCompat::QStringList;
using QJsonObject = RawrXD::QtCompat::QJsonObject;
using QJsonArray = RawrXD::QtCompat::QJsonArray;
using QVariant = RawrXD::QtCompat::QVariant;
using QUuid = RawrXD::QtCompat::QUuid;
using QHash = RawrXD::QtCompat::QHash;
using QList = RawrXD::QtCompat::QList;
using QMutex = RawrXD::QtCompat::QMutex;
using QMutexLocker = RawrXD::QtCompat::QMutexLocker;
using QDateTime = RawrXD::QtCompat::QDateTime;
using QTimer = RawrXD::QtCompat::QTimer;
using QObject = RawrXD::QtCompat::QObject;
