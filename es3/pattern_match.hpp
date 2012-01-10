// pattern_match.hpp: pattern matching for bjam

// Copyright Takeshi Mouri 2007.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

// See http://hamigaki.sourceforge.jp/ for library home page.

#ifndef IMPL_PATTERN_MATCH_HPP
#define IMPL_PATTERN_MATCH_HPP

#include <algorithm>
#include <bitset>
#include <functional>
#include <stdexcept>
#include <string>

inline std::bitset<256>
make_char_class(
    std::string::const_iterator ptn_beg,
    std::string::const_iterator ptn_end)
{
    std::bitset<256> res;

    bool flip = false;
    if (*ptn_beg == '^')
    {
        flip = true;
        ++ptn_beg;
    }

    while (ptn_beg != ptn_end)
    {
        char c1 = *(ptn_beg++);
        if ((ptn_beg != ptn_end) && (*ptn_beg == '-'))
        {
            if (++ptn_beg == ptn_end)
                throw std::runtime_error("character class not end");

            std::size_t start = static_cast<unsigned char>(c1);
            std::size_t last = static_cast<unsigned char>(*(ptn_beg++));

            for (std::size_t i = start; i != last; ++i)
                res.set(i);
        }
        else
            res.set(static_cast<unsigned char>(c1));
    }

    if (flip)
        res.flip();

    return res;
}

inline bool pattern_match_impl(
    std::string::const_iterator ptn_beg, std::string::const_iterator ptn_end,
    std::string::const_iterator str_beg, std::string::const_iterator str_end)
{
    typedef std::string::const_iterator iter_type;

    while (ptn_beg != ptn_end)
    {
        char ptn = *(ptn_beg++);
        if (ptn == '*')
        {
            iter_type next = str_end;
            while (next != str_beg)
            {
                if (pattern_match_impl(ptn_beg, ptn_end, next, str_end))
                    return true;
                --next;
            }
        }
        else
        {
            if (str_beg == str_end)
                return false;

            if (ptn == '[')
            {
                if (ptn_beg == ptn_end)
                    throw std::runtime_error("character class not end");

                iter_type next = ptn_beg;
                if (*next == ']')
                    ++next;

                next = std::find(next, ptn_end, ']');
                if (next == ptn_end)
                    throw std::runtime_error("character class not end");

                const std::bitset<256>& mask = ::make_char_class(ptn_beg, next);
                if (!mask.test(static_cast<unsigned char>(*str_beg)))
                    return false;

                ptn_beg = ++next;
            }
            else if (ptn == '\\')
            {
                if (ptn_beg == ptn_end)
                    throw std::runtime_error("escape sequence not end");

                ptn = *(ptn_beg++);
                if (ptn != *str_beg)
                    return false;
            }
            else if ((ptn != '?') && (ptn != *str_beg))
                return false;

            ++str_beg;
        }
    }
    return str_beg == str_end;
}

struct pattern_match
    : public std::unary_function<std::string,bool>
{
public:
    pattern_match()
    {
    }

    explicit pattern_match(const std::string& ptn) : pattern_(ptn)
    {
    }

    bool operator()(const std::string& s) const
    {
        return ::pattern_match_impl(
            pattern_.begin(), pattern_.end(), s.begin(), s.end()
        );
    }

private:
    std::string pattern_;
};

#endif // IMPL_PATTERN_MATCH_HPP
