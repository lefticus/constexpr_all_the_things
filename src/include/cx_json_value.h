#pragma once

#include "cx_algorithm.h"
#include "cx_map.h"
#include "cx_string.h"
#include "cx_vector.h"

#include <string_view>
#include <variant>

namespace JSON
{

  // ---------------------------------------------------------------------------
  // non-recursive definition of a JSON value

  struct value
  {
    struct ExternalView
    {
      std::size_t offset;
      std::size_t extent;
    };

    union Data
    {
      std::string_view unparsed;
      ExternalView external;
      double number;
      bool boolean;

      constexpr Data() : boolean(false) {}
      constexpr Data(const std::string_view& sv) : unparsed(sv) {}
      constexpr Data(bool b) : boolean(b) {}
      constexpr Data(double d) : number(d) {}
      constexpr Data(const ExternalView& ev) : external(ev) {}
    };

    enum class Type
    {
      Unparsed,
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

    constexpr value(const std::string_view& extent)
      : type(Type::Unparsed), data(extent)
    {}

    constexpr value(const double t_d) {
      to_Number() = t_d;
    }

    constexpr value(const bool t_b) {
      to_Boolean() = t_b;
    }

    constexpr value(const std::monostate) {
      type = Type::Null;
    }

    constexpr value(ExternalView t_s) {
      to_String() = std::move(t_s);
    }

    constexpr decltype(auto) to_Object() const
    {
      assert_type(Type::Object);
      return (data.external);
    }

    constexpr decltype(auto) to_Object()
    {
      if (type != Type::Object) {
        type = Type::Object;
        data = Data(ExternalView{0,0});
      }
      return (data.external);
    }

    // objects are stored contiguously in storage as alternate string, value,
    // string, value, etc
    constexpr auto object_Size() const
    {
      assert_type(Type::Object);
      return data.external.extent / 2;
    }

    constexpr void assert_type(Type t) const
    {
      if (type != t) throw std::runtime_error("Incorrect type");
    }

    constexpr bool is_Null() const
    {
      return type == Type::Null;
    }

    constexpr void to_Null()
    {
      if (type != Type::Null) {
        type = Type::Null;
        data = Data{};
      }
    }

    constexpr const std::string_view& to_Unparsed() const
    {
      assert_type(Type::Unparsed);
      return data.unparsed;
    }

    constexpr std::string_view& to_Unparsed()
    {
      if (type != Type::Unparsed) {
        type = Type::Unparsed;
        data = Data(std::string_view{});
      }
      return data.unparsed;
    }

    constexpr const ExternalView& to_Array() const
    {
      assert_type(Type::Array);
      return data.external;
    }

    constexpr ExternalView& to_Array()
    {
      if (type != Type::Array) {
        type = Type::Array;
        data = Data(ExternalView{0,0});
      }
      return data.external;
    }

    constexpr auto array_Size() const
    {
      assert_type(Type::Array);
      return data.external.extent;
    }

    constexpr const ExternalView& to_String() const
    {
      assert_type(Type::String);
      return data.external;
    }

    constexpr ExternalView& to_String()
    {
      if (type != Type::String) {
        type = Type::String;
        data = Data(ExternalView{0,0});
      }
      return data.external;
    }

    constexpr auto string_Size() const
    {
      assert_type(Type::String);
      return data.external.extent;
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
        data = Data(0.0);
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
        data = Data(false);
      }
      return data.boolean;
    }
  };

  // A value_proxy provides an interface to the value, decoupling the external
  // storage.
  template <size_t NumObjects, typename T, typename S>
  struct value_proxy
  {
    // Using a transparent comparison operator will allow us to index by any
    // kind of "string" (cx::static_string, cx::string, etc)
    struct StringCompare
    {
      template <typename S1, typename S2>
      constexpr bool operator()(const S1& s1, const S2& s2) {
        return cx::equal(std::cbegin(s1), std::cend(s1),
                         std::cbegin(s2), std::cend(s2));
      }

