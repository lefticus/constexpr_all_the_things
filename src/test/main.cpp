#include <cx_algorithm.h>
#include <cx_map.h>
#include <cx_optional.h>
#include <cx_pair.h>
#include <cx_string.h>
#include <cx_vector.h>

#include <cx_json_parser.h>
#include <cx_json_value.h>


#include <iostream>

constexpr auto get_json_value()
{
  // we can elide the <> in C++17 with class template type deduction
  cx::JSON_Value j{};
  j["a"].to_Number() = 15;
  j["b"].to_String() = "Hello World";
  j["d"].to_Array();
  j["c"]["b"].to_Array().push_back(10.0);
  j["c"]["a"] = cx::static_string("Hello World");
  j["c"]["a"].to_Array().push_back(5.2);
  return j;
}

int main(int, char *[])
{
  constexpr auto json_value = get_json_value();
  static_assert(json_value["c"]["a"][0].to_Number() == 5.2);

  static_assert(json_value["b"].to_String().size() == 11);
  static_assert(cx::static_string("Hello World").size() == 11);
  static_assert(json_value["b"].to_String() == "Hello World");
  

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
    constexpr auto f =
      [] () {
        cx::vector<char, 10> v;
        v.push_back(1);
        v.push_back(2);
        v.push_back(3);
        cx::vector<char, 10> v2;
        v.push_back(4);
        v.push_back(5);
        v.push_back(6);
        cx::copy(v2.cbegin(), v2.cend(), cx::back_insert_iterator(v));
        return v.size();
      };
    static_assert(f() == 6);
  }

  {
    // test JSON values

    constexpr auto true_val = JSON::value_parser<>()("true"sv);
    static_assert(true_val && true_val->first.to_Boolean());

    constexpr auto false_val = JSON::value_parser<>()("false"sv);
    static_assert(false_val && !false_val->first.to_Boolean());

    constexpr auto null_val = JSON::value_parser<>()("null"sv);
    static_assert(null_val && null_val->first.is_Null());

    constexpr auto number_val = JSON::value_parser<>()("1.23"sv);
    static_assert(number_val && number_val->first.to_Number() == 1.23);

    constexpr auto str_val = JSON::value_parser<>()(R"("hello")"sv);
    static_assert(str_val && str_val->first.to_String() == "hello");

    {
      constexpr auto object_val = JSON::value_parser<>()("{}"sv);
      static_assert(object_val && object_val->first.to_Object().empty());
    }

    {
      constexpr auto object_val = JSON::value_parser<>()(R"({"a":1})"sv);
      static_assert(object_val && object_val->first["a"].to_Number() == 1);
    }

    {
      constexpr auto object_val = JSON::value_parser<>()(R"({"a":1,"b":true,"c":{}})"sv);
      static_assert(object_val
                    && object_val->first.to_Object().size() == 3
                    && object_val->first["a"].to_Number() == 1
                    && object_val->first["b"].to_Boolean()
                    && object_val->first["c"].to_Object().empty());
    }

    {
      constexpr auto array_val = JSON::value_parser<>()("[]"sv);
      static_assert(array_val && array_val->first.to_Array().empty());
    }

    {
      constexpr auto array_val = JSON::recur::array_parser()(R"([1,null,true,[2],{},"hello"])"sv);
      static_assert(array_val
                    && array_val->first[0].to_Number() == 1
                    && array_val->first[1].is_Null()
                    && array_val->first[2].to_Boolean()
                    && array_val->first[3][0].to_Number() == 2
                    && array_val->first[4].to_Object().empty()
                    && array_val->first[5].to_String() == "hello");
    }

    {
      using namespace JSON::literals;
      constexpr auto val = R"( [
                                 1 , null , true , [ 2 ] ,
                                 { "a" : 3.14 } , "hello"
                               ] )"_json;
      static_assert(val
                    && val->first[0].to_Number() == 1
                    && val->first[1].is_Null()
                    && val->first[2].to_Boolean()
                    && val->first[3][0].to_Number() == 2
                    && val->first[4]["a"].to_Number() == 3.14
                    && val->first[5].to_String() == "hello");
    }

  }

  return 0;
}
