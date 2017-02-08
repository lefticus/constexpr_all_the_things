#include <cx_algorithm.h>
#include <cx_map.h>
#include <cx_optional.h>
#include <cx_pair.h>
#include <cx_string.h>
#include <cx_vector.h>

#include <cx_json_parser.h>
#include <cx_json_value.h>

constexpr auto get_json_value()
{
  // we can elide the <> in C++17 with class template type deduction
  cx::JSON_Value j{};
  j["a"].to_Number() = 15;
  j["b"].to_String() = "Hello World";
  j["d"].to_Array();
  j["c"]["a"]["b"].to_Array().push_back(10);
  j["c"]["a"]["c"] = cx::string("Hello World");
  j["c"]["a"]["d"].to_Array().push_back(5.2);
  return j;
}

int main(int, char *[])
{
  // constexpr auto json_value = get_json_value();
  // static_assert(json_value["c"]["a"]["d"][0].to_Number() == 5.2);

  using namespace std::literals;
  {
    // test true, false and null literals
    constexpr auto true_val = JSON::parse_true("true"sv);
    static_assert(true_val && true_val->first);

    constexpr auto false_val = JSON::parse_false("false"sv);
    static_assert(false_val && !false_val->first);

    constexpr auto null_val = JSON::parse_null("null"sv);
    static_assert(null_val);
  }

  {
    // test strings (unicode chars unsupported as yet)
    constexpr auto char_val = JSON::string_char_parser()("t"sv);
    static_assert(char_val && char_val->first == 't');

    constexpr auto echar_val = JSON::string_char_parser()("\t"sv);
    static_assert(echar_val && echar_val->first == '\t');

    {
      constexpr auto str_val = JSON::parse_string(R"("")"sv);
      static_assert(str_val && str_val->first.empty());
    }

    {
      // the implementation of operator==(string_view, string_view) isn't
      // constexpr yet, so we'll use a homegrown constexpr equal
      constexpr auto str_val = JSON::parse_string(R"("hello")"sv);
      constexpr auto expected_str = "hello"sv;
      static_assert(str_val &&
                    cx::equal(str_val->first.cbegin(), str_val->first.cend(),
                              expected_str.cbegin(), expected_str.cend()));
    }
  }

  {
    // test numbers
    {
      constexpr auto number_val = JSON::parse_number("0"sv);
      static_assert(number_val && number_val->first == 0);
    }

    {
      constexpr auto number_val = JSON::parse_number("123"sv);
      static_assert(number_val && number_val->first == 123.0);
    }

    {
      constexpr auto number_val = JSON::parse_number("-123"sv);
      static_assert(number_val && number_val->first == -123.0);
    }

    {
      constexpr auto number_val = JSON::parse_number(".123"sv);
      static_assert(!number_val);
    }

    {
      constexpr auto number_val = JSON::parse_number("0.123"sv);
      static_assert(number_val && number_val->first == 0.123);
    }

    {
      constexpr auto number_val = JSON::parse_number("456.123"sv);
      static_assert(number_val && number_val->first == 456.123);
    }

    {
      constexpr auto number_val = JSON::parse_number("456.123e1"sv);
      static_assert(number_val && number_val->first == 456.123e1);
    }

    {
      constexpr auto number_val = JSON::parse_number("456.123e-1"sv);
      static_assert(number_val && number_val->first == 456.123e-1);
    }
  }

  {
    // sum array of ints
    {
      constexpr auto sum_val = JSON::parse_array_sum("[1,2,3]"sv);
      static_assert(sum_val && sum_val->first == 6);
    }
  }

  return 0;
}
