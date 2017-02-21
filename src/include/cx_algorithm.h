#pragma once

#include "cx_pair.h"

namespace cx
{
  // Necessary because C++17 does not contain a constexpr version of find_if
  // probably not for any technical reason
  template <class InputIt, class UnaryPredicate>
  constexpr InputIt find_if(InputIt first, InputIt last, UnaryPredicate p)
  {
    for (; first != last; ++first) {
      if (p(*first)) {
        return first;
      }
    }
    return last;
  }

  // And the same for all the functions here...

  template <class InputIt, class T>
  constexpr InputIt find(InputIt first, InputIt last, const T& value)
  {
    for (; first != last; ++first) {
      if (*first == value) {
        return first;
      }
    }
    return last;
  }

  template <class InputIt1, class InputIt2>
  constexpr cx::pair<InputIt1, InputIt2> mismatch(InputIt1 first1, InputIt1 last1,
                                                  InputIt2 first2, InputIt2 last2)
  {
    while (first1 != last1 && first2 != last2 && *first1 == *first2) {
      ++first1, ++first2;
    }
    return cx::pair<InputIt1, InputIt2>{first1, first2};
  }

  template <class InputIt1, class InputIt2>
  constexpr bool equal(InputIt1 first1, InputIt1 last1,
                       InputIt2 first2, InputIt2 last2)
  {
    while (first1 != last1 && first2 != last2 && *first1 == *first2) {
      ++first1, ++first2;
    }
    return first1 == last1 && first2 == last2;
  }

  template <class InputIt, class OutputIt>
  constexpr OutputIt copy(InputIt first, InputIt last,
                          OutputIt d_first)
  {
    while (first != last) {
        *d_first++ = *first++;
    }
    return d_first;
  }

}
