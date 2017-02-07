#pragma once

#include <cx_algorithm.h>
#include <cx_optional.h>
#include <cx_pair.h>

#include <cstddef>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace JSON
{
  //----------------------------------------------------------------------------
  // A parser for T is a callable thing that takes a "string" and returns
  // (optionally) an T plus the "leftover string".
  using parse_input_t = std::string_view;
  template <typename T>
  using parse_result_t = cx::optional<cx::pair<T, std::string_view>>;

  // Get various types out of a parser
  template <typename P>
  using opt_pair_parse_t = std::result_of_t<P(parse_input_t)>;
  template <typename P>
  using pair_parse_t = typename opt_pair_parse_t<P>::value_type;
  template <typename P>
  using parse_t = typename pair_parse_t<P>::first_type;

  //----------------------------------------------------------------------------
  // parsers as monads

  // fmap a function into a parser. F :: parse_t<P> -> a
  template <typename F, typename P>
  constexpr auto fmap(F&& f, P&& p)
  {
    using R = parse_result_t<std::result_of_t<F(parse_t<P>)>>;
    return [f = std::forward<F>(f),
            p = std::forward<P>(p)] (parse_input_t i) -> R {
             const auto r = p(i);
             if (!r) return std::nullopt;
             return R(cx::make_pair(f(r->first), r->second));
           };
  }

  // bind a function into a parser. F :: (parse_t<P>, parse_input_t) -> a
  template <typename P, typename F>
  constexpr auto bind(P&& p, F&& f)
  {
    using R = std::result_of_t<F(parse_t<P>, parse_input_t)>;
    return [=] (parse_input_t i) -> R {
             const auto r = p(i);
             if (!r) return std::nullopt;
             return f(r->first, r->second);
           };
  }

  //----------------------------------------------------------------------------
  // parser combinators

  // alternation: try the first parser, and if it fails, do the second. They
  // must both return the same type.
  template <typename P1, typename P2,
            typename = std::enable_if_t<std::is_same_v<parse_t<P1>, parse_t<P2>>>>
  constexpr auto operator|(P1&& p1, P2&& p2) {
    return [=] (parse_input_t i) {
             const auto r1 = p1(i);
             if (r1) return r1;
             return p2(i);
           };
  }

  // accumulation: run two parsers in sequence and combine the outputs using the
  // given function. Both parsers must succeed.
  template <typename P1, typename P2, typename F,
            typename R = std::result_of_t<F(parse_t<P1>, parse_t<P2>)>>
  constexpr auto combine(P1&& p1, P2&& p2, F&& f) {
    return [=] (parse_input_t i) -> parse_result_t<R> {
             const auto r1 = p1(i);
             if (!r1) return std::nullopt;
             const auto r2 = p2(r1->second);
             if (!r2) return std::nullopt;
             return parse_result_t<R>(
                 cx::make_pair(f(r1->first, r2->first), r2->second));
           };
  }

  // for convenience, overload < and > to mean sequencing parsers
  // and returning the result of the chosen one
  template <typename P1, typename P2,
            typename = parse_t<P1>, typename = parse_t<P2>>
  constexpr auto operator<(P1&& p1, P2&& p2) {
    return combine(std::forward<P1>(p1),
                   std::forward<P2>(p2),
                   [] (auto, const auto& r) { return r; });
  }

  template <typename P1, typename P2,
            typename = parse_t<P1>, typename = parse_t<P2>>
  constexpr auto operator>(P1&& p1, P2&& p2) {
    return combine(std::forward<P1>(p1),
                   std::forward<P2>(p2),
                   [] (const auto& r, auto) { return r; });
  }

  // apply ? (zero or one) of a parser
  template <typename P>
  constexpr auto zero_or_one(P&& p)
  {
    using R = parse_result_t<parse_input_t>;
    return [p = std::forward<P>(p)] (parse_input_t s) -> R {
             const auto r = p(s);
             if (r) return r;
             return R(cx::make_pair(parse_input_t(s.data(), 0), s));
           };
  }

  namespace detail
  {
    template <typename P, typename T, typename F>
    constexpr cx::pair<T, parse_input_t> accumulate_parse(
        parse_input_t s, P&& p, T init, F&& f)
    {
      while (!s.empty()) {
        const auto r = p(s);
        if (!r) return cx::make_pair(init, s);
        init = f(init, *r);
        s = r->second;
      }
      return cx::make_pair(init, s);
    }
  }

  // apply * (zero or more) of a parser, accumulating the results according to a
  // function F. F :: T -> (parse_t<P>, parse_input_t) -> T
  template <typename P, typename T, typename F>
  constexpr auto many(P&& p, T&& init, F&& f)
  {
    return [p = std::forward<P>(p), init = std::forward<T>(init),
            f = std::forward<F>(f)] (parse_input_t s) {
             return parse_result_t<T>(
                 detail::accumulate_parse(s, p, init, f));
           };
  }

  // apply + (one or more) of a parser, accumulating the results according to a
  // function F. F :: T -> (parse_t<P>, parse_input_t) -> T
  template <typename P, typename T, typename F>
  constexpr auto many1(P&& p, T&& init, F&& f)
  {
    return [p = std::forward<P>(p), init = std::forward<T>(init),
            f = std::forward<F>(f)] (parse_input_t s) -> parse_result_t<T> {
             const auto r = p(s);
             if (!r) return std::nullopt;
             return parse_result_t<T>(
                 detail::accumulate_parse(r->second, p, f(init, r->first), f));
           };
  }

  // try to apply a parser, and if it fails, return a default
  template <typename P, typename T = parse_t<P>>
  constexpr auto option(T&& def, P&& p)
  {
    return [p = std::forward<P>(p),
            def = std::forward<T>(def)] (parse_input_t s) {
             const auto r = p(s);
             if (r) return r;
             return parse_result_t<T>(cx::make_pair(def, s));
           };
  }

  //----------------------------------------------------------------------------
  // parsers for various types

  // parse a given char
  constexpr auto make_char_parser(char c)
  {
    return [=] (parse_input_t s) -> parse_result_t<char> {
      if (s.empty() || s[0] != c) return std::nullopt;
      return parse_result_t<char>(
          cx::make_pair(c, parse_input_t(s.data()+1, s.size()-1)));
    };
  }

  // parse one of a set of chars
  constexpr auto one_of(std::string_view chars)
  {
    return [=] (parse_input_t s) -> parse_result_t<char> {
      if (s.empty()) return std::nullopt;
      // basic_string_view::find is supposed to be constexpr, but no...
      auto j = cx::find(chars.cbegin(), chars.cend(), s[0]);
      if (j != chars.cend()) {
        return parse_result_t<char>(
            cx::make_pair(s[0], parse_input_t(s.data()+1, s.size()-1)));
      }
      return std::nullopt;
    };
  }

  // parse none of a set of chars
  constexpr auto none_of(std::string_view chars)
  {
    return [=] (parse_input_t s) -> parse_result_t<char> {
      if (s.empty()) return std::nullopt;
      // basic_string_view::find is supposed to be constexpr, but no...
      auto j = cx::find(chars.cbegin(), chars.cend(), s[0]);
      if (j == chars.cend()) {
        return parse_result_t<char>(
            cx::make_pair(s[0], parse_input_t(s.data()+1, s.size()-1)));
      }
      return std::nullopt;
    };
  }

  // parse a given string
  constexpr auto make_string_parser(std::string_view str)
  {
    return [=] (parse_input_t s) -> parse_result_t<std::string_view> {
      const auto p = cx::mismatch(str.cbegin(), str.cend(), s.cbegin(), s.cend());
      if (p.first == str.cend()) {
        // std::distance is not constexpr?
        const auto len = static_cast<std::string_view::size_type>(
            s.cend() - p.second);
        return parse_result_t<std::string_view>(
            cx::make_pair(str, parse_input_t(p.second, len)));
      }
      return std::nullopt;
    };
  }

  // parse an int (may begin with 0)
  constexpr auto int0_parser()
  {
    using namespace std::literals;
    constexpr auto sv_to_int =
      [] (const auto& sv) -> int {
        int j = 0;
        for (char c : sv) {
          j *= 10;
          j += c - '0';
        }
        return j;
      };
    constexpr auto p =
      [] (parse_input_t s) {
        constexpr auto digit_parser = one_of("0123456789"sv);
        return many1(digit_parser,
                     std::string_view(s.data(), 0),
                     [] (const auto& acc, auto) {
                       return std::string_view(acc.data(), acc.size()+1);
                     })(s);
      };
    return fmap(sv_to_int, p);
  }

  // parse an int (may not begin with 0)
  constexpr auto int1_parser()
  {
    using namespace std::literals;
    constexpr auto sv_to_int =
      [] (const auto& sv) -> int {
        int j = 0;
        for (char c : sv) {
          j *= 10;
          j += c - '0';
        }
        return j;
      };
    constexpr auto p =
      [] (parse_input_t s) -> parse_result_t<parse_input_t> {
        constexpr auto digit0_parser = one_of("0123456789"sv);
        constexpr auto digit1_parser = one_of("123456789"sv);
        const auto r = digit1_parser(s);
        if (!r) return std::nullopt;
        return parse_result_t<parse_input_t>(
            detail::accumulate_parse(r->second, digit0_parser,
                                     parse_input_t(s.data(), 1),
                                     [] (const auto& acc, auto) {
                                       return std::string_view(acc.data(), acc.size()+1);
                                     }));
    };
    return fmap(sv_to_int, p);
  }

  //----------------------------------------------------------------------------
  // JSON value parsers

  // parse "true"
  constexpr parse_result_t<bool> parse_true(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = make_char_parser('"');
    constexpr auto p =
      quote_parser < make_string_parser("true"sv) > quote_parser;
    return fmap([] (std::string_view) { return true; }, p)(s);
  }

  // parse "false"
  constexpr parse_result_t<bool> parse_false(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = make_char_parser('"');
    constexpr auto p =
      quote_parser < make_string_parser("false"sv) > quote_parser;
    return fmap([] (std::string_view) { return false; }, p)(s);
  }

  // parse "null"
  constexpr parse_result_t<std::monostate> parse_null(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = make_char_parser('"');
    constexpr auto p =
      quote_parser < make_string_parser("null"sv) > quote_parser;
    return fmap([] (std::string_view) { return std::monostate{}; }, p)(s);
  }

  // parse a number
  constexpr parse_result_t<double> parse_number(parse_input_t s)
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

    constexpr auto p = combine(
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

    return p(s);
  }

  // parse a JSON string char
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
    return escaped_char_parser | none_of("\\\""sv);
  }

  // parse a JSON string
  constexpr auto parse_string(parse_input_t s)
  {
    constexpr auto quote_parser = make_char_parser('"');
    const auto str_parser =
      many(string_char_parser(),
           std::string_view(s.data()+1, 0),
           [] (const auto& acc, auto) {
             return std::string_view(acc.data(), acc.size()+1);
           });
    const auto p = quote_parser < str_parser > quote_parser;
    return p(s);
  }

}
