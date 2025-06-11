#pragma once
#include <utility>
#include <vector>
#include <random>
#include <string>
#include <limits>

struct SkipListNode {
    std::string member;
    double score;
    std::vector<SkipListNode*> forward;
    std::vector<int> span; // current node -> forward[i] node (including forward[i] node)

    SkipListNode(const int level, std::string  m, const double s)
         : member(std::move(m)), score(s), forward(level, nullptr), span(level, 0) {}
};

class SkipList {
    static constexpr int MAX_LEVEL = 16;
    static constexpr double P = 0.5;

    SkipListNode* head;
    int level;                             // Current max level of the skip list (level is 1-based)
    std::mt19937 rng;                      // Mersenne Twister random number generator (high-quality RNG)
    std::uniform_real_distribution<> dist; // Uniform distribution in range [0.0, 1.0)

public:
    SkipList() : level(1), rng(std::random_device{}()), dist(0.0, 1.0) {
        head = new SkipListNode(MAX_LEVEL, "", std::numeric_limits<double>::lowest());
        for (int i = 0; i < MAX_LEVEL; ++i) {
            head->span[i] = 0;
        }
    }

    ~SkipList() {
        const SkipListNode* node = head;
        while (node) {
            const SkipListNode* next = node->forward[0];
            delete node;
            node = next;
        }
    }

    int randomLevel() {
        int lvl = 1;
        while (dist(rng) < P && lvl < MAX_LEVEL) ++lvl;
        return lvl;
    }

    void insert(const std::string& member, const double score) {
        std::vector<SkipListNode*> update(MAX_LEVEL, nullptr); // nodes prior to the new node
        std::vector rank(MAX_LEVEL, 0); // span from head (of nodes prior to the new node)
        SkipListNode* x = head;

        // Search for insertion position from top level to bottom
        for (int i = level - 1; i >= 0; --i) {
            rank[i] = (i == level - 1 ? 0 : rank[i + 1]);
            while (x->forward[i] &&
                   (x->forward[i]->score < score ||
                   (x->forward[i]->score == score && x->forward[i]->member < member))) {
                rank[i] += x->span[i];
                x = x->forward[i];
            }
            update[i] = x;
        }

        x = x->forward[0];
        if (x && x->score == score && x->member == member) return;  // Already exists

        const int newLevel = randomLevel();
        if (newLevel > level) {
            for (int i = level; i < newLevel; ++i) {
                update[i] = head;
                rank[i] = 0;
                head->span[i] = rank[0];
            }
            level = newLevel;
        }

        x = new SkipListNode(newLevel, member, score);
        for (int i = 0; i < newLevel; ++i) {
            x->forward[i] = update[i]->forward[i];
            x->span[i] = update[i]->span[i] - (rank[0] - rank[i]);
            update[i]->forward[i] = x;
            update[i]->span[i] = (rank[0] - rank[i]) + 1;
        }

        // if newLevel is less than level
        for (int i = newLevel; i < level; ++i) {
            update[i]->span[i]++;
        }
    }

    bool erase(const std::string& member, const double score) {
        std::vector<SkipListNode*> update(MAX_LEVEL, nullptr);
        SkipListNode* x = head;
        for (int i = level - 1; i >= 0; --i) {
            while (x->forward[i] &&
                   (x->forward[i]->score < score ||
                   (x->forward[i]->score == score && x->forward[i]->member < member))) {
                x = x->forward[i];
            }
            update[i] = x;
        }

        x = x->forward[0];
        if (!x || x->score != score || x->member != member) return false;

        for (int i = 0; i < level; ++i) {
            if (update[i]->forward[i] == x) {
                update[i]->span[i] += x->span[i] - 1;
                update[i]->forward[i] = x->forward[i];
            } else {
                update[i]->span[i]--;
            }
        }
        delete x;

        // Reduce level if the highest levels are empty
        while (level > 1 && head->forward[level - 1] == nullptr) {
            --level;
        }
        return true;
    }

    // rank is 0-based and head is excluded from ranking
    int rank(const std::string& member, const double score) const {
        int rank = 0;
        const SkipListNode *x = head;
        for (int i = level - 1; i >= 0; --i) {
            while (x->forward[i] &&
                   (x->forward[i]->score < score ||
                    (x->forward[i]->score == score && x->forward[i]->member < member))) {
                rank += x->span[i];
                x = x->forward[i];
            }
        }
        x = x->forward[0];
        if (x && x->score == score && x->member == member) return rank;
        return -1;
    }

    // Return list of members in range [start, end] (inclusive, 0-based)
    std::vector<std::string> range(const int start, const int end) const {
        std::vector<std::string> result;
        if (start > end || start < 0) return result;

        int rank = 0;
        const SkipListNode* x = head;

        for (int i = level - 1; i >= 0; --i) {
            while (x->forward[i] && (rank + x->span[i]) <= start) {
                rank += x->span[i];
                x = x->forward[i];
            }
        }

        x = x->forward[0];
        int count = end - start + 1;

        while (x && count-- > 0) {
            result.push_back(x->member);
            x = x->forward[0];
        }

        return result;
    }

    std::vector<std::string> rangeByScore(const double min, const bool minExclusive, const double max, const bool maxExclusive) const {
        std::vector<std::string> result;
        const SkipListNode* x = head;

        for (int i = level - 1; i >= 0; --i) {
            while (x->forward[i] &&
                   (x->forward[i]->score < min ||
                    (minExclusive && x->forward[i]->score == min))) {
                x = x->forward[i];
            }
        }

        x = x->forward[0]; // first candidate

        while (x) {
            if (x->score > max || (maxExclusive && x->score == max)) break;
            result.push_back(x->member);
            x = x->forward[0];
        }

        return result;
    }

};
