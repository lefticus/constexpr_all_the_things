#pragma once

// necessary because std::pair seems to be disabling the move assignment for
// some unclear reason. It's not clear if this is a bug in the gcc impl

namespace cx
{
  template <typename First, typename Second>
  struct pair
  {
    using first_type = First;
    using second_type = Second;
    First first;
    Second second;
  };

  template <typename First, typename Second>
  constexpr auto make_pair(First f, Second s)
  {
    return pair<First, Second>{f, s};
  }
}
