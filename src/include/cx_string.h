#pragma once

#include <array>
#include <cstddef>

#include "cx_vector.h"

namespace cx
{
  struct static_string
  {
    template <std::size_t N>
    constexpr static_string(const char (&str)[N])
      : m_size(N-1), m_data(&str[0])
    {
    }
    constexpr static_string(const char* str, std::size_t s)
      : m_size(s), m_data(str)
    {
    }

    constexpr static_string() = default;

    constexpr size_t size() const {
      return m_size;
    }

    constexpr const char *c_str() const {
      return m_data;
    }

    std::size_t m_size{0};
    const char *m_data = nullptr;
  };

  constexpr bool operator==(const static_string &x, const static_string &y)
  {
    if (x.m_size != y.m_size)
      return false;

    std::size_t i = 0;
    while (i < x.m_size && x.m_data[i] == y.m_data[i]) { ++i; }
    return i == x.m_size;
  }

  template <std::size_t N = 32>
  using string = vector<char, N>;

}
