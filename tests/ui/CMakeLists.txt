# Qt Quick Test (offscreen) de src/ui/qml/Main.qml com ViewModel falso em QML:
# verifica a ligação QML <-> ViewModel sem sysfs nem hardware.
find_package(Qt6 6.12 REQUIRED COMPONENTS Qml Quick QuickTest)

add_executable(ghub_ui_tests main.cpp)
set_target_properties(ghub_ui_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_ui_tests PRIVATE Qt6::Qml Qt6::Quick Qt6::QuickTest ghub_camera_presentation)
target_compile_definitions(ghub_ui_tests PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
ghub_set_warnings(ghub_ui_tests)

add_test(NAME ui.main_qml COMMAND ghub_ui_tests)
set_tests_properties(ui.main_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    TIMEOUT 30)
