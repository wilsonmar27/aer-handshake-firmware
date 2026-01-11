cmake_minimum_required(VERSION 3.13)

# Common (platform-agnostic) AER library
add_library(aer_common
    src/aer_burst.c
    src/aer_codec.c
    src/ringbuf.c
)

# Expose public headers to anything that links this library
target_include_directories(aer_common
    PUBLIC
        ${CMAKE_CURRENT_LIST_DIR}/include
)

# Keep the common lib pure C (works fine even if linked into C++ projects)
set_target_properties(aer_common PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
)

# Optional: good baseline warnings for this library
if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(aer_common PRIVATE -Wall -Wextra -Wpedantic)
elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(aer_common PRIVATE /W4)
endif()
