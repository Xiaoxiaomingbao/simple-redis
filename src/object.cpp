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

std::string RedisString::std_string() const {
    switch (this->encoding_) {
        case Encoding::INT:
            return std::to_string(std::get<int>(this->str));
        case Encoding::DOUBLE:
            return std::to_string(std::get<double>(this->str));
        case Encoding::STD_STRING:
            return std::get<std::string>(this->str);
        default:
            return "(nil)";
    }
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
            break;
        case Type::LIST:
            this->type_ = Type::LIST;
            this->encoding_ = Encoding::STD_VECTOR;
            this->value = std::vector<RedisString>();
            break;
        case Type::HASH:
            this->type_ = Type::HASH;
            this->encoding_ = Encoding::STD_UNORDERED_MAP;
            this->value = std::unordered_map<std::string, RedisString>();
            break;
        case Type::SET:
            this->type_ = Type::SET;
            this->encoding_ = Encoding::STD_UNORDERED_SET;
            this->value = std::unordered_set<RedisString, RedisStringHasher, RedisStringEqual>{};
            break;
        case Type::ZSET:
            this->type_ = Type::ZSET;
            this->encoding_ = Encoding::STD_MAP;
            this->value = std::map<double, RedisString>();
            break;
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
    if (this->type_ != Type::STRING) return "Redis object type error";
    const auto rs = std::get<RedisString>(this->value);
    return rs.std_string();
}

std::string RedisObject::set(const std::string& value) {
    if (this->type_ != Type::STRING) return "Redis object type error";
    this->value = RedisString(value);
    return "OK";
}

std::string RedisObject::incr() {
    if (this->type_ != Type::STRING) return "Redis object type error";
    return incr_by(1);
}

std::string RedisObject::incr_by(const int stride) {
    if (this->type_ != Type::STRING) return "Redis object type error";
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
            return "String encoding not supported for incrementation";
        default:
            return "Unexpected error";
    }
}

std::string RedisObject::incr_by_float(const double stride) {
    if (this->type_ != Type::STRING) return "Redis object type error";
    double new_double;
    switch (auto& rs = std::get<RedisString>(this->value); rs.encoding()) {
        case RedisString::Encoding::INT:
            rs.convert_num_type(); // then matched to next case
        case RedisString::Encoding::DOUBLE:
            new_double = rs.get_double() + stride;
            rs.update_content(new_double);
            return std::to_string(new_double);
        case RedisString::Encoding::STD_STRING:
            return "String encoding not supported for incrementation";
        default:
            return "Unexpected error";
    }
}

// List
std::string RedisObject::l_push(const std::string& value) {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<RedisString>>(this->value);
    list.insert(list.begin(), RedisString(value));
    return "OK";
}

std::string RedisObject::l_pop() {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<RedisString>>(this->value);
    if (list.empty()) return "(nil)";
    const RedisString val = list.front();
    list.erase(list.begin());
    return val.std_string();
}

std::string RedisObject::r_push(const std::string& value) {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<RedisString>>(this->value);
    list.emplace_back(value);
    return "OK";
}

std::string RedisObject::r_pop() {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<RedisString>>(this->value);
    if (list.empty()) return "(nil)";
    const RedisString val = list.back();
    list.pop_back();
    return val.std_string();
}

std::string RedisObject::l_range(int start, int end) const {
    if (this->type_ != Type::LIST) return "Redis object type error";
    const auto& list = std::get<std::vector<RedisString>>(this->value);
    const int size = list.size();

    // minus index
    if (start < 0) start += size;
    if (end < 0) end += size;

    // calculate border
    start = std::max(0, start);
    end = std::min(size - 1, end);
    if (start > end) return "(empty list)";

    std::string result;
    for (size_t i = start; i <= end; ++i) {
        result += list[i].std_string() + "\n";
    }
    return result.empty() ? "(empty list)" : result;
}

std::string RedisObject::l_len() const {
    if (this->type_ != Type::LIST) return "Redis object type error";
    const auto& list = std::get<std::vector<RedisString>>(this->value);
    return std::to_string(list.size());
}