      // const char arrays are tricky because their length includes the null
      // terminator, so we use N-1 as the length
      template <typename S1, std::size_t N>
      constexpr bool operator()(const char (&s2)[N], const S1& s1) {
        return cx::equal(std::cbegin(s1), std::cend(s1),
                         s2, &s2[N-1]);
      }
      template <typename S1, std::size_t N>
      constexpr bool operator()(const S1& s1, const char (&s2)[N]) {
        return cx::equal(std::cbegin(s1), std::cend(s1),
                         s2, &s2[N-1]);
      }
    };

    // The use of "notfound" in these functions serves no purpose; but if the
    // throw expression is not guarded, it is evaluated, even though the
    // function returns early and it should be unevaluated. And the final return
    // here is because clang complains about control flow reaching the end of
    // this non-void function, although it never will.
    template <typename K,
              std::enable_if_t<!std::is_integral<K>::value, int> = 0>
    constexpr auto operator[](const K& s) const {
      const auto& ext = object_storage[index].to_Object();
      bool notfound = true;
      for (auto i = ext.offset; i < ext.offset + ext.extent; i += 2) {
        const auto& str = object_storage[i].to_String();
        cx::static_string k { &string_storage[str.offset], str.extent };
        if (StringCompare{}(k, s))
          return value_proxy{i+1, object_storage, string_storage};
      }
      if (notfound) throw std::runtime_error("Key not found in object");
      return value_proxy{0, object_storage, string_storage};
    }
    template <typename K,
              std::enable_if_t<!std::is_integral<K>::value, int> = 0>
    constexpr auto operator[](const K& s) {
      const auto& ext = object_storage[index].to_Object();
      bool notfound = true;
      for (auto i = ext.offset; i < ext.offset + ext.extent; i += 2) {
        const auto& str = object_storage[i].to_String();
        cx::static_string k { &string_storage[str.offset], str.extent };
        if (StringCompare{}(k, s))
          return value_proxy{i+1, object_storage, string_storage};
      }
      if (notfound) throw std::runtime_error("Key not found in object");
      return value_proxy{0, object_storage, string_storage};
    }
    constexpr auto object_Size() const {
      return object_storage[index].object_Size();
    }

    constexpr auto operator[](std::size_t idx) const {
      auto& ext = object_storage[index].to_Array();
      if (idx > ext.extent) throw std::runtime_error("Index past end of array");
      return value_proxy{ext.offset + idx, object_storage, string_storage};
    }
    constexpr auto operator[](std::size_t idx) {
      auto& ext = object_storage[index].to_Array();
      if (idx > ext.extent) throw std::runtime_error("Index past end of array");
      return value_proxy{ext.offset + idx, object_storage, string_storage};
    }
    constexpr auto array_Size() const {
      return object_storage[index].array_Size();
    }

    constexpr auto is_Null() const { return object_storage[index].is_Null(); }

    constexpr auto to_String() const {
      auto s = object_storage[index].to_String();
      return cx::static_string { &string_storage[s.offset], s.extent };
    }
    constexpr auto to_String() {
      auto s = object_storage[index].to_String();
      return cx::static_string { &string_storage[s.offset], s.extent };
    }
    constexpr auto string_Size() const {
      return object_storage[index].string_Size();
    }

    constexpr decltype(auto) to_Number() const { return object_storage[index].to_Number(); }
    constexpr decltype(auto) to_Number() { return object_storage[index].to_Number(); }

    constexpr decltype(auto) to_Boolean() const { return object_storage[index].to_Boolean(); }
    constexpr decltype(auto) to_Boolean() { return object_storage[index].to_Boolean(); }

    std::size_t index;
    T& object_storage;
    S& string_storage;
  };

  template <size_t NumObjects, size_t StringSize>
  value_proxy(std::size_t i, const value(&v)[NumObjects],
              const cx::basic_string<char, StringSize>& s)
    -> value_proxy<NumObjects, const value(&)[NumObjects],
                   const cx::basic_string<char, StringSize>>;

  template <size_t NumObjects, size_t StringSize>
  value_proxy(std::size_t i, value(&v)[NumObjects],
              cx::basic_string<char, StringSize>& s)
    -> value_proxy<NumObjects, value(&)[NumObjects],
                   cx::basic_string<char, StringSize>>;

}
