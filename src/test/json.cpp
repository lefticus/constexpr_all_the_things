#include <cx_algorithm.h>

#include <cx_json_parser.h>
#include <cx_json_value.h>

#include <string_view>
#include <tuple>

using namespace std::literals;

void simple_parse_tests()
{
  // test true, false and null literals
  constexpr auto true_val = JSON::bool_parser()("true"sv);
  static_assert(true_val && true_val->first);

  constexpr auto false_val = JSON::bool_parser()("false"sv);
  static_assert(false_val && !false_val->first);

  constexpr auto null_val = JSON::null_parser()("null"sv);
  static_assert(null_val);
}

void string_parse_tests()
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

void number_parse_tests()
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

void numobjects_tests()
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

void stringsize_tests()
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

void extent_tests()
{
  // test JSON object extent parsing
  {
    constexpr auto sv = "true"sv;
    constexpr auto r = JSON::extent_parser()(sv);
    static_assert(r &&
                  cx::equal(r->first.cbegin(), r->first.cend(),
                            sv.cbegin(), sv.cend()));
  }
  {
    constexpr auto sv = R"("hello")"sv;
    constexpr auto r = JSON::extent_parser()(sv);
    static_assert(r &&
                  cx::equal(r->first.cbegin(), r->first.cend(),
                            sv.cbegin(), sv.cend()));
  }
  {
    constexpr auto sv = "123.456"sv;
    constexpr auto r = JSON::extent_parser()(sv);
    static_assert(r &&
                  cx::equal(r->first.cbegin(), r->first.cend(),
                            sv.cbegin(), sv.cend()));
  }
  {
    constexpr auto sv = "[1,2,3]"sv;
    constexpr auto r = JSON::extent_parser()(sv);
    static_assert(r &&
                  cx::equal(r->first.cbegin(), r->first.cend(),
                            sv.cbegin(), sv.cend()));
  }
  {
    constexpr auto sv = R"({"a":1, "b":2})"sv;
    constexpr auto r = JSON::extent_parser()(sv);
    static_assert(r &&
                  cx::equal(r->first.cbegin(), r->first.cend(),
                            sv.cbegin(), sv.cend()));
  }
}

void simple_value_tests()
{
  // test JSON value parsing
  using namespace JSON::literals;

  {
    constexpr auto jsv = "true"_json;
    static_assert(jsv.to_Boolean());
  }
  {
    constexpr auto jsv = "false"_json;
    static_assert(!jsv.to_Boolean());
  }
  {
    constexpr auto jsv = "null"_json;
    static_assert(jsv.is_Null());
  }
  {
    constexpr auto jsv = "123.456"_json;
    static_assert(jsv.to_Number() == 123.456);
  }
  {
    constexpr auto jsv = R"("hello")"_json;
    static_assert(jsv.to_String() == "hello");
  }
}

void array_value_tests()
{
  // test JSON array value parsing
  using namespace JSON::literals;

  {
    constexpr auto jsv = "[]"_json;
    static_assert(jsv.array_Size() == 0);
  }
  {
    constexpr auto jsv = "[1, true, 3]"_json;
    static_assert(jsv[0].to_Number() == 1);
    static_assert(jsv[1].to_Boolean());
    static_assert(jsv[2].to_Number() == 3);
  }
  {
    constexpr auto jsv = "[1, [true, false], [2, 3]]"_json;
    static_assert(jsv[0].to_Number() == 1);
    static_assert(jsv[1][0].to_Boolean());
    static_assert(!jsv[1][1].to_Boolean());
    static_assert(jsv[2][0].to_Number() == 2);
    static_assert(jsv[2][1].to_Number() == 3);
  }
  {
    constexpr auto jsv = "[1, null, true, [2]]"_json;
    static_assert(jsv.array_Size() == 4);
    static_assert(jsv[0].to_Number() == 1);
    static_assert(jsv[1].is_Null());
    static_assert(jsv[2].to_Boolean());
    static_assert(jsv[3][0].to_Number() == 2);
  }
  {
    // arrays can go arbitrarily deep...
    constexpr auto jsv = "[[[[[[[[[[[[1]]]]]]]]]]]]"_json;
    static_assert(jsv[0][0][0][0][0][0][0][0][0][0][0][0].to_Number() == 1);
  }
  {
    // and arbitrarily wide...
    constexpr auto jsv = "[1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16]"_json;
    static_assert(jsv[0].to_Number() == 1 && jsv[15].to_Number() == 16);
  }
}

void object_value_tests()
{
  // test JSON object value parsing
  using namespace JSON::literals;

  {
    constexpr auto jsv = R"({})"_json;
    static_assert(jsv.object_Size() == 0);
  }
  {
    constexpr auto jsv = R"({"a":1})"_json;
    static_assert(jsv["a"].to_Number() == 1);
  }
  {
    constexpr auto jsv = R"({"a":1, "b":true, "c":2})"_json;
    static_assert(jsv["a"].to_Number() == 1);
    static_assert(jsv["b"].to_Boolean());
    static_assert(jsv["c"].to_Number() == 2);
  }
  {
    constexpr auto jsv = R"({"a":{}})"_json;
    static_assert(jsv["a"].object_Size() == 0);
  }
  {
    constexpr auto jsv = R"({"a":1, "b":true, "c":["hello"]})"_json;
    static_assert(jsv["a"].to_Number() == 1);
    static_assert(jsv["b"].to_Boolean());
    static_assert(jsv["c"][0].to_String() == "hello");
  }
  {
    constexpr auto jsv = R"( [
                               1 , null , true , [ 2 ] ,
                               { "a" : 3.14 } , "hello"
                             ] )"_json;
    static_assert(jsv[0].to_Number() == 1);
    static_assert(jsv[1].is_Null());
    static_assert(jsv[2].to_Boolean());
    static_assert(jsv[3][0].to_Number() == 2);
    static_assert(jsv[4]["a"].to_Number() == 3.14);
    static_assert(jsv[5].to_String() == "hello");
  }
  {
    //what's the point of all this?
    //constexpr can be used as template parameters!

    constexpr auto jsv = R"({"a":0, "b":1})"_json;

    constexpr std::tuple<double, int> t{ 5.2, 33 };
    static_assert(std::get<int(jsv["b"].to_Number())>(t) == 33);
  }
}

void fail_tests()
{
  // intentionally failing parse tests
  using namespace JSON::literals;

  // constexpr auto jsv1 = R"({)"_json;
  // constexpr auto jsv2 = R"([)"_json;
  // constexpr auto jsv3 = R"({"a")"_json;
  // constexpr auto jsv4 = R"({1)"_json;
  // constexpr auto jsv5 = R"({"a":1)"_json;
  // constexpr auto jsv6 = R"([1,])"_json;
}
