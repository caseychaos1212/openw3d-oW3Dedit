#pragma once
#ifndef JSON_COMPAT_QT_SHIM
#define JSON_COMPAT_QT_SHIM

#include <nlohmann/json.hpp>
#include <QString>
#include <QByteArray>
#include <string>
#include <type_traits>
#include <utility>
#include <initializer_list>
#include <cctype>
#include <algorithm>

class QJsonObject;
class QJsonArray;
class QJsonValue;
class QJsonValueRef;

namespace json_compat_detail {

    struct JsonEntry;


inline std::string toKey(const char* key) { return key ? std::string(key) : std::string(); }
inline std::string toKey(const std::string& key) { return key; }
inline std::string toKey(const QString& key) { return key.toStdString(); }
}

class QJsonValue {
public:
    QJsonValue() : data_(nullptr) {}
    explicit QJsonValue(const nlohmann::ordered_json& value) : data_(value) {}
    explicit QJsonValue(nlohmann::ordered_json&& value) : data_(std::move(value)) {}
    QJsonValue(bool value) : data_(value) {}
    QJsonValue(int value) : data_(value) {}
    QJsonValue(unsigned int value) : data_(value) {}
    QJsonValue(int64_t value) : data_(value) {}
    QJsonValue(uint64_t value) : data_(value) {}
    QJsonValue(double value) : data_(value) {}
    QJsonValue(float value) : data_(static_cast<double>(value)) {}
    QJsonValue(const char* value) : data_(value ? nlohmann::ordered_json(value) : nlohmann::ordered_json(nullptr)) {}
    QJsonValue(const std::string& value) : data_(value) {}
    QJsonValue(const QString& value) : data_(value.toStdString()) {}
    QJsonValue(const QByteArray& value) : data_(std::string(value.constData(), value.size())) {}
    QJsonValue(const QJsonObject& value);
    QJsonValue(const QJsonArray& value);

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T> && !std::is_same_v<T, bool>>> 
    QJsonValue(T value) : data_(static_cast<double>(value)) {}

    int toInt(int defaultValue = 0) const;
    double toDouble(double defaultValue = 0.0) const;
    bool toBool(bool defaultValue = false) const;
    QString toString() const;
    QJsonObject toObject() const;
    QJsonArray toArray() const;
    bool isArray() const { return data_.is_array(); }
    bool isObject() const { return data_.is_object(); }
    bool isString() const { return data_.is_string(); }
    bool isNull() const { return data_.is_null(); }

    const nlohmann::ordered_json& ordered() const { return data_; }
    nlohmann::ordered_json& ordered() { return data_; }

private:
    nlohmann::ordered_json data_;
};

namespace json_compat_detail {

    struct JsonEntry {
        QString key;
        QJsonValue value;

        JsonEntry(QString k, QJsonValue v)
            : key(std::move(k)), value(std::move(v)) {}

        template <typename V>
        JsonEntry(const char* k, V&& v)
            : key(QString::fromUtf8(k)), value(QJsonValue(std::forward<V>(v))) {}

        template <typename V>
        JsonEntry(const QString& k, V&& v)
            : key(k), value(QJsonValue(std::forward<V>(v))) {}
    };

} // namespace json_compat_detail


class QJsonArray {
public:
    QJsonArray() : data_(nlohmann::ordered_json::array()) {}
    explicit QJsonArray(const nlohmann::ordered_json& value) : data_(value.is_array() ? value : nlohmann::ordered_json::array()) {}

    template <typename T>
    QJsonArray(std::initializer_list<T> init) : data_(nlohmann::ordered_json::array()) {
        for (const auto& v : init) {
            append(QJsonValue(v));
        }
    }

    QJsonArray& append(const QJsonValue& value) { data_.push_back(value.ordered()); return *this; }
    QJsonArray& append(const QJsonObject& value);
    QJsonArray& append(const QJsonArray& value);

    template <typename T,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, QJsonValue> &&
                                          !std::is_same_v<std::decay_t<T>, QJsonObject> &&
                                          !std::is_same_v<std::decay_t<T>, QJsonArray>>> 
    QJsonArray& append(const T& value) { return append(QJsonValue(value)); }

    int size() const { return static_cast<int>(data_.size()); }
    bool isEmpty() const { return data_.empty(); }

    QJsonValue operator[](int index) const { return QJsonValue(data_.at(static_cast<size_t>(index))); }

    const nlohmann::ordered_json& ordered() const { return data_; }
    nlohmann::ordered_json& ordered() { return data_; }

private:
    nlohmann::ordered_json data_;
};

class QJsonValueRef {
public:
    explicit QJsonValueRef(nlohmann::ordered_json& value) : value_(&value) {}

    QJsonValueRef& operator=(const QJsonValue& value) { *value_ = value.ordered(); return *this; }
    QJsonValueRef& operator=(const QJsonObject& value);
    QJsonValueRef& operator=(const QJsonArray& value);
    QJsonValueRef& operator=(const QString& value) { *value_ = value.toStdString(); return *this; }
    QJsonValueRef& operator=(const QByteArray& value) { *value_ = std::string(value.constData(), value.size()); return *this; }
    QJsonValueRef& operator=(const char* value) { *value_ = value ? nlohmann::ordered_json(value) : nlohmann::ordered_json(nullptr); return *this; }

    template <typename T, typename = std::enable_if_t<std::is_arithmetic_v<T>>> 
    QJsonValueRef& operator=(T value) { *value_ = value; return *this; }

