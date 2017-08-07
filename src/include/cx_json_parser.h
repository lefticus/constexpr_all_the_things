#pragma once

#include <cx_algorithm.h>
#include <cx_iterator.h>
#include <cx_json_value.h>
#include <cx_parser.h>
#include <cx_string.h>

#include <functional>
#include <limits>
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

  // ---------------------------------------------------------------------------
  // parsing JSON strings

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

  // ---------------------------------------------------------------------------
  // parse the size of a JSON string

  constexpr std::size_t to_utf8_count(uint32_t hexcode)
  {
    if (hexcode <= 0x7f) {
      return 1;
    } else if (hexcode <= 0x7ff) {
      return 2;
    } else if (hexcode <= 0xffff) {
      return 3;
    }
    return 4;
  }

  constexpr auto unicode_point_count_parser()
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
    return fmap(to_utf8_count, p);
  }

  constexpr auto string_char_count_parser()
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

    return fmap([] (auto) -> std::size_t { return 1; }, p)
      | unicode_point_count_parser();
  }

  constexpr auto string_size_parser()
  {
    constexpr auto quote_parser = make_char_parser('"');
    constexpr auto str_parser =
      many(string_char_count_parser(), std::size_t{}, std::plus<>{});
    return quote_parser < str_parser > quote_parser;
  }

  //----------------------------------------------------------------------------
  // JSON number-of-objects-required and string-size-required parser
  //
  // An array is 1 + number of objects in the array
  // An object is 1 + number of objects in the object + 1 for each key
  // Anything else is just 1
  //
  // A string is its own size
  // An array is the sum of value sizes within it
  // An object is the sum of key sizes and value sizes within it
  // Anything else is just 0

  struct Sizes
  {
    std::size_t num_objects;
    std::size_t string_size;
  };

  constexpr Sizes operator+(const Sizes& x, const Sizes& y)
  {
    return {x.num_objects + y.num_objects,
            x.string_size + y.string_size};
  }

  template <std::size_t = 0>
  struct sizes_recur
  {
    // Because the lambda returned from value_parser has no captures, it can
    // decay to a function pointer. This helps clang deduce the return type of
    // value_parser here.
    using P = auto (*)(const parse_input_t&) -> parse_result_t<Sizes>;

    // parse a JSON value

    static constexpr auto value_parser() -> P
    {
      using namespace std::literals;
      return [] (const auto& sv) -> parse_result_t<Sizes> {
        constexpr auto p =
          fmap([] (auto) { return Sizes{1, 0}; },
               make_string_parser("true"sv) | make_string_parser("false"sv)
               | make_string_parser("null"sv))
          | fmap([] (auto) { return Sizes{1, 0}; },
                 number_parser())
          | fmap([] (std::size_t len) { return Sizes{1, len}; },
                 string_size_parser())
          | array_parser()
          | object_parser();
        return (skip_whitespace() < p)(sv);
      };
    }

    // parse a JSON array

    static constexpr auto array_parser()
    {
      return make_char_parser('[') <
        separated_by_val(value_parser(),
                         skip_whitespace() < make_char_parser(','),
                         Sizes{1, 0}, std::plus<>{})
          > skip_whitespace()
        > (make_char_parser(']') | fail(']', [] { throw "expected ]"; }));
    }

    // parse a JSON object

    static constexpr auto key_value_parser()
    {
      // would like to give sensible errors here about expecting ':' or
      // expecting string key, but this parser needs to be able to fail without
      // error to deal with empty objects ({})
      constexpr auto p =
        skip_whitespace() < string_size_parser() > skip_whitespace() > make_char_parser(':');
      return bind(p,
                  [&] (std::size_t len, const auto& sv) {
                    return fmap(
                        [len] (const Sizes& s) {
                          return Sizes{s.num_objects + 1, s.string_size + len}; },
                        value_parser())(sv);
                  });
    }

    static constexpr auto object_parser()
    {
      return make_char_parser('{') <
        separated_by_val(key_value_parser(),
                         skip_whitespace() < make_char_parser(','),
                         Sizes{1, 0}, std::plus<>{})
        > skip_whitespace()
        > (make_char_parser('}') | fail('}', [] { throw "expected }"; }));
    }

  };

  // provide the sizes parser outside the struct qualification
  constexpr auto sizes_parser = sizes_recur<>::value_parser;

  template <char... Cs>
  constexpr auto sizes()
  {
    std::initializer_list<char> il{Cs...};
    return sizes_parser()(std::string_view(il.begin(), il.size()))->first;
  }

  //----------------------------------------------------------------------------
  // JSON extent parser
  // Returns the string_view that represents the value literal

  template <std::size_t = 0>
  struct extent_recur
  {
    // As with sizes_recur, the lambda returned from value_parser has no
    // captures, so it can decay to a function pointer. This helps clang deduce
    // the return type of value_parser here.
    using P = auto (*)(const parse_input_t&) -> parse_result_t<std::string_view>;

    // parse a JSON value

    static constexpr auto value_parser() -> P
    {
      using namespace std::literals;
      using R = parse_result_t<std::string_view>;
      return [] (const auto& sv) -> R {
        constexpr auto p =
          fmap([] (auto) { return std::monostate{}; },
               make_string_parser("true"sv) | make_string_parser("false"sv)
               | make_string_parser("null"sv))
          | fmap([] (auto) { return std::monostate{}; },
                 number_parser())
          | fmap([] (auto) { return std::monostate{}; },
                 string_size_parser())
          | array_parser()
          | object_parser();
        auto r = (skip_whitespace() < p)(sv);
        if (!r) return std::nullopt;
        std::size_t len = static_cast<std::size_t>(r->second.data() - sv.data());
        return R(cx::make_pair(std::string_view{sv.data(), len}, r->second));
      };
    }

    // parse a JSON array

    static constexpr auto array_parser()
    {
      return make_char_parser('[') <
        separated_by_val(value_parser(),
                         skip_whitespace() < make_char_parser(','),
                         std::monostate{}, [] (auto x, auto) { return x; })
          > skip_whitespace() > make_char_parser(']');
    }

    // parse a JSON object

    static constexpr auto key_value_parser()
    {
      return skip_whitespace() < string_size_parser()
        < skip_whitespace() < make_char_parser(':') < value_parser();
    }

    static constexpr auto object_parser()
    {
      return make_char_parser('{') <
        separated_by_val(key_value_parser(),
                         skip_whitespace() < make_char_parser(','),
                         std::monostate{}, [] (auto x, auto) { return x; })
          > skip_whitespace() > make_char_parser('}');
    }

  };

  // provide the extent parser outside the struct qualification
  constexpr auto extent_parser = extent_recur<>::value_parser;

  //----------------------------------------------------------------------------
  // JSON parser

  // parse into a vector
  // return the past-the-end index into the vector resulting from parsing

  template <std::size_t NObj, std::size_t NString>
  struct value_recur
  {
    using V = value[NObj];
    using S = cx::basic_string<char, NString>;

    // Here, value_parser returns a lambda with captures, so it can't decay to a
    // function pointer type. clang cannot deduce the return type of
    // value_parser properly when it uses a lambda, but we can make our own
    // "lambda object" and then it works...
#ifdef __clang__
    struct lambda
    {
      constexpr lambda(V& v_, S& s_, const std::size_t& idx_, const std::size_t& max_)
        : v(v_), s(s_), idx(idx_), max(max_)
      {}

      constexpr auto operator()(const parse_input_t& sv) -> parse_result_t<std::size_t>
      {
        using namespace std::literals;
        const auto p =
          fmap([&v = v, idx = idx, max = max] (auto) { v[idx].to_Boolean() = true; return max; },
               make_string_parser("true"sv))
          | fmap([&v = v, idx = idx, max = max] (auto) { v[idx].to_Boolean() = false; return max; },
                 make_string_parser("false"sv))
          | fmap([&v = v, idx = idx, max = max] (auto) { v[idx].to_Null(); return max; },
                 make_string_parser("null"sv))
          | fmap([&v = v, idx = idx, max = max] (double d) { v[idx].to_Number() = d; return max; },
                 number_parser())
          | fmap([&v = v, idx = idx, max = max] (const value::ExternalView& ev) {
                   v[idx].to_String() = ev;
                   return max;
                 }, string_parser(s))
          | (make_char_parser('[') < array_parser(v, s, idx, max))
          | (make_char_parser('{') < object_parser(v, s, idx, max))
        ;
        return (skip_whitespace() < p)(sv);
      }

      V& v;
      S& s;
      const std::size_t& idx;
      const std::size_t& max;
    };
#endif // __clang__

    // parse a JSON value

    // note idx and max are const refs here because otherwise they are "not a
    // constant expression"?
    // idx is the index of the thing we're currently parsing
    // max is the one-past-the-end index into the vector (ie. where it can grow)
    static constexpr auto value_parser(V& v, S& s,
                                       const std::size_t& idx,
                                       const std::size_t& max)
    {
#ifdef __clang__
      return lambda(v, s, idx, max);
#else
      using namespace std::literals;
      return [&] (const auto& sv) -> parse_result_t<std::size_t> {
        const auto p =
          fmap([&] (auto) { v[idx].to_Boolean() = true; return max; },
               make_string_parser("true"sv))
          | fmap([&] (auto) { v[idx].to_Boolean() = false; return max; },
                 make_string_parser("false"sv))
          | fmap([&] (auto) { v[idx].to_Null(); return max; },
                 make_string_parser("null"sv))
          | fmap([&] (double d) { v[idx].to_Number() = d; return max; },
                 number_parser())
          | fmap([&] (const value::ExternalView& ev) {
                   v[idx].to_String() = ev;
                   return max;
                 }, string_parser(s))
          | (make_char_parser('[') < array_parser(v, s, idx, max))
          | (make_char_parser('{') < object_parser(v, s, idx, max))
        ;
        return (skip_whitespace() < p)(sv);
      };
#endif
    }

    // a string parser which accumulates its string into external storage - we
    // use the max value of size_t as a sentinel to lazily evaluate the
    // beginning of the string... I'm not proud

    static constexpr auto string_parser(S& s)
    {
      constexpr auto quote_parser = make_char_parser('"');
      const auto str_parser =
        many(string_char_parser(),
             value::ExternalView{std::numeric_limits<std::size_t>::max(), 0},
             [&] (auto ev, const auto& str) {
               auto offset = ev.offset == std::numeric_limits<std::size_t>::max()
                 ? s.size() : ev.offset;
               cx::copy(str.cbegin(), str.cend(), cx::back_insert_iterator(s));
               return value::ExternalView{offset, ev.extent + str.size()};
             });
      return quote_parser < str_parser > quote_parser;
    }

    // parse a JSON array

    static constexpr auto array_parser(V& v, S& s,
                                       const std::size_t& idx,
                                       const std::size_t& max)
    {
      using R = parse_result_t<std::size_t>;
      return [&] (const auto& sv) -> R {
        // parse the extent of each subvalue and put it into storage to
        // be parsed later
        const auto p = separated_by_val(
            extent_parser(), skip_whitespace() < make_char_parser(','),
            std::size_t{max}, [&] (std::size_t i, const std::string_view& extent) {
                 v[i].to_Unparsed() = extent;
                 return i+1;
               })
          > skip_whitespace() > make_char_parser(']');
        auto r = p(sv);
        if (!r) return std::nullopt;
        // set up the array value
        v[idx].to_Array() =
          value::ExternalView{ max, r->first - max };
        // now properly parse the subvalues
        std::size_t m = r->first;
        for (auto i = max; i < r->first; ++i) {
          auto subr = value_parser(v, s, i, m)(v[i].to_Unparsed());
          if (!subr) return std::nullopt;
          m = subr->first;
        }
        // finally return the current extent of the storage
        return R(cx::make_pair(m, r->second));
      };
    }

    // parse a JSON object

    struct kv_extent
    {
      value::ExternalView key;
      std::string_view val;
    };

    // parse a key-value pair as the string key and the extent of the value
    static constexpr auto key_value_extent_parser(S& s)
    {
      const auto p =
        skip_whitespace() < string_parser(s) > skip_whitespace() > make_char_parser(':');
      return bind(p,
                  [] (const value::ExternalView& key, const auto& sv) {
                    return fmap([&] (const std::string_view& val) {
                                  return kv_extent{ key, val };
                                },
                      extent_parser())(sv);
                  });
    }

    static constexpr auto object_parser(V& v, S& s,
                                        const std::size_t& idx,
                                        const std::size_t& max)
    {
      using R = parse_result_t<std::size_t>;
      return [&] (const auto& sv) -> R {
        // parse the extent of each subvalue and put it into storage to
        // be parsed later
        const auto p = separated_by_val(
            key_value_extent_parser(s), skip_whitespace() < make_char_parser(','),
            std::size_t{max}, [&] (std::size_t i, const auto& kve) {
                v[i].to_String() = kve.key;
                v[i+1].to_Unparsed() = kve.val;
                return i+2;
            }) > skip_whitespace() > make_char_parser('}');
        auto r = p(sv);
        if (!r) return std::nullopt;
        // set up the object value
        v[idx].to_Object() =
          value::ExternalView{ max, r->first - max };
        // now properly parse the subvalues
        std::size_t m = r->first;
        for (auto i = max; i < r->first; i += 2) {
          auto subr = value_parser(v, s, i+1, m)(v[i+1].to_Unparsed());
          if (!subr) return std::nullopt;
          m = subr->first;
        }
        // finally return the current extent of the storage
        return R(cx::make_pair(m, r->second));
      };
    }

  };

  // A value_wrapper wraps a parsed JSON::value and contains the externalized
  // storage.
  template <size_t NumObjects, size_t StringSize>
  struct value_wrapper
  {
    constexpr void construct(parse_input_t s)
    {
      value_recur<NumObjects, StringSize>::value_parser(
          object_storage, string_storage, 0, 1)(s);
    }

    constexpr operator value_proxy<NumObjects, const cx::vector<value, NumObjects>,
                                   const cx::basic_string<char, StringSize>>() const {
      return value_proxy{0, object_storage, string_storage};
    }
    constexpr operator value_proxy<NumObjects, cx::vector<value, NumObjects>,
                                   cx::basic_string<char, StringSize>>() {
      return value_proxy{0, object_storage, string_storage};
    }

    using Value_Proxy = value_proxy<NumObjects, const value[NumObjects],
                                   const cx::basic_string<char, StringSize>>;

    template <typename K,
              std::enable_if_t<!std::is_integral<K>::value, int> = 0>
    constexpr auto operator[](const K& s) const {
      return Value_Proxy{0, object_storage, string_storage}[s];
    }
    template <typename K,
              std::enable_if_t<!std::is_integral<K>::value, int> = 0>
    constexpr auto operator[](const K& s) {
      return Value_Proxy{0, object_storage, string_storage}[s];
    }
    constexpr auto object_Size() const {
      return Value_Proxy{0, object_storage, string_storage}.object_Size();
    }

    constexpr auto operator[](std::size_t idx) const {
      return Value_Proxy{0, object_storage, string_storage}[idx];
    }
    constexpr auto operator[](std::size_t idx) {
      return Value_Proxy{0, object_storage, string_storage}[idx];
    }
    constexpr auto array_Size() const {
      return Value_Proxy{0, object_storage, string_storage}.array_Size();
    }

    constexpr auto is_Null() const { return object_storage[0].is_Null(); }

    constexpr decltype(auto) to_String() const {
      return Value_Proxy{0, object_storage, string_storage}.to_String();
    }
    constexpr decltype(auto) to_String() {
      return Value_Proxy{0, object_storage, string_storage}.to_String();
    }
    constexpr auto string_Size() const {
      return Value_Proxy{0, object_storage, string_storage}.string_Size();
    }

    constexpr decltype(auto) to_Number() const { return object_storage[0].to_Number(); }
    constexpr decltype(auto) to_Number() { return object_storage[0].to_Number(); }

    constexpr decltype(auto) to_Boolean() const { return object_storage[0].to_Boolean(); }
    constexpr decltype(auto) to_Boolean() { return object_storage[0].to_Boolean(); }

    constexpr auto num_objects() const { return NumObjects; }
    constexpr auto string_size() const { return StringSize; }

  private:
    // when this is a cx::vector, GCC ICEs...
    value object_storage[NumObjects];
    cx::basic_string<char, StringSize> string_storage;
  };

  namespace literals
  {

    // why cannot we get regular literal operator template here?
    template <typename T, T... Ts>
    constexpr auto operator "" _json()
    {
      const std::initializer_list<T> il{Ts...};
      // I tried using structured bindings here, but g++ says:
      // "error: decomposition declaration cannot be declared 'constexpr'"
      constexpr auto S = sizes<Ts...>();
      auto val = value_wrapper<S.num_objects, S.string_size>{};
      val.construct(std::string_view(il.begin(), il.size()));
      return val;
    }

  }

}
