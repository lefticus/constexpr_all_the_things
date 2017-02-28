#pragma once

#include <cx_algorithm.h>
#include <cx_parser.h>
#include <cx_string.h>

#include <functional>
#include <string_view>
#include <variant>

namespace JSON
{
  using namespace cx::parser;

  //----------------------------------------------------------------------------
  // JSON value parsers

  // parse a JSON boolean value

  constexpr auto bool_parser()
  {
    using namespace std::literals;
    return fmap([] (std::string_view) { return true; },
                make_string_parser("true"sv))
      | fmap([] (std::string_view) { return false; },
             make_string_parser("false"sv));
  }

  // parse a JSON null value

  constexpr auto null_parser()
  {
    using namespace std::literals;
    return fmap([] (std::string_view) { return std::monostate{}; },
                make_string_parser("null"sv));
  }

  // parse a JSON number

  constexpr auto number_parser()
  {
    constexpr auto neg_parser = option('+', make_char_parser('-'));
    constexpr auto integral_parser =
      combine(neg_parser,
              fmap([] (char) { return 0; }, make_char_parser('0')) | int1_parser(),
              [] (char sign, int i) { return sign == '+' ? i : -i; });

    constexpr auto frac_parser = make_char_parser('.') < int0_parser();

    constexpr auto mantissa_parser = combine(
        integral_parser, option(0, frac_parser),
        [] (int i, int f) -> double {
          double d = 0;
          while (f > 0) {
            d += f % 10;
            d /= 10;
            f /= 10;
          }
          return i + d;
        });

    constexpr auto e_parser = make_char_parser('e') | make_char_parser('E');
    constexpr auto sign_parser = make_char_parser('+') | neg_parser;
    constexpr auto exponent_parser =
      bind(e_parser < sign_parser,
           [] (const char sign, const auto& sv) {
             return fmap([sign] (int j) { return sign == '+' ? j : -j; },
                         int0_parser())(sv);
           });

    return combine(
        mantissa_parser, option(0, exponent_parser),
        [] (double mantissa, int exp) {
          if (exp > 0) {
            while (exp--) {
              mantissa *= 10;
            }
          } else {
            while (exp++) {
              mantissa /= 10;
            }
          }
          return mantissa;
        });
  }

  // parse a JSON string char

  // When parsing a JSON string, in general multiple chars in the input may
  // result in different chars in the output (for example, an escaped char, or a
  // unicode code point) - which means we can't just return part of the input as
  // a sub-string_view: we need to actually build a string. So we will use
  // cx::string<4> as the return type of the char parsers to allow for the max
  // utf-8 conversion (note that all char parsers return the same thing so that
  // we can use the alternation combinator), and we will build up a cx::string<>
  // as the return type of the string parser.

  // if a char is escaped, simply convert it to the appropriate thing
  constexpr auto convert_escaped_char(char c)
  {
    switch (c) {
      case 'b': return '\b';
      case 'f': return '\f';
      case 'n': return '\n';
      case 'r': return '\r';
      case 't': return '\t';
      default: return c;
    }
  }

  // convert a unicode code point to utf-8
  constexpr auto to_utf8(uint32_t hexcode)
  {
    cx::string<4> s;
    if (hexcode <= 0x7f) {
      s.push_back(static_cast<char>(hexcode));
    } else if (hexcode <= 0x7ff) {
      s.push_back(static_cast<char>(0xC0 | (hexcode >> 6)));
      s.push_back(static_cast<char>(0x80 | (hexcode & 0x3f)));
    } else if (hexcode <= 0xffff) {
      s.push_back(static_cast<char>(0xE0 | (hexcode >> 12)));
      s.push_back(static_cast<char>(0x80 | ((hexcode >> 6) & 0x3f)));
      s.push_back(static_cast<char>(0x80 | (hexcode & 0x3f)));
    } else if (hexcode <= 0x10ffff) {
      s.push_back(static_cast<char>(0xF0 | (hexcode >> 18)));
      s.push_back(static_cast<char>(0x80 | ((hexcode >> 12) & 0x3f)));
      s.push_back(static_cast<char>(0x80 | ((hexcode >> 6) & 0x3f)));
      s.push_back(static_cast<char>(0x80 | (hexcode & 0x3f)));
    }
    return s;
  }

  constexpr auto to_hex(char c)
  {
    if (c >= '0' && c <= '9') return static_cast<uint16_t>(c - '0');
    if (c >= 'a' && c <= 'f') return static_cast<uint16_t>(c - 'a' + 10);
    return static_cast<uint16_t>(c - 'A' + 10);
  }

  constexpr auto unicode_point_parser()
  {
    using namespace std::literals;
    constexpr auto p =
      make_char_parser('\\') <
      make_char_parser('u') <
      exactly_n(
          one_of("0123456789abcdefABCDEF"sv),
          4, 0u,
          [] (uint16_t hexcode, char c) -> uint16_t {
            return (hexcode << 4) + to_hex(c);
          });
    return fmap(to_utf8, p);
  }

  constexpr auto string_char_parser()
  {
    using namespace std::literals;
    constexpr auto slash_parser = make_char_parser('\\');
    constexpr auto special_char_parser =
      make_char_parser('"')
      | make_char_parser('\\')
      | make_char_parser('/')
      | make_char_parser('b')
      | make_char_parser('f')
      | make_char_parser('n')
      | make_char_parser('r')
      | make_char_parser('t');
    constexpr auto escaped_char_parser = fmap(
        convert_escaped_char, slash_parser < special_char_parser);
    constexpr auto p = escaped_char_parser | none_of("\\\""sv);

    return fmap([] (auto c) {
      cx::string<4> s;
      s.push_back(c);
      return s;
    }, p) | unicode_point_parser();
  }

  // parse a JSON string

  // See the comment about the char parsers above. Here we accumulate a
  // cx::string<> (which is arbitrarily sized at 32).

  constexpr auto string_parser()
  {
    constexpr auto quote_parser = make_char_parser('"');
    const auto str_parser =
      many(string_char_parser(),
           cx::string<>{},
           [] (auto acc, const auto& str) {
             cx::copy(str.cbegin(), str.cend(), cx::back_insert_iterator(acc));
             return acc;
           });
    return quote_parser < str_parser > quote_parser;
  }

  // parse an array sum
  // (at the moment this is just a proof-of-concept for separated_by)
  constexpr auto parse_array_sum(parse_input_t s)
  {
    constexpr auto array_parser =
      make_char_parser('[') <
      separated_by(int1_parser(), make_char_parser(','), std::plus<>{})
      > make_char_parser(']');
    return array_parser(s);
  }

}
