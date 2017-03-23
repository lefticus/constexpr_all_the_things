#include <cx_algorithm.h>
#include <cx_map.h>
#include <cx_optional.h>
#include <cx_pair.h>
#include <cx_string.h>
#include <cx_vector.h>

#include <cx_json_parser.h>
#include <cx_json_value.h>

void json_tests()
{
  using namespace std::literals;

  {
    // test true, false and null literals
    constexpr auto true_val = JSON::bool_parser()("true"sv);
    static_assert(true_val && true_val->first);

    constexpr auto false_val = JSON::bool_parser()("false"sv);
    static_assert(false_val && !false_val->first);

    constexpr auto null_val = JSON::null_parser()("null"sv);
    static_assert(null_val);
  }

  {
    // test strings (unicode chars unsupported as yet)
    constexpr auto char_val = JSON::string_char_parser()("t"sv);
    static_assert(char_val && char_val->first[0] == 't');

    constexpr auto echar_val = JSON::string_char_parser()("\t"sv);
    static_assert(echar_val && echar_val->first[0] == '\t');

    {
      constexpr auto str_val = JSON::string_parser()(R"("")"sv);
      static_assert(str_val && str_val->first.empty());
    }

    {
      // the implementation of operator==(string_view, string_view) isn't
      // constexpr yet, so we'll use a homegrown constexpr equal
      constexpr auto str_val = JSON::string_parser()(R"("hello")"sv);
      constexpr auto expected_str = "hello"sv;
      static_assert(str_val &&
                    cx::equal(str_val->first.cbegin(), str_val->first.cend(),
                              expected_str.cbegin(), expected_str.cend()));
    }
  }

  {
    // test numbers
    {
      constexpr auto number_val = JSON::number_parser()("0"sv);
      static_assert(number_val && number_val->first == 0);
    }

    {
      constexpr auto number_val = JSON::number_parser()("123"sv);
      static_assert(number_val && number_val->first == 123.0);
    }

    {
      constexpr auto number_val = JSON::number_parser()("-123"sv);
      static_assert(number_val && number_val->first == -123.0);
    }

    {
      constexpr auto number_val = JSON::number_parser()(".123"sv);
      static_assert(!number_val);
    }

    {
      constexpr auto number_val = JSON::number_parser()("0.123"sv);
      static_assert(number_val && number_val->first == 0.123);
    }

    {
      constexpr auto number_val = JSON::number_parser()("456.123"sv);
      static_assert(number_val && number_val->first == 456.123);
    }

    {
      constexpr auto number_val = JSON::number_parser()("456.123e1"sv);
      static_assert(number_val && number_val->first == 456.123e1);
    }

    {
      constexpr auto number_val = JSON::number_parser()("456.123e-1"sv);
      static_assert(number_val && number_val->first == 456.123e-1);
    }
  }

  {
    // unicode points: should come out as utf-8
    // U+2603 is the snowman
    {
      constexpr auto u = JSON::unicode_point_parser()("\\u2603"sv);
      static_assert(u && u->first.size() == 3
                    && u->first[0] == static_cast<char>(0xe2)
                    && u->first[1] == static_cast<char>(0x98)
                    && u->first[2] == static_cast<char>(0x83));
    }
  }

  {
    // test number of JSON objects parsing
    {
      constexpr auto d = JSON::numobjects_parser()("true"sv);
      static_assert(d && d->first == 1);
    }
    {
      constexpr auto d = JSON::numobjects_parser()("[]"sv);
      static_assert(d && d->first == 1);
    }
    {
      constexpr auto d = JSON::numobjects_parser()("[1,2,3,4]"sv);
      static_assert(d && d->first == 5);
    }
    {
      constexpr auto d = JSON::numobjects_parser()(R"({"a":1, "b":2})"sv);
      static_assert(d && d->first == 3);
    }
  }

  {
    // test string size of JSON objects parsing
    {
      constexpr auto d = JSON::string_size_parser()(R"("a")"sv);
      static_assert(d && d->first == 1);
    }
    {
      constexpr auto d = JSON::stringsize_parser()("true"sv);
      static_assert(d && d->first == 0);
    }
    {
      constexpr auto d = JSON::stringsize_parser()(R"(["a", "b"])"sv);
      static_assert(d && d->first == 2);
    }
    {
      constexpr auto d = JSON::stringsize_parser()(R"({"a":1, "b":2})"sv);
      static_assert(d && d->first == 2);
    }
  }

  {
    // test JSON value parsing
    using namespace JSON::literals;

    {
      constexpr auto jsa = R"([1, ["hello"], true])"_json;
      static_assert(jsa[0].to_Number() == 1
                    && jsa[1][0].to_String() == "hello"
                    && jsa[2].to_Boolean());
    }

    {
      constexpr auto jsa = R"({"a":1, "b":true, "c":["hello"]})"_json;
      static_assert(jsa["a"].to_Number() == 1
                    && jsa["b"].to_Boolean()
                    && jsa["c"][0].to_String() == "hello");
    }

    {
      constexpr auto val = R"( [
                                 1 , null , true , [ 2 ] ,
                                 { "a" : 3.14 } , "hello"
                               ] )"_json;
      static_assert(val[0].to_Number() == 1
                    && val[1].is_Null()
                    && val[2].to_Boolean()
                    && val[3][0].to_Number() == 2
                    && val[4]["a"].to_Number() == 3.14
                    && val[5].to_String() == "hello");
    }

    {
      // this one can go pretty deep...
      constexpr auto val = R"([[[[[[[[[[[[1]]]]]]]]]]]])"_json;
      static_assert(val[0][0][0][0][0][0][0][0][0][0][0][0].to_Number() == 1);
    }

    {
      //what's the point of all this?
      //constexpr can be used as template parameters!

      constexpr auto jsa = R"({"a":0, "b":1})"_json;

      constexpr std::tuple<double, int> t{ 5.2, 33 };
      static_assert(std::get<int(jsa["b"].to_Number())>(t) == 33);
    }
  }

  {
    // intentionally failing parse tests
    // using namespace JSON::literals;
    // constexpr auto jsa1 = R"({)"_json;
    // constexpr auto jsa2 = R"([)"_json;
    // constexpr auto jsa3 = R"({"a")"_json;
    // constexpr auto jsa4 = R"({1)"_json;
    // constexpr auto jsa5 = R"({"a":1)"_json;
    // constexpr auto jsa6 = R"([1,])"_json;

  }
}
