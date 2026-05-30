#include <array>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <vector>

#include "benchmark_data.h"


// -- AhoCorasick (hashmap-transition version) ----------------------------------

class AhoCorasickHashMap
{
public:
    struct State
    {
        std::unordered_map<char, int> next;
        int fail = 0;
        bool is_end_anchored_match = false; // e.g. ".so"  → must match at end
        bool is_mid_match = false;          // e.g. ".so." → match anywhere
    };

    std::vector<State> trie;

    AhoCorasickHashMap()
    {
        trie.emplace_back(); // root state
    }

    void insertEndAnchored(const std::string &pattern)
    {
        int cur = buildPath(pattern);
        trie[cur].is_end_anchored_match = true;
    }

    void insertMid(const std::string &pattern)
    {
        int cur = buildPath(pattern);
        trie[cur].is_mid_match = true;
    }

    void build()
    {
        std::queue<int> q;

        for (auto &[c, s] : trie[0].next)
        {
            trie[s].fail = 0;
            q.push(s);
        }

        while (!q.empty())
        {
            int cur = q.front();
            q.pop();

            for (auto &[c, next_state] : trie[cur].next)
            {
                int fail = trie[cur].fail;

                while (fail != 0 && trie[fail].next.find(c) == trie[fail].next.end())
                    fail = trie[fail].fail;

                auto it = trie[fail].next.find(c);
                int fs = (it != trie[fail].next.end() && it->second != next_state) ? it->second : 0;
                trie[next_state].fail = fs;

                // Propagate both match flags through suffix links
                trie[next_state].is_end_anchored_match |= trie[fs].is_end_anchored_match;
                trie[next_state].is_mid_match |= trie[fs].is_mid_match;

                q.push(next_state);
            }
        }
    }

    // Returns true if:
    //   - any mid pattern is found ANYWHERE in text, OR
    //   - any end-anchored pattern lands exactly at the last character
    bool matches(const std::string &text) const
    {
        int cur = 0;
        int last = text.size() - 1;

        for (int i = 0; i <= last; ++i)
        {
            char c = text[i];

            while (cur != 0 && trie[cur].next.find(c) == trie[cur].next.end())
                cur = trie[cur].fail;

            auto it = trie[cur].next.find(c);
            cur = (it != trie[cur].next.end()) ? it->second : 0;

            // Mid patterns: match at any position
            if (trie[cur].is_mid_match)
                return true;

            // End-anchored patterns: only match if we're at the last character
            if (i == last && trie[cur].is_end_anchored_match)
                return true;
        }
        return false;
    }

private:
    int buildPath(const std::string &pattern)
    {
        int cur = 0;
        for (char c : pattern)
        {
            auto it = trie[cur].next.find(c);
            if (it == trie[cur].next.end())
            {
                trie[cur].next[c] = trie.size();
                cur = trie.size();
                trie.emplace_back();
            }
            else
            {
                cur = it->second;
            }
        }
        return cur;
    }
};

// Build once, reuse for many filenames
AhoCorasickHashMap buildMatcherHashMap()
{
    AhoCorasickHashMap ac;
    for (const auto &ext : kExtensions)
    {
        ac.insertEndAnchored(ext); // ".so", ".a", ".o"  — end-anchored
        ac.insertMid(ext + ".");   // ".so.", ".a.", ".o." — mid-string
    }
    ac.build();
    return ac;
}

// -- AhoCorasick (array-transition version) ----------------------------------

class AhoCorasickArray
{
public:
    struct State
    {
        std::array<int, 256> next{};
        int fail = 0;
        bool is_end_anchored_match = false;
        bool is_mid_match = false;
        State()
        {
            next.fill(-1);
        }
    };

    std::vector<State> trie;

    AhoCorasickArray()
    {
        trie.emplace_back();
    }

    void insertEndAnchored(const std::string &p)
    {
        trie[buildPath(p)].is_end_anchored_match = true;
    }
    void insertMid(const std::string &p)
    {
        trie[buildPath(p)].is_mid_match = true;
    }

    void build()
    {
        std::queue<int> q;
        for (int c = 0; c < 256; ++c)
        {
            int s = trie[0].next[c];
            if (s == -1)
            {
                trie[0].next[c] = 0;
                continue;
            }
            trie[s].fail = 0;
            q.push(s);
        }
        while (!q.empty())
        {
            int cur = q.front();
            q.pop();
            for (int c = 0; c < 256; ++c)
            {
                int ns = trie[cur].next[c];
                if (ns == -1)
                {
                    trie[cur].next[c] = trie[trie[cur].fail].next[c];
                    continue;
                }
                trie[ns].fail = trie[trie[cur].fail].next[c];
                int fs = trie[ns].fail;
                trie[ns].is_end_anchored_match |= trie[fs].is_end_anchored_match;
                trie[ns].is_mid_match |= trie[fs].is_mid_match;
                q.push(ns);
            }
        }
    }

