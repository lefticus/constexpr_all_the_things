#include <cx_algorithm.h>
#include <cx_map.h>
#include <cx_pair.h>
#include <cx_string.h>
#include <cx_vector.h>
#include <cx_json_value.h>


constexpr auto get_json_value()
{
  cx::JSON_Value<> j;
  j["a"].to_Number() = 15;
  j["b"].to_String() = "Hello World";
  j["d"].to_Array();
  j["c"]["a"]["b"].to_Array().push_back(10);
  j["c"]["a"]["c"] = cx::string("Hello World");
  j["c"]["a"]["d"].to_Array().push_back(5.2);
  return j;
}


int main(int, char *[])
{

  constexpr auto json_value = get_json_value();
  static_assert(json_value["c"]["a"]["d"][0].to_Number() == 5.2);

  return 0;
}



