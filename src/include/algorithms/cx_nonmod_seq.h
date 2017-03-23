#pragma once

#include "cx_pair.h"

#include <iterator>

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

  template <class InputIt, class UnaryPredicate>
  constexpr InputIt find_if_not(InputIt first, InputIt last, UnaryPredicate p)
  {
    for (; first != last; ++first) {
      if (!p(*first)) {
        return first;
      }
    }
    return last;
  }

  template <class InputIt, class UnaryPredicate>
  constexpr bool all_of(InputIt first, InputIt last, UnaryPredicate p)
  {
    return cx::find_if_not(first, last, p) == last;
  }

  template <class InputIt, class UnaryPredicate>
  constexpr bool any_of(InputIt first, InputIt last, UnaryPredicate p)
  {
    return cx::find_if(first, last, p) != last;
  }

  template <class InputIt, class UnaryPredicate>
  constexpr bool none_of(InputIt first, InputIt last, UnaryPredicate p)
  {
    return cx::find_if(first, last, p) == last;
  }

  template <class InputIt, class T>
  constexpr typename std::iterator_traits<InputIt>::difference_type
  count(InputIt first, InputIt last, const T& value)
  {
    typename std::iterator_traits<InputIt>::difference_type ret = 0;
    for (; first != last; ++first) {
      if (*first == value) {
        ret++;
      }
    }
    return ret;
  }

  template <class InputIt, class UnaryPredicate>
  constexpr typename std::iterator_traits<InputIt>::difference_type
  count_if(InputIt first, InputIt last, UnaryPredicate p)
  {
    typename std::iterator_traits<InputIt>::difference_type ret = 0;
    for (; first != last; ++first) {
      if (p(*first)) {
        ret++;
      }
    }
    return ret;
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

  // for_each compiles as constexpr, but I can't see a use for it... we can't
  // carry a state inside the UnaryFunction if it's constexpr, and we can't
  // really do anything to a constexpr capture (see the useless test code)

  template <class InputIt, class UnaryFunction>
  constexpr UnaryFunction for_each(InputIt first, InputIt last, UnaryFunction f)
  {
    for (; first != last; ++first) {
      f(*first);
    }
    return f;
  }

  // for_each_n also compiles but is similarly useless as constexpr... an
  // expensive way to do something like std::next?

  template <class InputIt, class Size, class UnaryFunction>
  constexpr InputIt for_each_n(InputIt first, Size n, UnaryFunction f)
  {
    for (Size i = 0; i < n; ++first, (void) ++i) {
      f(*first);
    }
    return first;
  }

  template <class ForwardIt1, class ForwardIt2>
  constexpr ForwardIt1 search(ForwardIt1 first, ForwardIt1 last,
                              ForwardIt2 s_first, ForwardIt2 s_last)
  {
    for (; ; ++first) {
      ForwardIt1 it = first;
      for (ForwardIt2 s_it = s_first; ; ++it, ++s_it) {
        if (s_it == s_last) {
          return first;
        }
        if (it == last) {
          return last;
        }
        if (!(*it == *s_it)) {
          break;
        }
      }
    }
  }

  template <class ForwardIt1, class ForwardIt2>
  constexpr ForwardIt1 find_end(ForwardIt1 first, ForwardIt1 last,
                                ForwardIt2 s_first, ForwardIt2 s_last)
  {
    if (s_first == s_last)
      return last;
    ForwardIt1 result = last;
    while (1) {
      ForwardIt1 new_result = cx::search(first, last, s_first, s_last);
      if (new_result == last) {
        return result;
      } else {
        result = new_result;
        first = result;
        ++first;
      }
    }
    return result;
  }

  template <class InputIt, class ForwardIt>
  constexpr InputIt find_first_of(InputIt first, InputIt last,
                                  ForwardIt s_first, ForwardIt s_last)
  {
    for (; first != last; ++first) {
      for (ForwardIt it = s_first; it != s_last; ++it) {
        if (*first == *it) {
          return first;
        }
      }
    }
    return last;
  }


  // this implementation of search_n (from cppreference.com) has a constexpr
  // problem - my version of GCC gives the error: constexpr loop iteration count
  // exceeds limit of 262144 (use -fconstexpr-loop-limit= to increase the limit)
  template <class ForwardIt, class Size, class T>
  constexpr ForwardIt bad_search_n(ForwardIt first, ForwardIt last,
                                   Size count, const T& value)
  {
    for(; first != last; ++first) {
      if (!(*first == value)) {
        continue;
      }

      ForwardIt candidate = first;
      Size cur_count = 0;

      while (true) {
        ++cur_count;
        if (cur_count == count) {
          // success
          return candidate;
        }
        ++first;
        if (first == last) {
          // exhausted the list
          return last;
        }
        if (!(*first == value)) {
          // too few in a row
          break;
        }
      }
    }
    return last;
  }

  // this implementation works - maybe the nested loop and/or continue of the
  // other version is confusing the compiler somehow?
  template <class ForwardIt, class Size, class T>
  constexpr ForwardIt search_n(ForwardIt first, ForwardIt last,
                               Size count, const T& value)
  {
    for(; first != last; ++first) {
      ForwardIt candidate = first;
      Size cur_count = 0;

      while (*first == value) {
        ++cur_count;
        if (cur_count == count) {
          // success
          return candidate;
        }
        ++first;
        if (first == last) {
          // exhausted the list
          return last;
        }
      }
    }
    return last;
  }

  template <class ForwardIt>
  constexpr ForwardIt adjacent_find(ForwardIt first, ForwardIt last)
  {
    if (first == last) {
      return last;
    }
    ForwardIt next = first;
    ++next;
    for (; next != last; ++next, ++first) {
      if (*first == *next) {
        return first;
      }
    }
    return last;
  }

}
