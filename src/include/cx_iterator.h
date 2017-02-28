#pragma once

namespace cx
{

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

}
