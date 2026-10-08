find_package(GTest CONFIG REQUIRED)
find_package(wxWidgets REQUIRED COMPONENTS core base aui html xml xrc propgrid stc adv)
include(${wxWidgets_USE_FILE})

# CDMS source files under test (non-UI utility modules)
set(CDMS_SRC_DIR ${CMAKE_SOURCE_DIR}/src/cdms)

add_executable(de_archive_stub dearchive_stub.cpp)
set_target_properties(de_archive_stub PROPERTIES OUTPUT_NAME Archive
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/stubs")

add_executable(cdms_tests
    placeholder_test.cpp
    strconv_test.cpp
    config_test.cpp
    strings_test.cpp
    utility_test.cpp
    result_test.cpp
    fileservice_test.cpp
    moduleservice_test.cpp
    formatservice_test.cpp
    hashservice_test.cpp
    cancellationtoken_test.cpp
    progresschannel_test.cpp
    taskrunner_test.cpp
    moduleloadpresenter_test.cpp
    sgamakepresenter_test.cpp
    ucstodatpresenter_test.cpp
    dpscalculatorpresenter_test.cpp
    luainherittreepresenter_test.cpp
    rgdmacropresenter_test.cpp
    massextractpresenter_test.cpp
    refreshfilespresenter_test.cpp
    imagepresenter_test.cpp
    modulesettingspresenter_test.cpp
    ucseditorphresenter_test.cpp
    scareditorpresenter_test.cpp
    filetreepresenter_test.cpp
    rgdeditorpresenter_test.cpp
    relictoolresolver_test.cpp
    modbrowser_test.cpp
    demodgenerator_test.cpp
    dearchivetool_test.cpp
    ${CDMS_SRC_DIR}/common/strconv.cpp
    ${CDMS_SRC_DIR}/common/strings.cpp
    ${CDMS_SRC_DIR}/common/Utility.cpp
    ${CDMS_SRC_DIR}/common/config.cpp
    ${CDMS_SRC_DIR}/common/ThemeColours.cpp
    ${CDMS_SRC_DIR}/services/ModuleService.cpp
    ${CDMS_SRC_DIR}/services/FileService.cpp
    ${CDMS_SRC_DIR}/services/FormatService.cpp
    ${CDMS_SRC_DIR}/services/HashService.cpp
    ${CDMS_SRC_DIR}/services/CDEModGenerator.cpp
    ${CDMS_SRC_DIR}/async/CProgressChannel.cpp
    ${CDMS_SRC_DIR}/async/CTaskRunner.cpp
    ${CDMS_SRC_DIR}/async/CWxTaskRunner.cpp
    ${CDMS_SRC_DIR}/presenters/CModuleLoadPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CModBrowserPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CSgaMakePresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CUcsToDatPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CDpsCalculatorPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CRgdMacroPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CMassExtractPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CRefreshFilesPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CImagePresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CModuleSettingsPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CUcsEditorPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CScarEditorPresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CFileTreePresenter.cpp
    ${CDMS_SRC_DIR}/presenters/CRgdEditorPresenter.cpp
    ${CDMS_SRC_DIR}/tools/Tool_AutoDPS.cpp
    ${CDMS_SRC_DIR}/tools/Tool_AESetup.cpp
    ${CDMS_SRC_DIR}/tools/CDeArchiveTool.cpp
    ${CDMS_SRC_DIR}/views/CtrlStatusText.cpp
    ${CDMS_SRC_DIR}/frame/RelicToolResolver.cpp
)
target_precompile_headers(cdms_tests PRIVATE
    "${CDMS_SRC_DIR}/common/pch_cdms.h")

add_dependencies(cdms_tests de_archive_stub)
target_compile_definitions(cdms_tests PRIVATE DE_ARCHIVE_STUB_PATH="$<TARGET_FILE:de_archive_stub>")

target_include_directories(cdms_tests PRIVATE
    ${CDMS_SRC_DIR}
    ${CMAKE_BINARY_DIR}/generated
    ${wxWidgets_INCLUDE_DIRS}
)

target_link_libraries(cdms_tests PRIVATE
    rainman
    lsp
    GTest::gtest
    GTest::gtest_main
    ${wxWidgets_LIBRARIES}
)

# wxWidgets STC requires lexilla when statically linked
if(WXLEXILLA_LIB_RELEASE)
    target_link_libraries(cdms_tests PRIVATE
        $<IF:$<CONFIG:Debug>,${WXLEXILLA_LIB_DEBUG},${WXLEXILLA_LIB_RELEASE}>
    )
endif()

target_compile_definitions(cdms_tests PRIVATE
    _CRT_SECURE_NO_WARNINGS
    _CRT_NONSTDC_NO_DEPRECATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
)

if(MSVC)
    target_compile_options(cdms_tests PRIVATE
        /W3
        /wd4996
        /wd4267
        /wd4244
    )
endif()

include(GoogleTest)
gtest_discover_tests(cdms_tests)
