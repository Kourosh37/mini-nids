find_package(Qt6 6.8.3 EXACT REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.8)
qt_add_executable(mininids_gui main.cpp AppController.cpp AppController.hpp DataModel.cpp DataModel.hpp)
target_link_libraries(mininids_gui PRIVATE mininids_core Qt6::Quick Qt6::QuickControls2)
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(mininids_gui URI MiniNids VERSION 1.0
  QML_FILES qml/Main.qml qml/Theme.qml qml/StatCard.qml qml/DataPage.qml qml/Dashboard.qml qml/ArchivePicker.qml)
qt_add_resources(mininids_gui fonts PREFIX /fonts BASE ${PROJECT_SOURCE_DIR}/assets/fonts
  FILES ${PROJECT_SOURCE_DIR}/assets/fonts/InterVariable.ttf
        ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-Regular.ttf)
install(FILES ${PROJECT_SOURCE_DIR}/assets/fonts/Inter-OFL.txt
              ${PROJECT_SOURCE_DIR}/assets/fonts/JetBrainsMono-OFL.txt
        DESTINATION share/mininids/licenses/fonts)
install(TARGETS mininids_gui RUNTIME DESTINATION bin BUNDLE DESTINATION .)
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
