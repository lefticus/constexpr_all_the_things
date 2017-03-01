#pragma once

#include "cx_map.h"
#include "cx_vector.h"

#include <variant>

namespace cx
{
  // This Depth=5, max_vector_size=6, max_map_size=6
  // represents the practical limits of what can be compiled with GCC 7.0.1 trunk
  template<size_t Depth=4>
  struct JSON_Value
  {
    static constexpr size_t max_vector_size{6};
    static constexpr size_t max_map_size{6};

    // We cannot use a union because the constexpr rules are too strict 
    // for which element is initialized and which is accessed, making it
    // impossible to default initialize it
    struct Data
    {
      // We decrease the Depth by 1 to build a tree of different types
      cx::map<cx::string, JSON_Value<Depth-1>, max_map_size> object;
      cx::vector<JSON_Value<Depth-1>, max_vector_size> array;
      cx::string string;
      double number{0};
      bool boolean{false};
    };

    enum class Type
    {
      String,
      Number,
      Array,
      Object,
      Boolean,
      Null
    };

    Type type = Type::Null;
    Data data;

    constexpr JSON_Value() = default;

    constexpr JSON_Value(const double t_d) {
      to_Number() = t_d;
    }

    constexpr JSON_Value(const bool t_b) {
      to_Boolean() = t_b;
    }

    constexpr JSON_Value(const std::monostate) {
      type = Type::Null;
    }

    constexpr JSON_Value(cx::static_string t_s) {
      to_String() = cx::string(std::move(t_s));
    }

    constexpr JSON_Value(cx::string t_s) {
      to_String() = std::move(t_s);
    }

    constexpr decltype(auto) operator[](const cx::string &s) {
      return to_Object()[s];
    }

    constexpr decltype(auto) operator[](const cx::static_string &s) {
      return operator[](cx::string(s));
    }

    constexpr decltype(auto) operator[](const cx::string &s) const {
      return to_Object().at(s);
    }

    constexpr decltype(auto) operator[](const cx::static_string &s) const {
      return operator[](cx::string(s));
    }


    constexpr decltype(auto) operator[](const size_t idx) {
      return to_Array()[idx];
    }

    constexpr decltype(auto) operator[](const size_t idx) const {
      return to_Array()[idx];
    }

    constexpr void assert_type(Type t) const
    {
      if (type != t) throw std::runtime_error("Incorrect type");
    }

    constexpr bool is_Null() const
    {
      return type == Type::Null;
    }

    constexpr decltype(auto) to_Array() const
    {
      assert_type(Type::Array);
      return (data.array);
    }

    constexpr decltype(auto) to_Array()
    {
      if (type != Type::Array) {
        type = Type::Array;
        data.array = {};
      }
      return (data.array);
    }

    constexpr decltype(auto) to_Object() const
    {
      assert_type(Type::Object);
      return (data.object);
    }

    constexpr decltype(auto) to_Object()
    {
      if (type != Type::Object) {
        type = Type::Object;
        data.object = {};
      }
      return (data.object);
    }

    constexpr const cx::string& to_String() const
    {
      assert_type(Type::String);
      return data.string;
    }

    constexpr cx::string& to_String()
    {
      if (type != Type::String) {
        type = Type::String;
        data.string = "";
      }
      return data.string;
    }

    constexpr const double& to_Number() const
    {
      assert_type(Type::Number);
      return data.number;
    }

    constexpr double& to_Number()
    {
      if (type != Type::Number) {
        type = Type::Number;
        data.number = 0.0;
      }
      return data.number;
    }

    constexpr const bool& to_Boolean() const
    {
      assert_type(Type::Boolean);
      return data.boolean;
    }

    constexpr bool& to_Boolean()
    {
      if (type != Type::Boolean) {
        type = Type::Boolean;
        data.boolean = false;
      }
      return data.boolean;
    }

  };

  // End of the tree
  template<>
  struct JSON_Value<0>
  {
  };

}

