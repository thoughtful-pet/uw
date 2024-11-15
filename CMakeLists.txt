project(uw)

set(CMAKE_C_COMPILER clang-16)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-gnu -Wno-unused-parameter -Wno-format-pedantic)

find_package(ICU COMPONENTS uc)

add_library(uw STATIC
    src/uw_base.c
    src/uw_hash.c
    src/uw_ctype.c
    src/uw_null.c
    src/uw_bool.c
    src/uw_int.c
    src/uw_float.c
    src/uw_string.c
    src/uw_list.c
    src/uw_map.c
    src/uw_class.c
    src/uw_file.c
    src/uw_string_io.c
    src/uw_tg.c
)

target_include_directories(uw PUBLIC . include)
target_compile_definitions(uw PUBLIC DEBUG)
target_compile_definitions(uw PUBLIC UW_TYPE_CAPACITY=16 UW_INTERFACE_CAPACITY=16)

if(DEFINED ICU_FOUND AND NOT DEFINED ENV{UW_WITHOUT_ICU})
    target_compile_definitions(uw PUBLIC UW_WITH_ICU)
endif()

add_executable(test_uw test/test_uw.c)

target_compile_definitions(test_uw PUBLIC DEBUG)
target_compile_definitions(test_uw PUBLIC UW_TYPE_CAPACITY=16 UW_INTERFACE_CAPACITY=16)
target_link_libraries(test_uw uw)

if(DEFINED ICU_FOUND AND NOT DEFINED ENV{UW_WITHOUT_ICU})
    target_compile_definitions(test_uw PUBLIC UW_WITH_ICU)
    target_link_libraries(test_uw ICU::uc)
endif()
