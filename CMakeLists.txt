project(uw)

set(CMAKE_C_COMPILER clang-16)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

find_package(ICU COMPONENTS uc REQUIRED)

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

add_executable(test_uw test/test_uw.c)

target_compile_definitions(test_uw PUBLIC DEBUG)
target_link_libraries(test_uw uw ICU::uc)
