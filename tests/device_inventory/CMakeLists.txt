add_executable(ghub_device_inventory_domain_tests
    domain/DeviceCatalogTest.cpp
    domain/HidDeviceNameTest.cpp)
target_link_libraries(ghub_device_inventory_domain_tests PRIVATE ghub_device_inventory_domain GTest::gtest_main)
ghub_set_warnings(ghub_device_inventory_domain_tests)
gtest_discover_tests(ghub_device_inventory_domain_tests TEST_PREFIX "device_inventory.")

add_executable(ghub_device_inventory_application_tests
    application/ListDetectedDevicesTest.cpp)
target_link_libraries(ghub_device_inventory_application_tests PRIVATE
    ghub_device_inventory_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_device_inventory_application_tests)
gtest_discover_tests(ghub_device_inventory_application_tests TEST_PREFIX "device_inventory.")

add_executable(ghub_device_inventory_infrastructure_tests
    infrastructure/SysfsHidDiscoveryTest.cpp
    infrastructure/SysfsVideoDiscoveryTest.cpp)
target_link_libraries(ghub_device_inventory_infrastructure_tests PRIVATE
    ghub_device_inventory_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_device_inventory_infrastructure_tests)
gtest_discover_tests(ghub_device_inventory_infrastructure_tests TEST_PREFIX "device_inventory.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_device_inventory_presentation_tests presentation/DeviceInventoryViewModelTest.cpp)
    set_target_properties(ghub_device_inventory_presentation_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_device_inventory_presentation_tests PRIVATE
        ghub_device_inventory_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_device_inventory_presentation_tests)
    add_test(NAME device_inventory.DeviceInventoryViewModelTest COMMAND ghub_device_inventory_presentation_tests)
    set_tests_properties(device_inventory.DeviceInventoryViewModelTest PROPERTIES TIMEOUT 30)
endif()
