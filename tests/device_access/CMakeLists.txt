add_executable(ghub_device_access_domain_tests
    domain/PhysicalInterfaceIdTest.cpp
    domain/AccessRegistryTest.cpp
    domain/AccessRegistryConcurrencyTest.cpp
    domain/ExternalOwnerTest.cpp)
target_link_libraries(ghub_device_access_domain_tests PRIVATE
    ghub_device_access_domain ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_device_access_domain_tests)
gtest_discover_tests(ghub_device_access_domain_tests TEST_PREFIX "device_access.")

add_executable(ghub_device_access_infrastructure_tests
    infrastructure/ProcExternalProcessProbeTest.cpp
    infrastructure/SteadyClockTest.cpp)
target_link_libraries(ghub_device_access_infrastructure_tests PRIVATE
    ghub_device_access_infrastructure ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_device_access_infrastructure_tests)
gtest_discover_tests(ghub_device_access_infrastructure_tests TEST_PREFIX "device_access.")

# Adaptador do logind (sinal PrepareForSleep + inhibitor): só com Qt.
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus Test)
    add_executable(ghub_device_access_logind_tests infrastructure/LogindSleepMonitorTest.cpp)
    set_target_properties(ghub_device_access_logind_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_device_access_logind_tests PRIVATE
        ghub_device_access_logind Qt6::Core Qt6::DBus Qt6::Test)
    ghub_set_warnings(ghub_device_access_logind_tests)
    add_test(NAME device_access.LogindSleepMonitorTest COMMAND ghub_device_access_logind_tests)
    set_tests_properties(device_access.LogindSleepMonitorTest PROPERTIES TIMEOUT 60
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
