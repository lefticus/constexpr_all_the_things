#pragma once

#include <cx_algorithm.h>
#include <cx_json_value.h>
#include <cx_parser.h>
#include <cx_string.h>

#include <functional>
#include <string_view>
#include <type_traits>
#include <variant>

namespace JSON
{
  using namespace cx;
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
    cx::basic_string<char, 4> s;
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
      cx::basic_string<char, 4> s;
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
    constexpr auto str_parser =
      many(string_char_parser(),
           cx::string{},
           [] (auto acc, const auto& str) {
             cx::copy(str.cbegin(), str.cend(), cx::back_insert_iterator(acc));
             return acc;
           });
    return quote_parser < str_parser > quote_parser;
  }

  constexpr inline std::size_t max_parse_depth{3};


  //----------------------------------------------------------------------------
  // JSON number-of-objects-required parser
  // An array is 1 + number of objects in the array
  // An object is 1 + number of objects in the object
  // Anything else is just 1

  template <std::size_t = 0>
  struct numobjects_recur
  {
    // parse a JSON value

    static constexpr auto value_parser()
    {
      using namespace std::literals;
      return [] (const auto& sv) {
               // deduce the return type of this lambda
               if (false) return fail(std::size_t{})(sv);
               constexpr auto p =
                 fmap([] (auto) -> std::size_t { return 1; },
                      make_string_parser("true"sv) | make_string_parser("false"sv)
                      | make_string_parser("null"sv))
                 | fmap([] (auto) -> std::size_t { return 1; },
                        number_parser())
                 | fmap([] (auto) -> std::size_t { return 1; },
                        string_parser())
                 | array_parser()
                 | object_parser();
               return (skip_whitespace() < p)(sv);
             };
    }

    // parse a JSON array

    static constexpr auto array_parser()
    {
      return make_char_parser('[') <
        separated_by(value_parser(),
                     skip_whitespace() < make_char_parser(','),
                     [] () { return std::size_t{1}; }, std::plus<>{})
        > skip_whitespace()
        > (make_char_parser(']') | fail(']', [] { throw "expected ]"; }));
    }

    // parse a JSON object

    static constexpr auto key_value_parser()
    {
      return skip_whitespace()
        < (string_parser()
           | fail(cx::string{}, [] { throw "expected a string as object key"; }))
        < skip_whitespace()
        < (make_char_parser(':')
           | fail(':', [] { throw "expected a colon as object key-value separator"; }))
        < value_parser();
    }

    static constexpr auto object_parser()
    {
      return make_char_parser('{') <
        separated_by(key_value_parser(),
                     skip_whitespace() < make_char_parser(','),
                     [] () { return std::size_t{1}; }, std::plus<>{})
        > skip_whitespace()
        > (make_char_parser('}') | fail('}', [] { throw "expected }"; }));
    }

  };

  // provide the num objects parser outside the struct qualification
  constexpr auto numobjects_parser = numobjects_recur<>::value_parser;

  template <char... Cs>
  constexpr auto numobjects()
  {
    std::initializer_list<char> il{Cs...};
    return numobjects_parser()(std::string_view(il.begin(), il.size()))->first;
  }

  namespace literals
  {
    template <typename T, T... Ts>
    constexpr auto operator "" _json_size()
    {
      return numobjects<Ts...>();
    }
  }

  //----------------------------------------------------------------------------
  // alternative JSON parser

  // parse into a vector
  // return the index into the vector resulting from the parsed value

  template <std::size_t N>
  struct value_recur
  {
    using V = cx::vector<value, N>;

    // parse a JSON value

    static constexpr auto value_parser(V& v)
    {
      using namespace std::literals;
      return [&] (const auto& sv) {
               // deduce the return type of this lambda
               if (false) return fail(std::size_t{})(sv);
               const auto p =
                 fmap([&] (auto) { v.push_back(value(true)); return v.size()-1; },
                      make_string_parser("true"sv))
                 | fmap([&] (auto) { v.push_back(value(false)); return v.size()-1; },
                      make_string_parser("false"sv))
                 | fmap([&] (auto) { v.push_back(value(std::monostate{})); return v.size()-1; },
                        make_string_parser("null"sv))
                 | fmap([&] (double d) { v.push_back(value(d)); return v.size()-1; },
                        number_parser())
                 | fmap([&] (const cx::string& s) { v.push_back(value(s)); return v.size()-1; },
                        string_parser())
                 | (make_char_parser('[') < array_parser(v))
                 | (make_char_parser('{') < object_parser(v));
               return (skip_whitespace() < p)(sv);
             };
    }

    // parse a JSON array

    static constexpr auto array_parser(V& v)
    {
      return [&] (const auto& sv) {
               value val{};
               val.to_Array();
               v.push_back(std::move(val));
               const auto p = separated_by_val(
                   value_parser(v), skip_whitespace() < make_char_parser(','),
                   v.size()-1,
                   [&] (std::size_t arr_idx, std::size_t element_idx) {
                     v[arr_idx].to_Array().push_back(element_idx - arr_idx);
                     return arr_idx;
                   }) > skip_whitespace() > make_char_parser(']');
               return p(sv);
             };
    }

    // parse a JSON object

    static constexpr auto key_value_parser(V& v)
    {
      constexpr auto p =
        skip_whitespace() < string_parser() > skip_whitespace() > make_char_parser(':');
      return bind(p,
                  [&] (const cx::string& str, const auto& sv) {
                    return fmap([str] (std::size_t idx) { return cx::make_pair(str, idx); },
                                value_parser(v))(sv);
                  });
    }

    static constexpr auto object_parser(V& v)
    {
      return [&] (const auto& sv) {
               value val{};
               val.to_Object();
               v.push_back(std::move(val));
               const auto p = separated_by_val(
                   key_value_parser(v), skip_whitespace() < make_char_parser(','),
                   v.size()-1,
                   [&] (std::size_t obj_idx, const auto& kv) {
                     v[obj_idx].to_Object()[kv.first] = kv.second - obj_idx;
                     return obj_idx;
                   }) > skip_whitespace() > make_char_parser('}');
               return p(sv);
             };
    }

  };

  // a value_wrapper wraps a parsed JSON::value, and provides a proxy
  // pass-through interface to the value
  template <size_t N>
  struct value_wrapper
  {
    constexpr value_wrapper(parse_input_t s)
    {
      value_recur<N>::value_parser(storage)(s);
    }

    constexpr const value& operator[](const cx::string& s) const { return storage[0][s]; }
    constexpr const value& operator[](const cx::string& s) { return storage[0][s]; }
    constexpr const value& operator[](const cx::static_string& s) const { return storage[0][s]; }
    constexpr const value& operator[](const cx::static_string& s) { return storage[0][s]; }
    constexpr const value& operator[](const size_t idx) const { return storage[0][idx]; }
    constexpr const value& operator[](const size_t idx) { return storage[0][idx]; }

    constexpr bool is_Null() const { return storage[0].is_Null(); }
    constexpr decltype(auto) to_Object() const { return storage[0].to_Object(); }
    constexpr decltype(auto) to_Object() { return storage[0].to_Object(); }
    constexpr decltype(auto) to_Array() const { return storage[0].to_Array(); }
    constexpr decltype(auto) to_Array() { return storage[0].to_Array(); }
    constexpr bool& to_String() const { return storage[0].to_String(); }
    constexpr bool& to_String() { return storage[0].to_String(); }
    constexpr bool& to_Number() const { return storage[0].to_Number(); }
    constexpr bool& to_Number() { return storage[0].to_Number(); }
    constexpr bool& to_Boolean() const { return storage[0].to_Boolean(); }
    constexpr bool& to_Boolean() { return storage[0].to_Boolean(); }

  private:
    cx::vector<value, N> storage;
  };

  namespace literals
  {
    template <typename T, T... Ts>
    constexpr auto operator "" _json()
    {
      constexpr std::initializer_list<T> il{Ts...};
      constexpr auto N = numobjects<Ts...>();
      return value_wrapper<N>(std::string_view(il.begin(), il.size()));
    }
  }

}
