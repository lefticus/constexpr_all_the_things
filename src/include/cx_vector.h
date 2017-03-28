#pragma once

#include "cx_algorithm.h"
#include "cx_iterator.h"
#include "cx_allocator.h"

#include <array>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <memory>

namespace cx
{
  template <typename T, typename = void>
    struct has_fixed_size_flag : public std::false_type{};

  template <typename T>
    struct has_fixed_size_flag<T, std::void_t<typename T::is_fixed_size>> : public std::true_type {};

  template <typename Value, typename Allocator = cx::allocator<Value, 5>>
  class basic_vector
  {
  public:
    using iterator = Value *;
    using const_iterator = const Value *;
    using value_type = Value;
    using reference = Value &;
    using const_reference = const Value &;

    constexpr basic_vector()
    {
      if constexpr (Allocator::is_fixed_size) {
        m_data_ptr = m_alloc.allocate(Allocator::fixed_size);
        m_alloced_size = Allocator::fixed_size;
      }
    }

    template<typename Itr>
    constexpr basic_vector(Itr begin, const Itr &end)
      : basic_vector()
    {
      while (begin != end) {
        push_back(*begin);
        ++begin;
      }
    }
    constexpr basic_vector(std::initializer_list<Value> init)
      : basic_vector(init.begin(), init.end())
    {
    }

    template<typename OtherAllocator>
    constexpr basic_vector(const basic_vector<Value, OtherAllocator> &other) 
      : basic_vector(other.begin(), other.end())
    {
    }

    
    template<typename OtherAllocator>
    constexpr basic_vector(basic_vector<Value, OtherAllocator> &&other)
      : basic_vector(std::make_move_iterator(other.begin()), std::make_move_iterator(other.end()))
    {
    }

    template<typename OtherAllocator>
    constexpr basic_vector &operator=(basic_vector<Value, OtherAllocator> &&other)
    {
      // note this is not going to work with a non-constexpr allocator
      m_size = 0;

      for (auto &&o : std::move(other)) {
        push_back(std::move(o));
      }
    }

    template<typename OtherAllocator>
    constexpr basic_vector &operator=(const basic_vector<Value, OtherAllocator> &other)
    {
      // note this is not going to work with a non-constexpr allocator
      m_size = 0;

      for (const auto &o : other) {
        push_back(o);
      }
    }


    constexpr auto begin() const { return m_data_ptr; }
    constexpr auto begin() { return m_data_ptr; }

    // We would have prefered to use `std::next`, however it does not seem
    // to be enabled for constexpr use for std::array in this version
    // of gcc. TODO: reevaluate this
    constexpr auto end() const { return m_data_ptr + m_size; }
    constexpr auto end() { return m_data_ptr + m_size; }

    constexpr auto cbegin() const { return m_data_ptr; }
    constexpr auto cend() const { return m_data_ptr + m_size; }

    constexpr const Value &operator[](const std::size_t t_pos) const {
      return m_data_ptr[t_pos];
    }
    constexpr Value &operator[](const std::size_t t_pos) {
      return m_data_ptr[t_pos];
    }

    constexpr Value &at(const std::size_t t_pos) {
      if (t_pos >= m_size) {
        // This is allowed in constexpr context, but if the constexpr evaluation
        // hits this exception the compile would fail
        throw std::range_error("Index past end of basic_vector");
      } else {
        return m_data_ptr[t_pos];
      }
    }
    constexpr const Value &at(const std::size_t t_pos) const {
      if (t_pos >= m_size) {
        throw std::range_error("Index past end of basic_vector");
      } else {
        return m_data_ptr[t_pos];
      }
    }

    constexpr Value& push_back(Value t_v) {
      if (m_size == m_alloced_size) {
        const auto new_alloced_size = m_size + 1;
        auto new_data_ptr = m_alloc.allocate(new_alloced_size);

        std::uninitialized_move(m_data_ptr, m_data_ptr + m_size, new_data_ptr);
        m_alloc.deallocate(m_data_ptr, m_size);

        ++m_size;
        m_alloced_size = new_alloced_size;
        m_data_ptr = new_data_ptr;
        return m_data_ptr[m_size];
      } else {
        Value& v = m_data_ptr[m_size++];
        v = std::move(t_v);
        return v;
      }
    }

    constexpr const Value &back() const {
      if (empty()) {
        throw std::range_error("Index past end of basic_vector");
      } else {
        return m_data_ptr[m_size - 1];
      }
    }
    constexpr Value &back() {
      if (empty()) {
        throw std::range_error("Index past end of basic_vector");
      } else {
        return m_data_ptr[m_size - 1];
      }
    }

    constexpr auto capacity() const { return m_alloced_size; }
    constexpr auto size() const { return m_size; }
    constexpr auto empty() const { return m_size == 0; }

    constexpr void clear() { m_size = 0; }

    constexpr const Value* data() const {
      return m_data_ptr;
    }

  private:
    Allocator m_alloc{};
    Value *m_data_ptr{nullptr};
    std::size_t m_alloced_size{0};
    std::size_t m_size{0};
  };

  template<typename T, size_t S>
  using vector = basic_vector<T, allocator<T, S>>;


  template<typename T, typename Alloc1, typename Alloc2>
  constexpr bool operator==(const basic_vector<T, Alloc1> &x, const basic_vector<T, Alloc2> &y)
  {
    return cx::equal(x.begin(), x.end(), y.begin(), y.end());
  }


  // Is addition (concatenation) on strings useful? Not sure yet. But it does
  // allow us to carry the type information properly.

  template<typename Value, size_t Size1, size_t Size2>
  constexpr auto operator+(const basic_vector<Value, cx::allocator<Value, Size1>> &a, const basic_vector<Value, cx::allocator<Value, Size2>> &b) {
    basic_vector<Value, cx::allocator<Value, Size1 + Size2>> v;
    copy(a.cbegin(), a.cend(), back_insert_iterator(v));
    copy(b.cbegin(), b.cend(), back_insert_iterator(v));
    return v;
  }

}
