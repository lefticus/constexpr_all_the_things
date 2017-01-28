#pragma once

// necessary because std::pair seems to be disabling the move assignment for
// some unclear reason

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
}
