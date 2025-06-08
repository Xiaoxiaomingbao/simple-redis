#include "object.h"

#include <stdexcept>

RedisString::RedisString(const std::string& str) {
    this->str = str;
    parse_str(); // num and encoding_ will be set here
}

RedisString::RedisString() {
    this->str = "";
    this->num = nullptr;
    this->encoding_ = Encoding::NONE;
}


inline RedisString::Encoding RedisString::encoding() const {
    return this->encoding_;
}

std::string RedisString::std_string() const {
    switch (this->encoding_) {
        case Encoding::STRING_INT:
        case Encoding::STRING_DOUBLE:
            return str;
        case Encoding::ONLY_STRING:
            return "\"" + str + "\"";
        default:
            return "(nil)";
    }
}

void RedisString::update_num(const int delta) {
    int val = std::get<int>(this->num);
    val += delta;
    num = val;
    update_str();
}

void RedisString::update_num(const double delta) {
    double val;
    switch (this->encoding_) {
        case Encoding::STRING_INT:
            val = std::get<int>(this->num);
            break;
        case Encoding::STRING_DOUBLE:
            val = std::get<double>(this->num);
            break;
        default:
            return;
    }
    val += delta;
    if (int tried_val = static_cast<int>(val); tried_val == val) {
        num = tried_val;
        encoding_ = Encoding::STRING_INT;
    } else {
        num = val;
        encoding_ = Encoding::STRING_DOUBLE;
    }
    update_str();
}

void RedisString::parse_str() {
    if (str.empty()) {
        num = nullptr;
        encoding_ = Encoding::NONE;
        return;
    }

    bool is_strict_integer = true;

    // Check if a string is a pure integer (no leading sign, no leading zero unless it's just "0")
    if (str != "0" && str[0] == '0') is_strict_integer = false;
    for (const char ch : str) {
        if (!std::isdigit(ch)) is_strict_integer = false;
    }

    if (is_strict_integer) {
        // try to parse str as int
        try {
            num = std::stoi(str);
            encoding_ = Encoding::STRING_INT;
            return;
        } catch (...) {} // out of range error
    }

    // try to parse str as double
    size_t pos;
    try {
        // leading sign, leading zeros, scientific notation are allowed
        double val = std::stod(str, &pos);
        if (pos == str.size()) {
            // trailing junk is not allowed
            num = val;
            encoding_ = Encoding::STRING_DOUBLE;
            return;
        }
    } catch (...) {}

    num = nullptr;
    encoding_ = Encoding::ONLY_STRING;
}

void RedisString::update_str() {
    int int_val;
    double double_val;
    switch (this->encoding_) {
        case Encoding::STRING_INT:
            int_val = std::get<int>(this->num);
            str = std::to_string(int_val);
            break;
        case Encoding::STRING_DOUBLE:
            double_val = std::get<double>(this->num);
            str = std::to_string(double_val);
            break;
        default: ;
    }
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
            this->value = std::vector<std::string>();
            break;
        case Type::HASH:
            this->type_ = Type::HASH;
            this->encoding_ = Encoding::STD_UNORDERED_MAP;
            this->value = std::unordered_map<std::string, RedisString>();
            break;
        case Type::SET:
            this->type_ = Type::SET;
            this->encoding_ = Encoding::STD_UNORDERED_SET;
            this->value = std::unordered_set<std::string>{};
            break;
        case Type::ZSET:
            this->type_ = Type::ZSET;
            this->encoding_ = Encoding::STD_SET_STD_UNORDERED_MAP;
            this->value = ZSet{};
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
    return incr_by(1);
}

std::string RedisObject::incr_by(const int increment) {
    if (this->type_ != Type::STRING) return "Redis object type error";
    // encoding_ must be STRING_INT
    if (auto& rs = std::get<RedisString>(this->value); rs.encoding() == RedisString::Encoding::STRING_INT) {
        rs.update_num(increment);
        return rs.std_string();
    }
    return "Redis string can not be recognized as an integer";
}

std::string RedisObject::incr_by_float(const double increment) {
    if (this->type_ != Type::STRING) return "Redis object type error";
    switch (auto& rs = std::get<RedisString>(this->value); rs.encoding()) {
        case RedisString::Encoding::STRING_INT:
        case RedisString::Encoding::STRING_DOUBLE:
            rs.update_num(increment);
            return rs.std_string();
        default:
            return "Redis string can not be recognized as a number";
    }
}

// List
std::string RedisObject::l_push(const std::string& value) {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<std::string>>(this->value);
    list.insert(list.begin(), value);
    return "OK";
}

std::string RedisObject::l_pop() {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<std::string>>(this->value);
    if (list.empty()) return "(nil)";
    auto val = list.front();
    list.erase(list.begin());
    return val;
}

std::string RedisObject::r_push(const std::string& value) {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<std::string>>(this->value);
    list.emplace_back(value);
    return "OK";
}

std::string RedisObject::r_pop() {
    if (this->type_ != Type::LIST) return "Redis object type error";
    auto& list = std::get<std::vector<std::string>>(this->value);
    if (list.empty()) return "(nil)";
    auto val = list.back();
    list.pop_back();
    return val;
}

