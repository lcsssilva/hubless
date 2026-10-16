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

add_executable(ghub_app_settings_tests AppSettingsTest.cpp)
set_target_properties(ghub_app_settings_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_app_settings_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_app_settings_tests)
add_test(NAME app.AppSettingsTest COMMAND ghub_app_settings_tests)
set_tests_properties(app.AppSettingsTest PROPERTIES TIMEOUT 30)

add_executable(ghub_low_battery_notifier_tests LowBatteryNotifierTest.cpp)
set_target_properties(ghub_low_battery_notifier_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_low_battery_notifier_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_low_battery_notifier_tests)
add_test(NAME app.LowBatteryNotifierTest COMMAND ghub_low_battery_notifier_tests)
set_tests_properties(app.LowBatteryNotifierTest PROPERTIES TIMEOUT 30)

add_executable(ghub_tray_menu_model_tests TrayMenuModelTest.cpp)
set_target_properties(ghub_tray_menu_model_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_tray_menu_model_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_tray_menu_model_tests)
add_test(NAME app.TrayMenuModelTest COMMAND ghub_tray_menu_model_tests)
set_tests_properties(app.TrayMenuModelTest PROPERTIES TIMEOUT 30)

add_executable(ghub_config_backup_tests ConfigBackupTest.cpp)
set_target_properties(ghub_config_backup_tests PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_config_backup_tests PRIVATE ghub_app_support Qt6::Test)
ghub_set_warnings(ghub_config_backup_tests)
add_test(NAME app.ConfigBackupTest COMMAND ghub_config_backup_tests)
set_tests_properties(app.ConfigBackupTest PROPERTIES TIMEOUT 30)
