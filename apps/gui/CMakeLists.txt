find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
qt_add_executable(mininids_gui main.cpp AppController.cpp AppController.hpp DataModel.cpp DataModel.hpp)
target_compile_definitions(mininids_gui PRIVATE "MININIDS_VERSION=\"${MININIDS_PACKAGE_LABEL}\"")
set_target_properties(mininids_gui PROPERTIES
  OUTPUT_NAME "Mini-NIDS"
  WIN32_EXECUTABLE TRUE
  MACOSX_BUNDLE TRUE
  MACOSX_BUNDLE_GUI_IDENTIFIER "org.mininids.MiniNIDS"
  MACOSX_BUNDLE_BUNDLE_NAME "Mini-NIDS"
  MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
  MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}")
if(APPLE)
  set_target_properties(mininids_gui PROPERTIES INSTALL_RPATH "@executable_path/../Frameworks")
elseif(UNIX)
  set_target_properties(mininids_gui PROPERTIES INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()
if(WIN32)
  configure_file(${PROJECT_SOURCE_DIR}/packaging/windows/version.rc.in version.rc @ONLY)
  target_sources(mininids_gui PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/version.rc)
endif()
target_link_libraries(mininids_gui PRIVATE mininids_core Qt6::Quick Qt6::QuickControls2)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(mininids_gui URI MiniNids VERSION 1.0
  QML_FILES qml/Main.qml qml/Theme.qml qml/StatCard.qml qml/DataPage.qml qml/Dashboard.qml qml/ArchivePicker.qml)
qt_add_resources(mininids_gui fonts PREFIX /fonts BASE ${PROJECT_SOURCE_DIR}/assets/fonts
  FILES ${PROJECT_SOURCE_DIR}/assets/fonts/InterVariable.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-Regular.ttf)
install(FILES ${PROJECT_SOURCE_DIR}/assets/fonts/Inter-OFL.txt
              ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-OFL.txt
        DESTINATION ${MININIDS_DATA_DIR}/licenses/fonts)
install(TARGETS mininids_gui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} BUNDLE DESTINATION .)
if(APPLE)
  install(FILES ${PROJECT_SOURCE_DIR}/packaging/macos/Mini-NIDS.entitlements
    DESTINATION Mini-NIDS.app/Contents/Resources)
endif()
if(MININIDS_DEPLOY_RUNTIME)
  set(deploy_options NO_TRANSLATIONS)
  if(UNIX AND NOT APPLE)
    # Bundle non-Qt dependencies too. Keep the OS loader, glibc and graphics drivers native.
    list(APPEND deploy_options POST_EXCLUDE_REGEXES
      "/ld-linux[^/]*\\.so" "/lib(c|m|mvec|anl|pthread|dl|rt|resolv|util|BrokenLocale|nss_[^/]*)\\.so"
      "/lib(GL[^/]*|EGL[^/]*|OpenGL|gbm|drm[^/]*|vulkan[^/]*)\\.so")
  endif()
  qt_generate_deploy_qml_app_script(TARGET mininids_gui OUTPUT_SCRIPT deploy_script ${deploy_options})
  install(SCRIPT ${deploy_script})
endif()
if(MININIDS_BUILD_TESTS)
  find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Test)
  qt_add_executable(mininids_gui_tests ../../tests/GuiTest.cpp AppController.cpp AppController.hpp DataModel.cpp DataModel.hpp)
  target_include_directories(mininids_gui_tests PRIVATE . ../../tests)
  target_link_libraries(mininids_gui_tests PRIVATE mininids_core Qt6::Test Qt6::Gui Qt6::Qml miniz::miniz)
  add_test(NAME Gui.Controller COMMAND mininids_gui_tests)
  add_test(NAME Gui.QmlSmoke COMMAND mininids_gui --smoke-test)
  set_tests_properties(Gui.QmlSmoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software")
  if(WIN32)
    set_property(TEST Gui.QmlSmoke APPEND PROPERTY ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
  endif()
endif()
