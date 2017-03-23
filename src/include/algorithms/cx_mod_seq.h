#pragma once

namespace cx
{
  template <class InputIt, class OutputIt>
  constexpr OutputIt copy(InputIt first, InputIt last,
                          OutputIt d_first)
  {
    while (first != last) {
      *d_first++ = *first++;
    }
    return d_first;
  }

  template <class InputIt, class OutputIt, class UnaryPredicate>
  constexpr OutputIt copy_if(InputIt first, InputIt last,
                             OutputIt d_first, UnaryPredicate pred)
  {
    while (first != last) {
      if (pred(*first)) {
        *d_first++ = *first;
      }
      ++first;
    }
    return d_first;
  }

  template <class InputIt, class Size, class OutputIt>
  constexpr OutputIt copy_n(InputIt first, Size count, OutputIt result)
  {
    if (count > 0) {
      *result++ = *first;
      for (Size i = 1; i < count; ++i) {
        *result++ = *++first;
      }
    }
    return result;
  }

  template <class BidirIt1, class BidirIt2>
  constexpr BidirIt2 copy_backward(BidirIt1 first, BidirIt1 last, BidirIt2 d_last)
  {
    while (first != last) {
      *(--d_last) = *(--last);
    }
    return d_last;
  }

  template <class InputIt, class OutputIt>
  constexpr OutputIt move(InputIt first, InputIt last, OutputIt d_first)
  {
    while (first != last) {
      *d_first++ = std::move(*first++);
    }
    return d_first;
  }

  template <class BidirIt1, class BidirIt2>
  constexpr BidirIt2 move_backward(BidirIt1 first,
                                   BidirIt1 last,
                                   BidirIt2 d_last)
  {
    while (first != last) {
      *(--d_last) = std::move(*(--last));
    }
    return d_last;
  }

  template <class ForwardIt, class T>
  constexpr void fill(ForwardIt first, ForwardIt last, const T& value)
  {
    for (; first != last; ++first) {
      *first = value;
    }
  }

  template <class OutputIt, class Size, class T>
  constexpr OutputIt fill_n(OutputIt first, Size count, const T& value)
  {
    for (Size i = 0; i < count; i++) {
      *first++ = value;
    }
    return first;
  }

}
