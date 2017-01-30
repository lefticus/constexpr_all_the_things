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

  template <typename T>
  struct Parser
  {
    using input_t = std::string_view;
    using parsed_t = T;
    using parsed_pair_t = cx::pair<T, std::string_view>;
    using result_t = cx::optional<parsed_pair_t>;
  };

  // fmap a function into a parser
  template <typename F, typename P>
  constexpr auto fmap(F f, P p)
  {
    using R = parse_result_t<std::result_of_t<F(parse_t<P>)>>;
    return [=] (parse_input_t i) -> R {
             const auto r = p(i);
             if (!r) return std::nullopt;
             return R(cx::make_pair(f(r->first), r->second));
           };
  }

  // sequence two parsers together, returning the either the first or second
  // thing parsed (both must succeed)
  namespace detail
  {
    template <std::size_t N, typename P1, typename P2>
    struct sequence_parser {};

    // sequence_parser<0> returns the first result
    template <typename P1, typename P2>
    struct sequence_parser<0, P1, P2> {
      constexpr auto operator()(P1 p1, P2 p2)
      {
        return [=] (parse_input_t i) -> opt_pair_parse_t<P1> {
          const auto r1 = p1(i);
          if (!r1) return std::nullopt;
          const auto r2 = p2(r1->second);
          if (!r2) return std::nullopt;
          return r1;
        };
      }
    };

    // sequence_parser<1> returns the second result
    template <typename P1, typename P2>
    struct sequence_parser<1, P1, P2> {
      constexpr auto operator()(P1 p1, P2 p2)
      {
        return [=] (parse_input_t i) -> opt_pair_parse_t<P2> {
          const auto r1 = p1(i);
          if (!r1) return std::nullopt;
          return p2(r1->second);
        };
      }
    };

    template <std::size_t N, typename P1, typename P2>
    constexpr auto seq_parser(P1&& p1, P2&& p2) {
      return sequence_parser<N, P1, P2>{}(std::forward<P1>(p1),
                                          std::forward<P2>(p2));
    }
  }

  // For convenience, overload < and > to mean sequencing parsers
  // and returning the result of the chosen one
  template <typename P1, typename P2,
            typename = parse_t<P1>, typename = parse_t<P2>>
  constexpr auto operator<(P1&& p1, P2&& p2) {
    return detail::seq_parser<1>(std::forward<P1>(p1),
                                 std::forward<P2>(p2));
  }

  template <typename P1, typename P2,
            typename = parse_t<P1>, typename = parse_t<P2>>
  constexpr auto operator>(P1&& p1, P2&& p2) {
    return detail::seq_parser<0>(std::forward<P1>(p1),
                                 std::forward<P2>(p2));
  }

  // parse a given char
  constexpr auto char_parser(char c)
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

  // apply ? (zero or one) of a char parser
  template <typename P>
  constexpr auto zero_or_one(P&& p)
  {
    using R = parse_result_t<parse_input_t>;
    return [p = std::forward<P>(p)] (parse_input_t s) -> R {
             const auto r = p(s);
             if (!r) {
               return R(cx::make_pair(parse_input_t(s.data(), 0), s));
             }
             return r;
           };
  }

  // apply * (zero or more) of a char parser
  template <typename P>
  constexpr parse_result_t<parse_input_t> many_impl(
      const char* s, size_t n, size_t max, P&& p)
  {
    using R = parse_result_t<parse_input_t>;
    const parse_input_t sofar(s, n);
    const parse_input_t rest(s+n, max-n);
    if (n == max) {
      return R(cx::make_pair(sofar, rest));
    }
    const auto r = p(rest);
    if (!r) {
      return R(cx::make_pair(sofar, rest));
    }
    return many_impl(s, n+1, max, std::forward<P>(p));
  }

  template <typename P>
  constexpr auto many(P&& p)
  {
    return [p = std::forward<P>(p)] (parse_input_t s) {
             return many_impl(s.data(), 0, s.size(), p);
           };
  }

  // apply + (one or more) of a char parser
  template <typename P>
  constexpr auto many1(P&& p)
  {
    using R = parse_result_t<parse_input_t>;
    return [p = std::forward<P>(p)] (parse_input_t s) -> R {
             const auto r = p(s);
             if (!r) return std::nullopt;
             return many(p)(s);
           };
  }

  // parse a digit
  constexpr auto digit_parser()
  {
    using namespace std::literals;
    return one_of("0123456789"sv);
  }

  // parse a given string
  constexpr auto string_parser(std::string_view str)
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

  // parse "true"
  constexpr parse_result_t<bool> parse_true(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = char_parser('"');
    constexpr auto p =
      quote_parser < string_parser("true"sv) > quote_parser;
    return fmap([] (std::string_view) { return true; }, p)(s);
  }

  // parse "false"
  constexpr parse_result_t<bool> parse_false(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = char_parser('"');
    constexpr auto p =
      quote_parser < string_parser("false"sv) > quote_parser;
    return fmap([] (std::string_view) { return false; }, p)(s);
  }

  // parse "null"
  constexpr parse_result_t<std::monostate> parse_null(parse_input_t s)
  {
    using namespace std::literals;
    constexpr auto quote_parser = char_parser('"');
    constexpr auto p =
      quote_parser < string_parser("null"sv) > quote_parser;
    return fmap([] (std::string_view) { return std::monostate{}; }, p)(s);
  }

  // parse an integer
  constexpr parse_result_t<int> parse_int(parse_input_t s)
  {
    auto sv_to_int = [] (std::string_view v) -> int {
                       int i = 0;
                       for (char c : v) {
                         i *= 10;
                         i += c - '0';
                       }
                       return i;
                     };
    return fmap(sv_to_int, many1(digit_parser()))(s);
  }

}
