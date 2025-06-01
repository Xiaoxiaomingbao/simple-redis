#pragma once
#include <string>
#include <variant>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <map>

class RedisString {
public:

    enum class Encoding {
        STD_STRING, INT, DOUBLE, NONE
    };

    explicit RedisString(const std::string& str);

    explicit RedisString();

    Encoding encoding() const;

    std::string get_string() const;

    int get_int() const;

    double get_double() const;

    std::string std_string() const;

    void update_content(const std::string& new_value); // Type not changed

    void update_content(int new_value); // Type not changed

    void update_content(double new_value); // Type not changed

    void convert_num_type(); // Type converted from int to double

private:
    std::variant<std::string, int, double, std::nullptr_t> str;
    Encoding encoding_;
};

struct RedisStringHasher {
    std::size_t operator()(const RedisString& rs) const {
        switch (rs.encoding()) {
            case RedisString::Encoding::INT:
                return std::hash<int>()(rs.get_int());
            case RedisString::Encoding::DOUBLE:
                return std::hash<double>()(rs.get_double());
            case RedisString::Encoding::STD_STRING:
                return std::hash<std::string>()(rs.get_string());
            default:
                return 0;
        }
    }
};

struct RedisStringEqual {
    bool operator()(const RedisString& a, const RedisString& b) const {
        if (a.encoding() == b.encoding()) {
            switch (a.encoding()) {
                case RedisString::Encoding::INT:
                    return a.get_int() == b.get_int();
                case RedisString::Encoding::DOUBLE:
                    return a.get_double() == b.get_double();
                case RedisString::Encoding::STD_STRING:
                    return a.get_string() == b.get_string();
                default: ;
            }
        }
        return false;
    }
};

class RedisObject {
public:

    enum class Type {
        STRING, LIST, SET, HASH, ZSET
    };

    enum class Encoding {
        REDIS_STRING, STD_VECTOR, STD_UNORDERED_SET, STD_UNORDERED_MAP, STD_MAP
    };

    explicit RedisObject(Type type);

    Type type() const;

    Encoding encoding() const;

    // String
    std::string get() const;
    std::string set(const std::string& value);
    std::string incr();
    std::string incr_by(int stride);
    std::string incr_by_float(double stride);

    // List
    std::string l_push(const std::string& value);
    std::string l_pop();
    std::string r_push(const std::string& value);
    std::string r_pop();
    std::string l_range(int start, int end) const;
    std::string l_len() const;

private:
    std::variant<RedisString, std::vector<RedisString>,
        std::unordered_set<RedisString, RedisStringHasher, RedisStringEqual>,
        std::unordered_map<std::string, RedisString>, std::map<double, RedisString>> value;
    Type type_;
    Encoding encoding_;
};
