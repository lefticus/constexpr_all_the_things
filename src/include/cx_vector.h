#pragma once

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>

#include "cx_algorithm.h"

namespace cx
{
  template <typename Value, std::size_t Size = 5>
  class vector
  {
    using storage_t = std::array<Value, Size>;
  public:
    using iterator = typename storage_t::iterator;
    using const_iterator = typename storage_t::const_iterator;
    using value_type = Value;
    using reference = typename storage_t::reference;
    using const_reference = typename storage_t::const_reference;

    constexpr auto begin() const { return m_data.begin(); }
    constexpr auto begin() { return m_data.begin(); }

    // We would have prefered to use `std::next`, however it does not seem
    // to be enabled for constexpr use for std::array in this version
    // of gcc. TODO: reevaluate this
    constexpr auto end() const { return m_data.begin() + m_size; }
    constexpr auto end() { return m_data.begin() + m_size; }

    constexpr auto cbegin() const { return m_data.begin(); }
    constexpr auto cend() const { return m_data.begin() + m_size; }

    constexpr const Value &operator[](const std::size_t t_pos) const {
      return m_data[t_pos];
    }
    constexpr Value &operator[](const std::size_t t_pos) {
      return m_data[t_pos];
    }

    constexpr Value &at(const std::size_t t_pos) {
      if (t_pos >= m_size) {
        // This is allowed in constexpr context, but if the constexpr evaluation
        // hits this exception the compile would fail
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
      if (m_size >= Size) {
        throw std::range_error("Index past end of vector");
      } else {
        m_data[m_size++] = std::move(t_v);
      }
    }

    constexpr auto size() const { return m_size; }
    constexpr auto empty() const { return m_size == 0; }

  private:
    storage_t m_data{};
    std::size_t m_size{0};
  };

  // Possibly std::back_insert_iterator could be made mostly constexpr?

  template <typename Container>
  struct back_insert_iterator
  {
    constexpr explicit back_insert_iterator(Container& c)
      : m_c(c) {}

    constexpr back_insert_iterator& operator=(const typename Container::value_type& value) {
      m_c.push_back(value);
      return *this;
    }

    constexpr back_insert_iterator& operator*() { return *this; }
    constexpr back_insert_iterator& operator++() { return *this; }
    constexpr back_insert_iterator& operator++(int) { return *this; }

    Container& m_c;
  };

  // Is addition (concatenation) on strings useful? Not sure yet. But it does
  // allow us to carry the type information properly.

  template <typename Value, std::size_t S1, std::size_t S2>
  constexpr auto operator+(vector<Value, S1> a, vector<Value, S2> b) {
    vector<Value, S1+S2> v;
    copy(a.cbegin(), a.cend(), back_insert_iterator(v));
    copy(b.cbegin(), b.cend(), back_insert_iterator(v));
    return v;
  }

}