std::string RedisObject::l_range(int start, int end) const {
    if (this->type_ != Type::LIST) return "Redis object type error";
    const auto& list = std::get<std::vector<std::string>>(this->value);
    const int size = list.size();

    // minus index
    if (start < 0) start += size;
    if (end < 0) end += size;

    // calculate border
    start = std::max(0, start);
    end = std::min(size - 1, end);
    if (start > end) return "(empty array)";

    std::string result;
    for (size_t i = start; i <= end; ++i) {
        result += std::to_string(i - start + 1) + ") " + list[i];
        if (i < end) result += "\n";
    }
    return result.empty() ? "(empty array)" : result;
}

std::string RedisObject::l_len() const {
    if (this->type_ != Type::LIST) return "Redis object type error";
    const auto& list = std::get<std::vector<std::string>>(this->value);
    return std::to_string(list.size());
}

// Hash
std::string RedisObject::h_set(const std::string& field, const std::string& value) {
    if (this->type_ != Type::HASH) return "Redis object type error";
    auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value);
    map[field] = RedisString(value);
    return "OK";
}

std::string RedisObject::h_get(const std::string& field) {
    if (this->type_ != Type::HASH) return "Redis object type error";
    if (auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value); map.find(field) != map.end()) {
        return map[field].std_string();
    }
    return "(nil)";
}

std::string RedisObject::h_get_all() const {
    if (this->type_ != Type::HASH) return "Redis object type error";
    const auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value);
    std::string result;
    int count = 0;
    for (const auto&[fst, snd] : map) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        result += std::to_string(count) + ") " + fst + ": " + snd.std_string();
    }
    return result;
}

std::string RedisObject::h_keys() const {
    if (this->type_ != Type::HASH) return "Redis object type error";
    const auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value);
    std::string result;
    int count = 0;
    for (const auto&[fst, snd] : map) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        result += std::to_string(count) + ") " + fst;
    }
    return result;
}

std::string RedisObject::h_vals() const {
    if (this->type_ != Type::HASH) return "Redis object type error";
    const auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value);
    std::string result;
    int count = 0;
    for (const auto&[fst, snd] : map) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        result += std::to_string(count) + ") " + snd.std_string();
    }
    return result;
}

std::string RedisObject::h_set_n_x(const std::string& field, const std::string& value) {
    if (this->type_ != Type::HASH) return "Redis object type error";
    if (auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value); map.find(field) == map.end()) {
        map[field] = RedisString(value);
        return "OK";
    }
    return "(nil)";
}

std::string RedisObject::h_incr_by(const std::string& field, int increment) {
    if (this->type_ != Type::HASH) return "Redis object type error";
    if (auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value); map.find(field) != map.end()) {
        auto& rs = map[field];
        switch (rs.encoding()) {
            case RedisString::Encoding::STRING_INT:
                rs.update_num(increment);
                break;
            default:
                return "Hash value can not be recognized as an integer";
        }
        return rs.std_string();
    }
    return "(nil)";
}

std::string RedisObject::h_incr_by_float(const std::string& field, double increment) {
    if (this->type_ != Type::HASH) return "Redis object type error";
    if (auto& map = std::get<std::unordered_map<std::string, RedisString>>(this->value); map.find(field) != map.end()) {
        auto& rs = map[field];
        switch (rs.encoding()) {
            case RedisString::Encoding::STRING_INT:
            case RedisString::Encoding::STRING_DOUBLE:
                rs.update_num(increment);
                break;
            default:
                return "Hash value can not be recognized as a float number";
        }
        return rs.std_string();
    }
    return "(nil)";
}

