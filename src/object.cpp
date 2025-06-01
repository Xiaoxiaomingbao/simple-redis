#include "object.h"

#include <stdexcept>
#include <charconv>

RedisString::RedisString(const std::string& str) {
    int int_val;
    auto int_result = std::from_chars(str.data(), str.data() + str.size(), int_val);
    if (int_result.ec == std::errc() && int_result.ptr == str.data() + str.size()) {
        this->str = int_val;
        this->encoding_ = Encoding::INT;
        return;
    }

    double double_val;
    auto double_result = std::from_chars(str.data(), str.data() + str.size(), double_val);
    if (double_result.ec == std::errc() && double_result.ptr == str.data() + str.size()) {
        this->str = double_val;
        this->encoding_ = Encoding::DOUBLE;
        return;
    }

    this->str = str;
    this->encoding_ = Encoding::STD_STRING;
}

RedisString::RedisString() {
    this->str = nullptr;
    this->encoding_ = Encoding::NONE;
}


inline RedisString::Encoding RedisString::encoding() const {
    return this->encoding_;
}

inline int RedisString::get_int() const {
    return std::get<int>(this->str);
}

inline double RedisString::get_double() const {
    return std::get<double>(this->str);
}

inline std::string RedisString::get_string() const {
    return std::get<std::string>(this->str);
}

inline void RedisString::update_content(const std::string& new_value) {
    this->str = new_value;
}

inline void RedisString::update_content(int new_value) {
    this->str = new_value;
}

inline void RedisString::update_content(double new_value) {
    this->str = new_value;
}

void RedisString::convert_num_type() {
    const int int_value = std::get<int>(this->str);
    auto double_value = static_cast<double>(int_value);
    this->str = double_value;
    this->encoding_ = Encoding::DOUBLE;
}

RedisObject::RedisObject(const Type type) {
    switch (type) {
        case Type::STRING:
            this->type_ = Type::STRING;
            this->encoding_ = Encoding::REDIS_STRING;
            this->value = RedisString();
        case Type::LIST:
            this->type_ = Type::LIST;
            this->encoding_ = Encoding::STD_VECTOR;
            this->value = std::vector<RedisString>();
        case Type::HASH:
            this->type_ = Type::HASH;
            this->encoding_ = Encoding::STD_UNORDERED_MAP;
            this->value = std::unordered_map<std::string, RedisString>();
        case Type::SET:
            this->type_ = Type::SET;
            this->encoding_ = Encoding::STD_UNORDERED_SET;
            this->value = std::unordered_set<RedisString, RedisStringHasher, RedisStringEqual>{};
        case Type::ZSET:
            this->type_ = Type::ZSET;
            this->encoding_ = Encoding::STD_MAP;
            this->value = std::map<double, RedisString>();
        default: ;
    }
}

inline RedisObject::Type RedisObject::type() const {
    return this->type_;
}

inline RedisObject::Encoding RedisObject::encoding() const {
    return this->encoding_;
}

// String
std::string RedisObject::get() const {
    switch (const auto rs = std::get<RedisString>(this->value); rs.encoding()) {
        case RedisString::Encoding::INT:
            return std::to_string(rs.get_int());
        case RedisString::Encoding::DOUBLE:
            return std::to_string(rs.get_double());
        case RedisString::Encoding::STD_STRING:
            return rs.get_string();
        default:
            return "(nil)";
    }
}

std::string RedisObject::set(const std::string& value) {
    this->value = RedisString(value);
    return "OK";
}

std::string RedisObject::incr() {
    return incr_by(1);
}

std::string RedisObject::incr_by(const int stride) {
    int new_int;
    double new_double;
    switch (auto& rs = std::get<RedisString>(this->value); rs.encoding()) {
        case RedisString::Encoding::INT:
            new_int = rs.get_int() + stride;
            rs.update_content(new_int);
            return std::to_string(new_int);
        case RedisString::Encoding::DOUBLE:
            new_double = rs.get_double() + stride;
            rs.update_content(new_double);
            return std::to_string(new_double);
        case RedisString::Encoding::STD_STRING:
            return "String encoding not supported for incrementation";;
        default:
            return "Unexpected error";
    }
}

std::string RedisObject::incr_by_float(const double stride) {
    double new_double;
    switch (auto& rs = std::get<RedisString>(this->value); rs.encoding()) {
        case RedisString::Encoding::INT:
            rs.convert_num_type(); // then matched to next case
        case RedisString::Encoding::DOUBLE:
            new_double = rs.get_double() + stride;
            rs.update_content(new_double);
            return std::to_string(new_double);
        case RedisString::Encoding::STD_STRING:
            return "String encoding not supported for incrementation";;
        default:
            return "Unexpected error";
    }
}

