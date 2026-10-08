# CDMS GUI application
find_package(wxWidgets REQUIRED COMPONENTS core base aui html xml xrc propgrid stc adv)
include(${wxWidgets_USE_FILE})

set(DEFAULT_BURN_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/res/_default.burn")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${DEFAULT_BURN_SOURCE}")
file(READ "${DEFAULT_BURN_SOURCE}" DEFAULT_BURN_TEMPLATE)
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
configure_file(res/DefaultBurnTemplate.h.in "${CMAKE_BINARY_DIR}/generated/DefaultBurnTemplate.h" @ONLY)

file(GLOB CDMS_SOURCES "*.cpp" "*.c" "common/*.cpp" "frame/*.cpp" "tools/*.cpp" "services/*.cpp" "actions/*.cpp" "async/*.cpp" "presenters/*.cpp" "views/*.cpp")

add_executable(ModStudioDE WIN32 ${CDMS_SOURCES} resource.rc)
target_precompile_headers(ModStudioDE PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/common/pch_cdms.h>")

target_include_directories(ModStudioDE PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(ModStudioDE PRIVATE
    rainman
    lsp
    ${wxWidgets_LIBRARIES}
)

# wxWidgets STC requires lexilla when statically linked; the vcpkg
# cmake wrapper doesn't discover it automatically.
find_library(WXLEXILLA_LIB_RELEASE NAMES wxlexilla PATHS "${wxWidgets_ROOT_DIR}/lib" NO_DEFAULT_PATH)
find_library(WXLEXILLA_LIB_DEBUG NAMES wxlexillad PATHS "${wxWidgets_ROOT_DIR}/debug/lib" NO_DEFAULT_PATH)
if(WXLEXILLA_LIB_RELEASE)
    target_link_libraries(ModStudioDE PRIVATE
        $<IF:$<CONFIG:Debug>,${WXLEXILLA_LIB_DEBUG},${WXLEXILLA_LIB_RELEASE}>
    )
endif()

if(WIN32)
    target_link_libraries(ModStudioDE PRIVATE
        winmm comctl32 rpcrt4 wsock32 oleacc odbc32
    )
endif()

target_compile_definitions(ModStudioDE PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(ModStudioDE PRIVATE
        /W3
        /wd4996
        /wd4267
        /wd4244
    )
    # Disable MSVC auto-generated manifest — wx.rc embeds its own with
    # Per-Monitor V2 DPI awareness (wxUSE_DPI_AWARE_MANIFEST=2).
    target_link_options(ModStudioDE PRIVATE /MANIFEST:NO)
endif()

# Copy Mod_Studio_Files (config, dictionaries, docs, etc.) next to the exe
add_custom_command(TARGET ModStudioDE POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/Mod_Studio_Files"
        "$<TARGET_FILE_DIR:ModStudioDE>/Mod_Studio_Files"
    COMMENT "Copying Mod_Studio_Files to output directory"
)

if(NOT WIN32)
    foreach(bitmap donate.bmp logo_smaller.bmp icon.ico)
        add_custom_command(TARGET ModStudioDE POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/res/${bitmap}"
                "$<TARGET_FILE_DIR:ModStudioDE>/${bitmap}"
            COMMENT "Copying ${bitmap} to output directory"
        )
    endforeach()
endif()

# Copy LuaLS binary (downloaded at configure time) into the output directory
if(LUALS_DIR AND EXISTS "${LUALS_DIR}/bin/lua-language-server.exe")
    add_custom_command(TARGET ModStudioDE POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${LUALS_DIR}"
            "$<TARGET_FILE_DIR:ModStudioDE>/Mod_Studio_Files/lsp/luals"
        COMMENT "Copying LuaLS to output directory"
    )
endif()

install(TARGETS ModStudioDE RUNTIME DESTINATION ".")
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Mod_Studio_Files/"
    DESTINATION "Mod_Studio_Files")
if(NOT WIN32)
    install(FILES res/donate.bmp res/logo_smaller.bmp res/icon.ico DESTINATION ".")
endif()
install(CODE "
    foreach(file bin/lua-language-server.exe main.lua script/client.lua LICENSE)
        if(NOT EXISTS \"${LUALS_DIR}/\${file}\")
            message(FATAL_ERROR \"Missing LuaLS release file: ${LUALS_DIR}/\${file}\")
        endif()
    endforeach()
")
install(DIRECTORY "${LUALS_DIR}/"
    DESTINATION "Mod_Studio_Files/lsp/luals")
install(FILES "${CMAKE_SOURCE_DIR}/LICENSE" "${CMAKE_SOURCE_DIR}/README.md"
    DESTINATION ".")