    operator QJsonValue() const { return QJsonValue(*value_); }

private:
    nlohmann::ordered_json* value_;
};

class QJsonObject {
public:
    QJsonObject() : data_(nlohmann::ordered_json::object()) {}
    explicit QJsonObject(const nlohmann::ordered_json& value) : data_(value.is_object() ? value : nlohmann::ordered_json::object()) {}

    QJsonObject(std::initializer_list<json_compat_detail::JsonEntry> init) : QJsonObject() {
        for (const auto& entry : init) {
            (*this)[entry.key] = entry.value;
        }
    }

    QJsonValue value(const char* key) const;
    QJsonValue value(const std::string& key) const;
    QJsonValue value(const QString& key) const;

    bool contains(const char* key) const;
    bool contains(const std::string& key) const;
    bool contains(const QString& key) const;

    QJsonValueRef operator[](const char* key);
    QJsonValueRef operator[](const std::string& key);
    QJsonValueRef operator[](const QString& key);

    const nlohmann::ordered_json& ordered() const { return data_; }
    nlohmann::ordered_json& ordered() { return data_; }

private:
    nlohmann::ordered_json data_;
};

inline QJsonValue::QJsonValue(const QJsonObject& value) : data_(value.ordered()) {}
inline QJsonValue::QJsonValue(const QJsonArray& value) : data_(value.ordered()) {}
inline QJsonObject QJsonValue::toObject() const { return QJsonObject(data_); }
inline QJsonArray QJsonValue::toArray() const { return QJsonArray(data_); }

inline int QJsonValue::toInt(int defaultValue) const {
    if (data_.is_number_integer()) return data_.get<int>();
    if (data_.is_number_unsigned()) return static_cast<int>(data_.get<unsigned int>());
    if (data_.is_number_float()) return static_cast<int>(data_.get<double>());
    if (data_.is_boolean()) return data_.get<bool>() ? 1 : 0;
    if (data_.is_string()) {
        try {
            return std::stoi(data_.get<std::string>());
        }
        catch (...) {}
    }
    return defaultValue;
}

inline double QJsonValue::toDouble(double defaultValue) const {
    if (data_.is_number()) return data_.get<double>();
    if (data_.is_boolean()) return data_.get<bool>() ? 1.0 : 0.0;
    if (data_.is_string()) {
        try {
            return std::stod(data_.get<std::string>());
        }
        catch (...) {}
    }
    return defaultValue;
}

inline bool QJsonValue::toBool(bool defaultValue) const {
    if (data_.is_boolean()) return data_.get<bool>();
    if (data_.is_number()) return data_.get<double>() != 0.0;
    if (data_.is_string()) {
        auto s = data_.get<std::string>();
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s == "true" || s == "1";
    }
    return defaultValue;
}

inline QString QJsonValue::toString() const {
    if (data_.is_string()) return QString::fromStdString(data_.get<std::string>());
    if (data_.is_number_integer()) return QString::number(static_cast<long long>(data_.get<long long>()));
    if (data_.is_number_unsigned()) return QString::fromStdString(std::to_string(data_.get<unsigned long long>()));
    if (data_.is_number_float()) return QString::number(data_.get<double>(), 'g', 15);
    if (data_.is_boolean()) return data_.get<bool>() ? QStringLiteral("true") : QStringLiteral("false");
    return QString();
}

inline QJsonArray& QJsonArray::append(const QJsonObject& value) { data_.push_back(value.ordered()); return *this; }
inline QJsonArray& QJsonArray::append(const QJsonArray& value) { data_.push_back(value.ordered()); return *this; }

inline QJsonValue QJsonObject::value(const char* key) const {
    auto it = data_.find(key ? key : "");
    if (it == data_.end()) return QJsonValue();
    return QJsonValue(*it);
}
inline QJsonValue QJsonObject::value(const std::string& key) const {
    auto it = data_.find(key);
    if (it == data_.end()) return QJsonValue();
    return QJsonValue(*it);
}
inline QJsonValue QJsonObject::value(const QString& key) const {
    return value(key.toStdString());
}
inline bool QJsonObject::contains(const char* key) const {
    return data_.contains(key ? key : "");
}
inline bool QJsonObject::contains(const std::string& key) const {
    return data_.contains(key);
}
inline bool QJsonObject::contains(const QString& key) const {
    return data_.contains(key.toStdString());
}
inline QJsonValueRef QJsonObject::operator[](const char* key) {
    return QJsonValueRef(data_[key ? key : ""]);
}
inline QJsonValueRef QJsonObject::operator[](const std::string& key) {
    return QJsonValueRef(data_[key]);
}
inline QJsonValueRef QJsonObject::operator[](const QString& key) {
    return QJsonValueRef(data_[key.toStdString()]);
}

inline QJsonValueRef& QJsonValueRef::operator=(const QJsonObject& value) {
    *value_ = value.ordered();
    return *this;
}
inline QJsonValueRef& QJsonValueRef::operator=(const QJsonArray& value) {
    *value_ = value.ordered();
    return *this;
}

inline nlohmann::ordered_json toOrdered(const QJsonObject& obj) { return obj.ordered(); }
inline nlohmann::ordered_json toOrdered(const QJsonArray& arr) { return arr.ordered(); }
inline QJsonObject toQJsonObject(const nlohmann::ordered_json& value) { return QJsonObject(value); }
inline QJsonArray toQJsonArray(const nlohmann::ordered_json& value) { return QJsonArray(value); }

#endif // JSON_COMPAT_QT_SHIM
