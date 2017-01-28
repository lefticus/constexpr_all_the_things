#pragma once

#include <cstddef>

namespace cx
{
  struct string
  {
    template <std::size_t N>
    constexpr string(const char (&str)[N])
      : m_size(N-1), m_data(&str[0])
    {
    }

    constexpr string() = default;

    constexpr size_t size() const {
      return m_size;
    }

    constexpr const char *c_str() const {
      return m_data;
    }

    std::size_t m_size{0};
    const char *m_data = nullptr;
  };

  constexpr bool operator==(const string &x, const string &y)
  {
    if (x.m_size != y.m_size)
      return false;

    std::size_t i = 0;
    while (i < x.m_size && x.m_data[i] == y.m_data[i]) { ++i; }
    return i == x.m_size;
  }
}