// Set
std::string RedisObject::s_add(const std::string& member) {
    if (this->type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    set.emplace(member);
    return "OK";
}

std::string RedisObject::s_rem(const std::string& member) {
    if (this->type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    if (const auto it = set.find(member); it != set.end()) {
        set.erase(it);
        return "OK";
    }
    return "(nil)";
}

std::string RedisObject::s_card() const {
    if (this->type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    return std::to_string(set.size());
}

std::string RedisObject::s_is_member(const std::string& member) const {
    if (this->type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    return set.find(member) != set.end() ? "true" : "false";
}

std::string RedisObject::s_members() const {
    if (this->type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    std::string result;
    int count = 0;
    for (const auto& r : set) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        result += std::to_string(count) + ") " + r;
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}

std::string RedisObject::s_inter(const RedisObject& other) const {
    if (this->type_ != Type::SET) return "Redis object type error";
    if (other.type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    auto& set2 = std::get<std::unordered_set<std::string>>(other.value);
    std::string result;
    int count = 0;
    for (const auto& r : set) {
        if (set2.find(r) != set2.end()) {
            if (count > 0) {
                result += "\n";
            }
            count++;
            result += std::to_string(count) + ") " + r;
        }
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}

std::string RedisObject::s_diff(const RedisObject& other) const {
    if (this->type_ != Type::SET) return "Redis object type error";
    if (other.type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    auto& set2 = std::get<std::unordered_set<std::string>>(other.value);
    std::string result;
    int count = 0;
    for (const auto& r : set) {
        if (set2.find(r) == set2.end()) {
            if (count > 0) {
                result += "\n";
            }
            count++;
            result += std::to_string(count) + ") " + r;
        }
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}

std::string RedisObject::s_union(const RedisObject& other) const {
    if (this->type_ != Type::SET) return "Redis object type error";
    if (other.type_ != Type::SET) return "Redis object type error";
    auto& set = std::get<std::unordered_set<std::string>>(this->value);
    auto& set2 = std::get<std::unordered_set<std::string>>(other.value);
    std::string result;
    int count = 0;
    for (const auto& r : set) {
        if (set2.find(r) == set2.end()) {
            if (count > 0) {
                result += "\n";
            }
            count++;
            result += std::to_string(count) + ") " + r;
        }
    }
    for (const auto& r : set2) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        result += std::to_string(count) + ") " + r;
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}

// ZSet
std::string RedisObject::z_add(const double score, const std::string& member) {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    if (const auto it = scoreMap.find(member); it != scoreMap.end()) {
        sortedSet.erase(ZSetRecord{member, it->second});
    }
    if (const int temp = static_cast<int>(score); temp == score) {
        scoreMap[member] = temp;
        sortedSet.insert(ZSetRecord{member, temp});
    } else {
        scoreMap[member] = score;
        sortedSet.insert(ZSetRecord{member, score});
    }
    return "OK";
}

std::string RedisObject::z_rem(const std::string& member) {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    const auto it = scoreMap.find(member);
    if (it == scoreMap.end()) return "(nil)";
    sortedSet.erase(ZSetRecord{member, it->second});
    scoreMap.erase(it);
    return "OK";
}

const auto variant2string = [](const std::variant<int, double>& v) -> std::string {
    return std::to_string(std::holds_alternative<int>(v) ? std::get<int>(v) : std::get<double>(v));
};

std::string RedisObject::z_score(const std::string& member) const {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    const auto it = scoreMap.find(member);
    if (it == scoreMap.end()) return "(nil)";
    return variant2string(it->second);
}

std::string RedisObject::z_rank(const std::string& member, const bool with_score) const {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    const auto it = scoreMap.find(member);
    if (it == scoreMap.end()) return "(nil)";
    ZSetRecord rec{member, it->second};
    size_t rank = 0;
    // linear search to get rank
    for (const auto&[m, s] : sortedSet) {
        if (m == member) {
            if (with_score) {
                return std::to_string(rank) + " " + variant2string(s);
            }
            return std::to_string(rank);
        }
        ++rank;
    }
    return "Unexpected error";
}

std::string RedisObject::z_card() const {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    return std::to_string(scoreMap.size());
}

/*std::string RedisObject::z_count(double min, double max) const {
    ;
}

std::string RedisObject::z_incr_by(double increment, std::string& member) {
    ;
}

std::string RedisObject::z_range(int idx1, int idx2, bool with_scores) const {
    ;
}

std::string RedisObject::z_range_by_score(double min, double max, bool minus_inf, bool plus_inf, bool left_not_eq, bool right_not_eq, bool with_scores) const {
    ;
}*/

const auto add = [](const std::variant<int, double>& v1, const std::variant<int, double>& v2) -> std::string {
    if (std::holds_alternative<int>(v1)) {
        if (std::holds_alternative<int>(v2)) {
            return std::to_string(std::get<int>(v1) + std::get<int>(v2));
        }
    }
    const double sum = (std::holds_alternative<int>(v1) ? std::get<int>(v1) : std::get<double>(v1)) +
        (std::holds_alternative<int>(v2) ? std::get<int>(v2) : std::get<double>(v2));
    if (const int temp = static_cast<int>(sum); temp == sum) {
        return std::to_string(temp);
    }
    return std::to_string(sum);
};

std::string RedisObject::z_inter(const RedisObject& other) const {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    if (other.type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    auto&[sortedSet2, scoreMap2] = std::get<ZSet>(other.value);
    std::string result;
    int count = 0;
    for (const auto&[k, v] : scoreMap) {
        if (auto it = scoreMap2.find(k); it != scoreMap2.end()) {
            if (count > 0) {
                result += "\n";
            }
            count++;
            result += std::to_string(count) + ") " + k + " " + add(v, it->second);
        }
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}

std::string RedisObject::z_union(const RedisObject& other) const {
    if (this->type_ != Type::ZSET) return "Redis object type error";
    if (other.type_ != Type::ZSET) return "Redis object type error";
    auto&[sortedSet, scoreMap] = std::get<ZSet>(this->value);
    auto&[sortedSet2, scoreMap2] = std::get<ZSet>(other.value);
    std::string result;
    int count = 0;
    for (const auto&[k, v] : scoreMap) {
        if (count > 0) {
            result += "\n";
        }
        count++;
        if (auto it = scoreMap2.find(k); it != scoreMap2.end()) {
            result += std::to_string(count) + ") " + k + " " + add(v, it->second);
        } else {
            result += std::to_string(count) + ") " + k + " " + variant2string(v);
        }
    }
    if (count == 0) {
        return "(empty array)";
    }
    return result;
}
