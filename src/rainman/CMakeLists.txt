# Lua 5.0.2 - vendored static library
# Only the core library + standard libs (no lua/luac executables)
add_library(lua502 STATIC
    vendor/lua502/lapi.c
    vendor/lua502/lcode.c
    vendor/lua502/ldebug.c
    vendor/lua502/ldo.c
    vendor/lua502/ldump.c
    vendor/lua502/lfunc.c
    vendor/lua502/lgc.c
    vendor/lua502/llex.c
    vendor/lua502/lmem.c
    vendor/lua502/lobject.c
    vendor/lua502/lopcodes.c
    vendor/lua502/lparser.c
    vendor/lua502/lstate.c
    vendor/lua502/lstring.c
    vendor/lua502/ltable.c
    vendor/lua502/ltm.c
    vendor/lua502/lundump.c
    vendor/lua502/lvm.c
    vendor/lua502/lzio.c
    # Standard libraries
    vendor/lua502/lbaselib.c
    vendor/lua502/ldblib.c
    vendor/lua502/liolib.c
    vendor/lua502/lmathlib.c
    vendor/lua502/loadlib.c
    vendor/lua502/lstrlib.c
    vendor/lua502/ltablib.c
    vendor/lua502/lauxlib.c
)

target_include_directories(lua502 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua502)
target_compile_definitions(lua502 PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(lua502 PROPERTIES C_STANDARD 99)
# MSVC /Ot yields an invalid negative array size in Lua 5.0.2's table rehash
# on x64. Keep Release optimizations and favor size for this file only.
if(MSVC)
    set_source_files_properties(vendor/lua502/ltable.c PROPERTIES
        COMPILE_OPTIONS "$<$<CONFIG:Release>:/Os>")
endif()

# Lua 5.1.2 - vendored static library with renamed symbols
# All public symbols are prefixed lua51_ to avoid conflicts with Lua 5.0.2
add_library(lua512 STATIC
    vendor/lua512/lapi.c
    vendor/lua512/lauxlib.c
    vendor/lua512/lbaselib.c
    vendor/lua512/lcode.c
    vendor/lua512/ldblib.c
    vendor/lua512/ldebug.c
    vendor/lua512/ldo.c
    vendor/lua512/ldump.c
    vendor/lua512/lfunc.c
    vendor/lua512/lgc.c
    vendor/lua512/linit.c
    vendor/lua512/liolib.c
    vendor/lua512/llex.c
    vendor/lua512/lmathlib.c
    vendor/lua512/lmem.c
    vendor/lua512/loadlib.c
    vendor/lua512/lobject.c
    vendor/lua512/lopcodes.c
    vendor/lua512/loslib.c
    vendor/lua512/lparser.c
    vendor/lua512/lstate.c
    vendor/lua512/lstring.c
    vendor/lua512/lstrlib.c
    vendor/lua512/ltable.c
    vendor/lua512/ltablib.c
    vendor/lua512/ltm.c
    vendor/lua512/lundump.c
    vendor/lua512/lvm.c
    vendor/lua512/lzio.c
)

target_include_directories(lua512 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua512)
target_compile_definitions(lua512 PRIVATE _CRT_SECURE_NO_WARNINGS)
set_target_properties(lua512 PROPERTIES C_STANDARD 99)
# Force-include the rename header so all symbols get lua51_ prefix
if(MSVC)
    target_compile_options(lua512 PRIVATE /FI"lua512_rename.h")
else()
    target_compile_options(lua512 PRIVATE -include lua512_rename.h)
endif()

# Rainman library
find_package(ZLIB REQUIRED)
find_package(unofficial-libsquish CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

file(GLOB RAINMAN_SOURCES
    "*.cpp" "*.c"
    "core/*.cpp" "core/*.c"
    "util/*.cpp" "util/*.c"
    "io/*.cpp" "io/*.c"
    "localization/*.cpp" "localization/*.c"
    "archive/*.cpp" "archive/*.c"
    "formats/*.cpp" "formats/*.c"
    "lua/*.cpp" "lua/*.c"
    "module/*.cpp" "module/*.c"
)
# Exclude gnuc_defines.cpp on MSVC (it provides GNUC compatibility shims)
if(MSVC)
    list(FILTER RAINMAN_SOURCES EXCLUDE REGEX "gnuc_defines\\.cpp$")
endif()

add_library(rainman STATIC ${RAINMAN_SOURCES})

target_include_directories(rainman
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/vendor/lua502
)

target_link_libraries(rainman
    PUBLIC
        lua502
        lua512
        ZLIB::ZLIB
        spdlog::spdlog
    PRIVATE
        unofficial::libsquish::squish
)

if(WIN32)
    target_link_libraries(rainman PRIVATE shlwapi)
endif()

target_compile_definitions(rainman
    PUBLIC
        RAINMAN_NO_EXPORTS
    PRIVATE
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
)

if(MSVC)
    target_compile_options(rainman PRIVATE
        /W3
        /wd4251  # DLL interface warning (keeping for compatibility)
        /wd4996  # Deprecated CRT functions
    )
endif()

# Precompiled header for C++ sources (skip C files — PCH has C++ content)
set_source_files_properties(util/crc32_case_idt.c util/hash.c util/md5.c
    PROPERTIES SKIP_PRECOMPILE_HEADERS ON)
target_precompile_headers(rainman PRIVATE pch_rainman.h)
