add_executable(ghub_battery_domain_tests
    domain/BatteryLevelTest.cpp
    domain/BatteryReadingTest.cpp
    domain/AdcMeasurementTest.cpp)
target_link_libraries(ghub_battery_domain_tests PRIVATE ghub_battery_domain GTest::gtest_main)
ghub_set_warnings(ghub_battery_domain_tests)
gtest_discover_tests(ghub_battery_domain_tests TEST_PREFIX "battery.")

add_executable(ghub_battery_application_tests
    application/GetBatteryStatusTest.cpp)
target_link_libraries(ghub_battery_application_tests PRIVATE
    ghub_battery_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_battery_application_tests)
gtest_discover_tests(ghub_battery_application_tests TEST_PREFIX "battery.")

add_executable(ghub_battery_infrastructure_tests
    infrastructure/SysfsBatterySourceTest.cpp
    infrastructure/KernelUeventTest.cpp)
target_link_libraries(ghub_battery_infrastructure_tests PRIVATE ghub_battery_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_battery_infrastructure_tests)
gtest_discover_tests(ghub_battery_infrastructure_tests TEST_PREFIX "battery.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_battery_presentation_tests presentation/BatteryViewModelTest.cpp)
    set_target_properties(ghub_battery_presentation_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_presentation_tests PRIVATE
        ghub_battery_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_battery_presentation_tests)
    add_test(NAME battery.BatteryViewModelTest COMMAND ghub_battery_presentation_tests)
    set_tests_properties(battery.BatteryViewModelTest PROPERTIES TIMEOUT 30)

    add_executable(ghub_battery_uevent_tests infrastructure/UeventBatteryMonitorTest.cpp)
    set_target_properties(ghub_battery_uevent_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_uevent_tests PRIVATE ghub_battery_uevent Qt6::Test)
    ghub_set_warnings(ghub_battery_uevent_tests)
    add_test(NAME battery.UeventBatteryMonitorTest COMMAND ghub_battery_uevent_tests)
    set_tests_properties(battery.UeventBatteryMonitorTest PROPERTIES TIMEOUT 30)

    add_executable(ghub_battery_headsetcontrol_json_tests infrastructure/HeadsetControlJsonTest.cpp)
    target_link_libraries(ghub_battery_headsetcontrol_json_tests PRIVATE
        ghub_battery_headsetcontrol GTest::gtest_main)
    ghub_set_warnings(ghub_battery_headsetcontrol_json_tests)
    gtest_discover_tests(ghub_battery_headsetcontrol_json_tests TEST_PREFIX "battery.")

    add_executable(ghub_battery_headsetcontrol_source_tests infrastructure/HeadsetControlBatterySourceTest.cpp)
    set_target_properties(ghub_battery_headsetcontrol_source_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_headsetcontrol_source_tests PRIVATE
        ghub_battery_headsetcontrol ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_battery_headsetcontrol_source_tests)
    add_test(NAME battery.HeadsetControlBatterySourceTest COMMAND ghub_battery_headsetcontrol_source_tests)
    set_tests_properties(battery.HeadsetControlBatterySourceTest PROPERTIES TIMEOUT 60)
endif()

add_executable(ghub_battery_hidpp_tests infrastructure/HidppBatteryReaderTest.cpp)
target_link_libraries(ghub_battery_hidpp_tests PRIVATE ghub_battery_hidpp ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_battery_hidpp_tests)
gtest_discover_tests(ghub_battery_hidpp_tests TEST_PREFIX "battery.")

if(GHUB_BUILD_APP)
    add_executable(ghub_battery_hidpp_source_tests infrastructure/HidppBatterySourceTest.cpp)
    set_target_properties(ghub_battery_hidpp_source_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_battery_hidpp_source_tests PRIVATE
        ghub_battery_hidpp_polling ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_battery_hidpp_source_tests)
    add_test(NAME battery.HidppBatterySourceTest COMMAND ghub_battery_hidpp_source_tests)
    set_tests_properties(battery.HidppBatterySourceTest PROPERTIES TIMEOUT 60)
endif()
