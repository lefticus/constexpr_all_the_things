#pragma once

#include <optional>
#include <utility>

namespace cx
{
  template <typename T>
  struct optional
  {
    using value_type = T;

    constexpr optional(std::nullopt_t) {}
    constexpr explicit optional(const T& t)
      : m_valid(true), m_t(t)
    {}

    constexpr explicit operator bool() const { return m_valid; }

    constexpr const T* operator->() const { return &m_t; }
    constexpr T* operator->() { return &m_t; }
    constexpr const T& operator*() const { return m_t; }
    constexpr T& operator*() { return m_t; }

  private:
    bool m_valid = false;
    T m_t{};
  };

  template <typename T>
  constexpr auto make_optional(T&& t)
  {
    return optional<T>(std::forward<T>(t));
  }
}
