# Código de cola (main/QML) é coberto por teste de fumaça (ENGINEERING_RULES.md §2):
# o executável carrega o módulo QML em plataforma offscreen e sai com 0 somente se a janela foi criada.
add_test(NAME app.smoke_loads_main_qml COMMAND ghub-linux --smoke-test)
set_tests_properties(app.smoke_loads_main_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;XDG_CONFIG_HOME=${CMAKE_CURRENT_BINARY_DIR}/xdg-config"
    TIMEOUT 30)

find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
add_executable(ghub_app_termination_signals_tests TerminationSignalsTest.cpp)
set_target_properties(ghub_app_termination_signals_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_app_termination_signals_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_app_termination_signals_tests)
add_test(NAME app.TerminationSignalsTest COMMAND ghub_app_termination_signals_tests)
set_tests_properties(app.TerminationSignalsTest PROPERTIES TIMEOUT 30)

add_executable(ghub_app_autostart_tests AutostartTest.cpp)
set_target_properties(ghub_app_autostart_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_app_autostart_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_app_autostart_tests)
add_test(NAME app.AutostartTest COMMAND ghub_app_autostart_tests)
set_tests_properties(app.AutostartTest PROPERTIES TIMEOUT 30)

add_executable(ghub_app_desktop_controller_tests DesktopControllerTest.cpp)
set_target_properties(ghub_app_desktop_controller_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_app_desktop_controller_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_app_desktop_controller_tests)
add_test(NAME app.DesktopControllerTest COMMAND ghub_app_desktop_controller_tests)
set_tests_properties(app.DesktopControllerTest PROPERTIES TIMEOUT 30)
