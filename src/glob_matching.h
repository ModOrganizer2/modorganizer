/* Copyright (C) 2020 G'k
 * Imported by Holt59
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */
#ifndef GLOB_MATCHING_H
#define GLOB_MATCHING_H

#include <QString>
#include <cctype>
#include <string_view>

namespace MOShared
{

/**
 * Contraints string_traits to allow usage of both standard strings and QString in
 * GlobPattern.
 */
namespace details
{

  template <class CharT, class Traits = std::char_traits<CharT>,
            class Allocator = std::allocator<CharT>>
  struct string_traits
  {
    using string_type = std::basic_string<CharT, Traits, Allocator>;
    using string_view = std::basic_string_view<CharT, Traits>;

    static auto tolower(CharT c) { return std::tolower(c); }

    static auto empty(string_view const& view) { return view.empty(); }
  };

  template <>
  struct string_traits<QChar>
  {
    using string_type = QString;
    using string_view = QString;

    static auto tolower(QChar const& c) { return c.toLower(); }
    static auto empty(string_view const& view) { return view.isEmpty(); }
  };

}  // namespace details

/**
 * @brief Class that provides basic wildcard pattern matching.
 *
 * From https://gitlab.com/G_ka/playground/-/commits/master/include/wildcards.hpp
 *
 * Currently, this supports the following globbing character:
 *  - '*' matches zero or more characters.
 *  - '?' matches exactly one character.
 *  - '[abc]' matches one of 'a', 'b' or 'c'.
 *
 * Standard globbing feature not supported:
 *  - You cannot escape globbing characters with \.
 *  - You cannot use `[a-z]` to match any character from 'a' to 'z'.
 *
 * Custom class because the following alternatives have some issues:
 *  - QRegExp is a tad slow, and we need to convert everything to QString.
 *  - QDir::match is VERY slow. I think it converts the glob pattern to a
 * QRegularExpression and then use it.
 *  - PatchMatchSpecW (Windows API) is fast but does not support some useful glob
 * pattern (e.g., [ab]).
 *
 * Advantage of this over the above methods:
 *  - It is fast. Quick testing show that this is faster than PatchMatchSpecW.
 *  - It can be used on most string types (QString, std::string, std::wstring, etc.).
 */
template <class CharT, class Traits = std::char_traits<CharT>,
          class Allocator = std::allocator<CharT>>
class GlobPattern
{
public:
  using traits = details::string_traits<CharT, Traits, Allocator>;

  using string_type      = typename traits::string_type;
  using string_view_type = typename traits::string_view;

  struct card
  {
    // Relying on automatic conversion:
    static constexpr CharT any        = '?';
    static constexpr CharT any_repeat = '*';
    static constexpr CharT set_begin  = '[';
    static constexpr CharT set_end    = ']';
  };

public:
  GlobPattern(string_view_type const& s) : v{s} {}

  const string_type& native() const { return v; }

  constexpr bool match(string_view_type const& str, bool case_sensitive = false)
  {
    // Empty pattern can only match with empty sting
    if (traits::empty(v))
      return traits::empty(str);

    auto pat_it  = v.begin();
    auto pat_end = v.end();

    auto str_it  = str.begin();
    auto str_end = str.end();

    auto anyrep_pos_pat = pat_end;
    auto anyrep_pos_str = str_end;

    auto set_pos_pat = pat_end;

    while (str_it != str_end) {
      CharT current_pat = QChar(0);
      CharT current_str = QChar(-1);
      if (pat_it != pat_end) {
        current_pat = case_sensitive ? *pat_it : traits::tolower(*pat_it);
        current_str = case_sensitive ? *str_it : traits::tolower(*str_it);
      }
      if (pat_it != pat_end && current_pat == card::set_begin) {
        set_pos_pat = pat_it;
        pat_it++;
      } else if (pat_it != pat_end && current_pat == card::set_end) {
        if (anyrep_pos_pat != pat_end) {
          set_pos_pat = pat_end;
          pat_it++;
        } else {
          return false;
        }

      } else if (set_pos_pat != pat_end) {
        if (current_pat == current_str) {
          set_pos_pat = pat_end;
          pat_it      = std::find(pat_it, pat_end, card::set_end) + 1;
          str_it++;
        } else {
          if (pat_it == pat_end) {
            return false;
          }
          pat_it++;
        }
      } else if (pat_it != pat_end && current_pat == current_str) {
        pat_it++;
        str_it++;
      } else if (pat_it != pat_end && current_pat == card::any) {
        pat_it++;
        str_it++;
      } else if (pat_it != pat_end && current_pat == card::any_repeat) {
        anyrep_pos_pat = pat_it;
        anyrep_pos_str = str_it;
        pat_it++;
      } else if (anyrep_pos_pat != pat_end) {
        pat_it = anyrep_pos_pat + 1;
        str_it = anyrep_pos_str + 1;
        anyrep_pos_str++;
      } else {
        return false;
      }
    }
    while (pat_it != pat_end) {
      CharT cur = case_sensitive ? *pat_it : traits::tolower(*pat_it);
      if (cur == card::any_repeat)
        pat_it++;
      else
        break;
    }
    return pat_it == pat_end;
  }

private:
  string_type v;
};

template <class CharT, class Traits, class Allocator>
GlobPattern(std::basic_string<CharT, Traits, Allocator> const&)
    -> GlobPattern<CharT, Traits, Allocator>;

template <class CharT>
GlobPattern(CharT const*) -> GlobPattern<CharT>;

GlobPattern(QString const&) -> GlobPattern<QChar>;

}  // namespace MOShared

#endif
