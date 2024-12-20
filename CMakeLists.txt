project(uw)

set(CMAKE_C_COMPILER clang-16)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -pedantic -Werror -Wno-gnu -Wno-unused-parameter -Wno-format-pedantic)

find_package(ICU COMPONENTS uc)

add_library(uw STATIC
    src/uw_alloc_debug.c
    src/uw_alloc_std.c
    src/uw_base.c
    src/uw_bool.c
    src/uw_charptr.c
    src/uw_struct.c
    src/uw_compound.c
    src/uw_file.c
    src/uw_float.c
    src/uw_hash.c
    src/uw_int.c
    src/uw_list.c
    src/uw_map.c
    src/uw_null.c
    src/uw_signed.c
    src/uw_status.c
    src/uw_string.c
    src/uw_string_io.c
    src/uw_unsigned.c
)

target_include_directories(uw PUBLIC . include)

# test

add_executable(test_uw test/test_uw.c)

target_link_libraries(test_uw uw)

if(DEFINED ICU_FOUND AND NOT DEFINED ENV{UW_WITHOUT_ICU})
    target_link_libraries(test_uw ICU::uc)
endif()

# common definitions

set(common_defs_targets uw test_uw)

foreach(TARGET ${common_defs_targets})

    if(DEFINED ENV{DEBUG})
        target_compile_definitions(${TARGET} PUBLIC DEBUG)
    endif()

    if(DEFINED ENV{UW_TYPE_CAPACITY})
        target_compile_definitions(${TARGET} PUBLIC UW_TYPE_CAPACITY=$ENV{UW_TYPE_CAPACITY})
    endif()
    if(DEFINED ENV{UW_INTERFACE_CAPACITY})
        target_compile_definitions(${TARGET} PUBLIC UW_INTERFACE_CAPACITY=$ENV{UW_INTERFACE_CAPACITY})
    endif()

    if(DEFINED ICU_FOUND AND NOT DEFINED ENV{UW_WITHOUT_ICU})
        target_compile_definitions(${TARGET} PUBLIC UW_WITH_ICU)
    endif()

endforeach(TARGET)
