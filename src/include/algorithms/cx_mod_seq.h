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

}
