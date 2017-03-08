#pragma once

#include <cx_optional.h>
#include <cx_pair.h>

#include <cstddef>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace cx
{
namespace parser
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

  // lift a value into a parser
  template <typename T>
  constexpr auto lift(T&& t)
  {
    return [t = std::forward<T>(t)] (parse_input_t s) {
             return parse_result_t<T>(
                 cx::make_pair(std::move(t), s));
           };
  }

  // the parser that always fails
  template <typename T>
  constexpr auto fail(T)
  {
    return [=] (parse_input_t) -> parse_result_t<T> {
      return std::nullopt;
    };
  }

  template <typename T, typename ErrorFn>
  constexpr auto fail(T, ErrorFn f)
  {
    return [=] (parse_input_t) -> parse_result_t<T> {
      f();
      return std::nullopt;
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
        init = f(init, r->first);
        s = r->second;
      }
      return cx::make_pair(init, s);
    }

    template <typename P, typename T, typename F>
    constexpr cx::pair<T, parse_input_t> accumulate_n_parse(
        parse_input_t s, P&& p, std::size_t n, T init, F&& f)
    {
      while (n != 0) {
        const auto r = p(s);
        if (!r) return cx::make_pair(init, s);
        init = f(init, r->first);
        s = r->second;
        --n;
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

  // apply a parser exactly n times, accumulating the results according to a
  // function F. F :: T -> (parse_t<P>, parse_input_t) -> T
  template <typename P, typename T, typename F>
  constexpr auto exactly_n(P&& p, std::size_t n, T&& init, F&& f)
  {
    return [p = std::forward<P>(p), n, init = std::forward<T>(init),
            f = std::forward<F>(f)] (parse_input_t s) {
             return parse_result_t<T>(
                 detail::accumulate_n_parse(s, p, n, init, f));
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

  // apply many1 instances of a parser, with another parser interleaved.
  // accumulate the results with the given function.
  template <typename P1, typename P2, typename F>
  constexpr auto separated_by(P1&& p1, P2&& p2, F&& f)
  {
    using T = parse_t<P1>;
    return [p1 = std::forward<P1>(p1), p2 = std::forward<P2>(p2),
            f = std::forward<F>(f)] (
                parse_input_t s) -> parse_result_t<T> {
             const auto r = p1(s);
             if (!r) return std::nullopt;
             const auto p = p2 < p1;
             return parse_result_t<T>(
                 detail::accumulate_parse(r->second, p, r->first, f));
           };
  }

  // apply many instances of a parser, with another parser interleaved.
  // accumulate the results with the given function.
  template <typename P1, typename P2, typename F0, typename F>
  constexpr auto separated_by(P1&& p1, P2&& p2, F0&& init, F&& f)
  {
    using R = parse_result_t<std::result_of_t<F0()>>;
    return [p1 = std::forward<P1>(p1), p2 = std::forward<P2>(p2),
            init = std::forward<F0>(init), f = std::forward<F>(f)] (
                parse_input_t s) -> R {
      const auto r = p1(s);
      if (!r) return R(cx::make_pair(init(), s));
      const auto p = p2 < p1;
      return R(detail::accumulate_parse(r->second, p, f(init(), r->first), f));
    };
  }

  // apply many instances of a parser, with another parser interleaved.
  // accumulate the results with the given function.
  template <typename P1, typename P2, typename T, typename F>
  constexpr auto separated_by_val(P1&& p1, P2&& p2, T&& init, F&& f)
  {
    using R = parse_result_t<std::remove_reference_t<T>>;
    return [p1 = std::forward<P1>(p1), p2 = std::forward<P2>(p2),
            init = std::forward<T>(init), f = std::forward<F>(f)] (
                parse_input_t s) -> R {
      const auto r = p1(s);
      if (!r) return R(cx::make_pair(init, s));
      const auto p = p2 < p1;
      return R(detail::accumulate_parse(r->second, p, f(init, r->first), f));
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
    return many1(one_of("0123456789"sv),
                 0,
                 [] (int acc, char c) { return (acc*10) + (c-'0'); });
  }

  // parse an int (may not begin with 0)
  constexpr auto int1_parser()
  {
    using namespace std::literals;
    return bind(one_of("123456789"sv),
                [] (char x, parse_input_t rest) {
                  return many(one_of("0123456789"sv),
                              static_cast<int>(x - '0'),
                              [] (int acc, char c) { return (acc*10) + (c-'0'); })(rest);
                });
  }

  // a parser for skipping whitespace
  constexpr auto skip_whitespace()
  {
    constexpr auto ws_parser =
      make_char_parser(' ')
      | make_char_parser('\t')
      | make_char_parser('\n')
      | make_char_parser('\r');
    return many(ws_parser, std::monostate{}, [] (auto m, auto) { return m; });
  }

}
}
