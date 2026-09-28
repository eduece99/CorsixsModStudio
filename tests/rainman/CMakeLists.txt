find_package(GTest CONFIG REQUIRED)

add_executable(rainman_tests
    test_main.cpp
    placeholder_test.cpp
    exception_test.cpp
    memorystore_test.cpp
    crc32_case_idt_test.cpp
    filesystemstore_test.cpp
    sgafile_test.cpp
    rgdfile_test.cpp
    ucsfile_test.cpp
    chunkyfile_test.cpp
    modulefile_test.cpp
    dowde_module_loading_test.cpp
    rgdhashtable_test.cpp
    inherittable_test.cpp
    internal_util_test.cpp
    md5_test.cpp
    callbacks_test.cpp
    writetime_test.cpp
    directorytraverser_test.cpp
    rgmfile_test.cpp
    cbfxfile_test.cpp
    sgacreator_test.cpp
    luafile2_test.cpp
    luascript_test.cpp
    luafile_test.cpp
    luafromrgd_test.cpp
    rgtfile_test.cpp
    exceptiondeleter_test.cpp
    rgdfilemacro_test.cpp
    filemap_parallel_test.cpp
    filemap_output_test.cpp
    luafilecache_parallel_test.cpp
    parallel_loading_test.cpp
    threadpool_test.cpp
)

target_link_libraries(rainman_tests
    PRIVATE
        rainman
        GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(rainman_tests)
