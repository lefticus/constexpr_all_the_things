#pragma once

#include "cx_map.h"
#include "cx_vector.h"

#include <variant>

namespace JSON
{

  // ---------------------------------------------------------------------------
  // non-recursive definition of a JSON value

  struct value
  {
    static constexpr size_t max_vector_size{6};
    static constexpr size_t max_map_size{6};

    union Data
    {
      cx::map<cx::string, std::size_t, max_map_size> object;
      cx::vector<std::size_t, max_vector_size> array;
      cx::string string;
      double number;
      bool boolean;
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
    Data data{};

    constexpr value() = default;

    constexpr value(const double t_d) {
      to_Number() = t_d;
    }

    constexpr value(const bool t_b) {
      to_Boolean() = t_b;
    }

    constexpr value(const std::monostate) {
      type = Type::Null;
    }

    constexpr value(cx::static_string t_s) {
      to_String() = cx::string(std::move(t_s));
    }

    constexpr value(cx::string t_s) {
      to_String() = std::move(t_s);
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

    constexpr void assert_type(Type t) const
    {
      if (type != t) throw std::runtime_error("Incorrect type");
    }

    constexpr bool is_Null() const
    {
      return type == Type::Null;
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

  // A value_proxy provides an interface to the value, decoupling the external
  // storage.
  template <size_t NumObjects, typename T>
  struct value_proxy
  {
    constexpr auto operator[](const cx::string& s) const {
      auto idx = object_storage[index].to_Object().at(s);
      return value_proxy{idx, object_storage};
    }
    constexpr auto operator[](const cx::string& s) {
      auto idx = object_storage[index].to_Object().at(s);
      return value_proxy{idx, object_storage};
    }
    constexpr auto operator[](const cx::static_string &s) const {
      return operator[](cx::string(s));
    }
    constexpr auto operator[](const cx::static_string &s) {
      return operator[](cx::string(s));
    }
    constexpr auto operator[](std::size_t idx) const {
      auto i = object_storage[index].to_Array()[idx];
      return value_proxy{i, object_storage};
    }
    constexpr auto operator[](std::size_t idx) {
      auto i = object_storage[index].to_Array()[idx];
      return value_proxy{i, object_storage};
    }

    constexpr auto is_Null() const { return object_storage[index].is_Null(); }
    constexpr decltype(auto) to_String() const { return object_storage[index].to_String(); }
    constexpr decltype(auto) to_String() { return object_storage[index].to_String(); }
    constexpr decltype(auto) to_Number() const { return object_storage[index].to_Number(); }
    constexpr decltype(auto) to_Number() { return object_storage[index].to_Number(); }
    constexpr decltype(auto) to_Boolean() const { return object_storage[index].to_Boolean(); }
    constexpr decltype(auto) to_Boolean() { return object_storage[index].to_Boolean(); }

    std::size_t index;
    T& object_storage;
  };

  template <size_t NumObjects>
  value_proxy(std::size_t i, const cx::vector<value, NumObjects>& v)
    -> value_proxy<NumObjects, const cx::vector<value, NumObjects>>;

  template <size_t NumObjects>
  value_proxy(std::size_t i, cx::vector<value, NumObjects>& v)
    -> value_proxy<NumObjects, cx::vector<value, NumObjects>>;

}
