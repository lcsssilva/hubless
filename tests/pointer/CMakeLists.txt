add_executable(ghub_pointer_domain_tests
    domain/ResolutionStageTest.cpp
    domain/ButtonMappingTest.cpp
    domain/LedStateTest.cpp
    domain/PointerProfileTest.cpp
    domain/PointerConfigurationTest.cpp
    domain/G703ProfileSettingsTest.cpp
    domain/LedSettingTest.cpp
    domain/OnboardButtonsTest.cpp
    domain/OnboardPointerProfileTest.cpp
    domain/PointerSensorStatusTest.cpp)
target_link_libraries(ghub_pointer_domain_tests PRIVATE ghub_pointer_domain ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_pointer_domain_tests)
gtest_discover_tests(ghub_pointer_domain_tests TEST_PREFIX "pointer.")

add_executable(ghub_pointer_application_tests
    application/ReadPointerConfigurationTest.cpp
    application/G703OnboardSettingsTest.cpp
    application/ReadPointerSensorStatusTest.cpp)
target_link_libraries(ghub_pointer_application_tests PRIVATE
    ghub_pointer_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_pointer_application_tests)
gtest_discover_tests(ghub_pointer_application_tests TEST_PREFIX "pointer.")

add_executable(ghub_pointer_api_tests api/G703DpiSettingsTest.cpp)
target_link_libraries(ghub_pointer_api_tests PRIVATE ghub_pointer_api ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_pointer_api_tests)
gtest_discover_tests(ghub_pointer_api_tests TEST_PREFIX "pointer.")

add_executable(ghub_pointer_hidpp_tests
    infrastructure/G703OnboardPolicyTest.cpp
    infrastructure/G703NodeLocatorTest.cpp
    infrastructure/HidppPointerSensorSourceTest.cpp
    infrastructure/OnboardPointerBackendTest.cpp)
target_link_libraries(ghub_pointer_hidpp_tests PRIVATE ghub_pointer_hidpp ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_pointer_hidpp_tests)
gtest_discover_tests(ghub_pointer_hidpp_tests TEST_PREFIX "pointer.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus)
    add_executable(ghub_pointer_infrastructure_tests
        infrastructure/main.cpp
        infrastructure/RatbagPointerBackendTest.cpp)
    target_link_libraries(ghub_pointer_infrastructure_tests PRIVATE
        ghub_pointer_infrastructure Qt6::Core Qt6::DBus GTest::gtest)
    ghub_set_warnings(ghub_pointer_infrastructure_tests)
    gtest_discover_tests(ghub_pointer_infrastructure_tests TEST_PREFIX "pointer."
        PROPERTIES TIMEOUT 60 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_pointer_background_tests infrastructure/BackgroundPointerReaderTest.cpp)
    set_target_properties(ghub_pointer_background_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_background_tests PRIVATE ghub_pointer_background ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_pointer_background_tests)
    add_test(NAME pointer.BackgroundPointerReaderTest COMMAND ghub_pointer_background_tests)
    set_tests_properties(pointer.BackgroundPointerReaderTest PROPERTIES TIMEOUT 30)

    add_executable(ghub_pointer_viewmodel_tests presentation/PointerViewModelTest.cpp)
    set_target_properties(ghub_pointer_viewmodel_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_viewmodel_tests PRIVATE ghub_pointer_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_pointer_viewmodel_tests)
    add_test(NAME pointer.PointerViewModelTest COMMAND ghub_pointer_viewmodel_tests)
    set_tests_properties(pointer.PointerViewModelTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(ghub_pointer_onboard_controller_tests presentation/G703OnboardControllerTest.cpp)
    set_target_properties(ghub_pointer_onboard_controller_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_onboard_controller_tests PRIVATE
        ghub_pointer_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_pointer_onboard_controller_tests)
    add_test(NAME pointer.G703OnboardControllerTest COMMAND ghub_pointer_onboard_controller_tests)
    set_tests_properties(pointer.G703OnboardControllerTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")

    add_executable(ghub_pointer_sensor_status_viewmodel_tests presentation/PointerSensorStatusViewModelTest.cpp)
    set_target_properties(ghub_pointer_sensor_status_viewmodel_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_pointer_sensor_status_viewmodel_tests PRIVATE
        ghub_pointer_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_pointer_sensor_status_viewmodel_tests)
    add_test(NAME pointer.PointerSensorStatusViewModelTest COMMAND ghub_pointer_sensor_status_viewmodel_tests)
    set_tests_properties(pointer.PointerSensorStatusViewModelTest PROPERTIES TIMEOUT 30)
endif()

add_test(NAME pointer.read_only_source
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/read_only_source.py" "${PROJECT_SOURCE_DIR}")

