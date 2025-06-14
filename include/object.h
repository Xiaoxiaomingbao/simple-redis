#pragma once

#include "SkipList.cpp"

#include <string>
#include <variant>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class RedisString {
public:

    enum class Encoding {
        ONLY_STRING, STRING_INT, STRING_DOUBLE, NONE
    };

    explicit RedisString(const std::string& str);

    explicit RedisString();

    Encoding encoding() const;

    std::string std_string() const;

    void update_num(int delta); // used only when encoding_ is STRING_INT

    void update_num(double delta);

private:

    void parse_str(); // parse str to get num

    void update_str(); // after num is calculated, set str

    std::variant<int, double, std::nullptr_t> num;
    std::string str;
    Encoding encoding_;
};

struct ZSet {
    SkipList skipList;
    std::unordered_map<std::string, double> map;
};

class RedisObject {
public:

    enum class Type {
        STRING, LIST, SET, HASH, ZSET
    };

    enum class Encoding {
        REDIS_STRING, STD_VECTOR, STD_UNORDERED_SET, STD_UNORDERED_MAP, SKIPLIST_STD_UNORDERED_MAP
    };

    explicit RedisObject(Type type);

    Type type() const;

    Encoding encoding() const;

    // String
    std::string get() const;
    std::string set(const std::string& value);
    std::string incr();
    std::string incr_by(int increment);
    std::string incr_by_float(double increment);

    // List
    std::string l_push(const std::string& value);
    std::string l_pop();
    std::string r_push(const std::string& value);
    std::string r_pop();
    std::string l_range(int start, int end) const; // start & end included, same below
    std::string l_len() const;

    // Hash
    std::string h_set(const std::string& field, const std::string& value);
    std::string h_get(const std::string& field);
    std::string h_get_all() const;
    std::string h_keys() const;
    std::string h_vals() const;
    std::string h_set_n_x(const std::string& field, const std::string& value);
    std::string h_incr_by(const std::string& field, int increment);
    std::string h_incr_by_float(const std::string& field, double increment);

    // Set
    std::string s_add(const std::string& member);
    std::string s_rem(const std::string& member);
    std::string s_card() const;
    std::string s_is_member(const std::string& member) const;
    std::string s_members() const;
    std::string s_inter(const RedisObject& other) const;
    std::string s_diff(const RedisObject& other) const;
    std::string s_union(const RedisObject& other) const;

    // ZSet
    std::string z_add(double score, const std::string& member);
    std::string z_rem(const std::string& member);
    std::string z_score(const std::string& member) const;
    std::string z_rank(const std::string& member, bool with_score) const; // 0-based index
    std::string z_card() const;
    std::string z_count(double min, double max) const;
    std::string z_incr_by(double increment, const std::string& member);
    std::string z_range(int idx1, int idx2, bool with_scores) const;
    std::string z_range_by_score(double min, bool minExclusive, double max, bool maxExclusive, bool with_scores) const;
    std::string z_inter(const RedisObject& other) const; // add the score of common members
    std::string z_union(const RedisObject& other) const; // add the score of common members

private:
    std::variant<RedisString, std::vector<std::string>,
        std::unordered_map<std::string, RedisString>,
        std::unordered_set<std::string>, ZSet> value;
    Type type_;
    Encoding encoding_;
};
