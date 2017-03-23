#include <cx_algorithm.h>
#include <cx_vector.h>

void algo_tests()
{

  {
    constexpr int arr[] = {1, 3, 5, 7, 9};
    constexpr bool b = cx::all_of(std::cbegin(arr), std::cend(arr),
                                  [] (auto i) { return i % 2 == 1; });
    static_assert(b, "all_of fail");
  }

  {
    constexpr int arr[] = {1, 3, 5, 8, 9};
    constexpr bool b = cx::any_of(std::cbegin(arr), std::cend(arr),
                                  [] (auto i) { return i % 2 == 0; });
    static_assert(b, "any_of fail");
  }

  {
    constexpr int arr[] = {1, 3, 5, 7, 9};
    constexpr bool b = cx::none_of(std::cbegin(arr), std::cend(arr),
                                   [] (auto i) { return i % 2 == 0; });
    static_assert(b, "none_of fail");
  }

  {
    // the next few lines compile (why?), but are useless...
    constexpr int arr[] = {1, 3, 5, 7, 9};
    constexpr cx::vector<int, 5> v;
    // if v is constexpr, we can capture a reference-to-v in a constexpr lambda,
    // and call a constexpr method on it
    constexpr auto f = cx::for_each(std::cbegin(arr), std::cend(arr),
                                    [&] (int i) { v.push_back(i+1); });
    // and do it as much as we like, even though it looks like a buffer overflow
    // here...
    f(11);
    f(13);
    f(15);
    f(17);
    f(19);
    // there is no effect
    static_assert(v.empty(), "for_each fail");
  }

  {
    constexpr int arr[] = {1, 3, 5, 7, 9};
    constexpr auto n = cx::count_if(std::cbegin(arr), std::cend(arr),
                                    [] (auto i) { return i % 2 == 1; });
    static_assert(n == 5, "count_if fail");
  }

  {
    using namespace std::literals;
    constexpr std::string_view s1 = "hello"sv;
    constexpr std::string_view s2 = "helllo"sv;
    constexpr auto p = cx::mismatch(s1.cbegin(), s1.cend(), s2.cbegin(), s2.cend());
    static_assert(p.first == s1.cend() - 1
                  && p.second == s2.cend() - 2, "mismatch fail");
  }

  {
    using namespace std::literals;
    constexpr std::string_view s1 = "hello"sv;
    constexpr std::string_view s2 = "hallo"sv;
    constexpr auto eq = cx::equal(s1.cbegin(), s1.cend(), s1.cbegin(), s1.cend());
    static_assert(eq, "equal fail");
    constexpr auto noteq = cx::equal(s1.cbegin(), s1.cend(), s2.cbegin(), s2.cend());
    static_assert(!noteq, "equal fail");
  }

  {
    using namespace std::literals;
    constexpr std::string_view haystack = "rhythmic"sv;
    constexpr std::string_view needles = "aeiou"sv;
    constexpr auto it = cx::find_first_of(haystack.cbegin(), haystack.cend(),
                                          needles.cbegin(), needles.cend());
    static_assert(it == haystack.cend() - 2, "find_first_of fail");
  }

  {
    using namespace std::literals;
    constexpr std::string_view haystack = "banana"sv;
    constexpr std::string_view subseq = "ana"sv;
    constexpr auto it = cx::find_end(haystack.cbegin(), haystack.cend(),
                                     subseq.cbegin(), subseq.cend());
    static_assert(it == haystack.cend() - 3, "find_end fail");
  }

  {
    using namespace std::literals;
    constexpr std::string_view haystack = "banana"sv;
    constexpr std::string_view subseq = "ana"sv;
    constexpr auto it = cx::search(haystack.cbegin(), haystack.cend(),
                                   subseq.cbegin(), subseq.cend());
    static_assert(it == haystack.cbegin() + 1, "search fail");
  }

  {
    using namespace std::literals;

    // see the comments on search_n and bad_search_n

    // starting at the beginning is fine
    constexpr std::string_view haystack = "111110"sv;
    constexpr auto it = cx::search_n(haystack.cbegin(), haystack.cend(),
                                     5, '1');
    static_assert(it == haystack.cbegin(), "search_n fail");

    // failing is fine
    constexpr auto it2 = cx::search_n(haystack.cbegin(), haystack.cend(),
                                     6, '1');
    static_assert(it2 == haystack.cend(), "search_n fail");

    // but as soon as the search has to skip even one character, bad_search_n
    // gives up the ghost
    constexpr std::string_view haystack2 = "011111"sv;
    constexpr auto it3 = cx::search_n(haystack2.cbegin(), haystack2.cend(),
                                      5, '1');
    static_assert(it3 == haystack2.cbegin()+1, "search_n fail");

    constexpr auto it4 = cx::search_n(haystack2.cbegin(), haystack2.cend(),
                                      6, '1');
    static_assert(it4 == haystack2.cend(), "search_n fail");

  }

  {
    using namespace std::literals;
    constexpr std::string_view haystack = "wildebeest"sv;
    constexpr auto it = cx::adjacent_find(haystack.cbegin(), haystack.cend());
    static_assert(it == haystack.cend() - 4, "adjacent_find fail");
  }
}
