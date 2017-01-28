#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace cx
{
  template <typename Value, std::size_t Size = 5>
  class vector
  {
  public:
    constexpr auto begin() const { return m_data.begin(); }
    constexpr auto begin() { return m_data.begin(); }

    constexpr auto end() const { return m_data.begin() + m_size; }
    constexpr auto end() { return m_data.begin() + m_size; }

    constexpr auto cbegin() const { return m_data.begin(); }
    constexpr auto cend() const { m_data.begin() + m_size; }

    constexpr const Value &operator[](const std::size_t t_pos) const {
      return m_data[t_pos];
    }
    constexpr Value &operator[](const std::size_t t_pos) {
      return m_data[t_pos];
    }

    constexpr Value &at(const std::size_t t_pos) {
      if (t_pos >= m_size) {
        throw std::range_error("Index past end of vector");
      } else {
        return m_data[t_pos];
      }
    }
    constexpr const Value &at(const std::size_t t_pos) const {
      if (t_pos >= m_size) {
        throw std::range_error("Index past end of vector");
      } else {
        return m_data[t_pos];
      }
    }

    constexpr void push_back(Value t_v) {
      m_data[m_size++] = std::move(t_v);
    }

  private:
    std::array<Value, Size> m_data;
    std::size_t m_size{0};
  };
}