    bool matches(const std::string &text) const
    {
        int cur = 0;
        int last = static_cast<int>(text.size()) - 1;
        for (int i = 0; i <= last; ++i)
        {
            cur = trie[cur].next[static_cast<unsigned char>(text[i])];
            if (trie[cur].is_mid_match)
            {
                return true;
            }

            if (i == last && trie[cur].is_end_anchored_match)
            {
                return true;
            }
        }
        return false;
    }

private:
    int buildPath(const std::string &p)
    {
        int cur = 0;
        for (char c : p)
        {
            unsigned char uc = c;
            if (trie[cur].next[uc] == -1)
            {
                trie[cur].next[uc] = trie.size();
                cur = trie.size();
                trie.emplace_back();
            }
            else
            {
                cur = trie[cur].next[uc];
            }
        }
        return cur;
    }
};

AhoCorasickArray buildMatcherArray()
{
    AhoCorasickArray ac;
    for (const auto &ext : kExtensions)
    {
        ac.insertEndAnchored(ext);
        ac.insertMid(ext + ".");
    }
    ac.build();
    return ac;
}

// -- AhoCorasick (array-transition with "reverse iteration" version) ----------------------------------

class ReverseAhoCorasickArray
{
public:
    struct State
    {
        std::array<int, 256> next;
        int fail = 0;
        bool is_match = false; // reversed pattern ends here
        State()
        {
            next.fill(-1);
        }
    };

    std::vector<State> trie;

    ReverseAhoCorasickArray()
    {
        trie.emplace_back();
    }

    // Insert the extension reversed, e.g. ".so" is stored as "os."
    void insert(const std::string &pattern)
    {
        int cur = 0;
        for (auto it = pattern.rbegin(); it != pattern.rend(); ++it)
        {
            unsigned char c = *it;
            if (trie[cur].next[c] == -1)
            {
                trie[cur].next[c] = trie.size();
                cur = trie.size();
                trie.emplace_back();
            }
            else
            {
                cur = trie[cur].next[c];
            }
        }
        trie[cur].is_match = true;
    }

    void build()
    {
        std::queue<int> q;
        for (int c = 0; c < 256; ++c)
        {
            int s = trie[0].next[c];
            if (s == -1)
            {
                trie[0].next[c] = 0;
                continue;
            }
            trie[s].fail = 0;
            q.push(s);
        }
        while (!q.empty())
        {
            int cur = q.front();
            q.pop();
            for (int c = 0; c < 256; ++c)
            {
                int ns = trie[cur].next[c];
                if (ns == -1)
                {
                    trie[cur].next[c] = trie[trie[cur].fail].next[c];
                    continue;
                }
                trie[ns].fail = trie[trie[cur].fail].next[c];
                // Propagate match flag through suffix links
                trie[ns].is_match |= trie[trie[ns].fail].is_match;
                q.push(ns);
            }
        }
    }

    // Core matcher: scan filename right-to-left
    //
    // Phase 1 — Pre-scan: skip version suffix [0-9.]  e.g. ".6.1" in "libc.so.6.1"
    //           Stop as soon as a non-digit, non-dot character is seen.
    //           This is where the extension candidate ends (right side).
    //
    // Phase 2 — AC run: feed characters into the reversed automaton.
    //           The moment we hit a match state we know a full extension was
    //           consumed — return true immediately (early exit).
    //           Stop at '/' or start-of-string (no extension can cross a separator).
    bool matches(const std::string &filename) const
    {
        if (filename.empty())
            return false;

        int i = static_cast<int>(filename.size()) - 1;

        // -- Phase 1: skip trailing version tokens [digits and '.'] ----------
        // e.g. "libc.so.6.1"  →  skip "6.1", stop before the '.' of ".so."
        // e.g. "libfoo.so"    →  nothing to skip, we start right at 'o'
        while (i >= 0 && (std::isdigit((unsigned char)filename[i]) || filename[i] == '.'))
            --i;

        // If the whole string was digits/dots, it's not a library filename
        if (i < 0)
            return false;

        // -- Phase 2: run reversed Aho-Corasick left-ward --------------------
        int state = 0;
        while (i >= 0)
        {
            unsigned char c = filename[i];

            // A path separator means we've left the filename component — stop
            if (c == '/')
                break;

            state = trie[state].next[c];

            if (trie[state].is_match)
                return true; // ← early exit

            --i;
        }
        return false;
    }
};

ReverseAhoCorasickArray buildMatcherArrayReverse()
{
    ReverseAhoCorasickArray ac;
    for (const auto &ext : kExtensions)
    {
        ac.insert(ext);
    }
    ac.build();
    return ac;
}
