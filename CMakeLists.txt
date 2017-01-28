cmake_minimum_required (VERSION 2.8)
project (constexpr-all-the-things)

# Includes for this project
include_directories ("${PROJECT_SOURCE_DIR}/src/include")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Default C++ standard: C++1z
if(CXX_STD)
else()
  set(CXX_STD 1z)
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(MY_CXX_FLAGS_LIST
    )
  string(REPLACE ";" " " MY_CXX_FLAGS "${MY_CXX_FLAGS_LIST}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${MY_CXX_FLAGS}")
else()
  set(MY_CXX_FLAGS_LIST
    -ftemplate-backtrace-limit=0
    -ffunction-sections
    -Wall -Wextra -Werror -pedantic-errors
    -Wcast-align
    -Wcast-qual
    -Wctor-dtor-privacy
    -Wdisabled-optimization
    -Wformat=2
    -Winit-self
    -Wmissing-include-dirs
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wshadow
    -Wsign-conversion
    -Wsign-promo
    -Wstrict-overflow=5
    -Wswitch-default
    -Wundef
    )
  string(REPLACE ";" " " MY_CXX_FLAGS "${MY_CXX_FLAGS_LIST}")

  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++${CXX_STD} ${MY_CXX_FLAGS}")

  # Debug/Release
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -fno-inline -g3 -fstack-protector-all")
  set(CMAKE_CXX_FLAGS_RELEASE "-Ofast -g0 -march=native -mtune=native -DNDEBUG")
  set(CMAKE_CXX_FLAGS_COVERAGE "${CMAKE_CXX_FLAGS_DEBUG} -fprofile-arcs -ftest-coverage")
endif()

# Clang/GCC specifics
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(SAN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address,undefined,integer -fno-omit-frame-pointer -fno-sanitize=unsigned-integer-overflow")
  endif()
elseif(CMAKE_COMPILER_IS_GNUCXX)
endif()

add_subdirectory (src/test)
