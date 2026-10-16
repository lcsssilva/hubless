add_executable(ghub_headset_domain_tests
    domain/SidetoneLevelTest.cpp
    domain/HeadsetStatusTest.cpp)
target_link_libraries(ghub_headset_domain_tests PRIVATE ghub_headset_domain GTest::gtest_main)
ghub_set_warnings(ghub_headset_domain_tests)
gtest_discover_tests(ghub_headset_domain_tests TEST_PREFIX "headset.")

add_executable(ghub_headset_application_tests
    application/ChangeHeadsetSettingTest.cpp
    application/ReadHeadsetStatusTest.cpp)
target_link_libraries(ghub_headset_application_tests PRIVATE
    ghub_headset_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_headset_application_tests)
gtest_discover_tests(ghub_headset_application_tests TEST_PREFIX "headset.")

add_executable(ghub_headset_hidpp_tests
    infrastructure/G733StatusPolicyTest.cpp
    infrastructure/HidppHeadsetStatusSourceTest.cpp)
target_link_libraries(ghub_headset_hidpp_tests PRIVATE ghub_headset_hidpp ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_headset_hidpp_tests)
gtest_discover_tests(ghub_headset_hidpp_tests TEST_PREFIX "headset.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
    add_executable(ghub_headset_action_json_tests infrastructure/HeadsetControlActionJsonTest.cpp)
    target_link_libraries(ghub_headset_action_json_tests PRIVATE
        ghub_headset_infrastructure Qt6::Core GTest::gtest_main)
    ghub_set_warnings(ghub_headset_action_json_tests)
    gtest_discover_tests(ghub_headset_action_json_tests TEST_PREFIX "headset.")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_headset_writer_tests infrastructure/HeadsetControlSettingsWriterTest.cpp)
    set_target_properties(ghub_headset_writer_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_headset_writer_tests PRIVATE ghub_headset_infrastructure Qt6::Test)
    ghub_set_warnings(ghub_headset_writer_tests)
    add_test(NAME headset.HeadsetControlSettingsWriterTest COMMAND ghub_headset_writer_tests)
    set_tests_properties(headset.HeadsetControlSettingsWriterTest PROPERTIES TIMEOUT 60)
endif()

if(GHUB_BUILD_APP)
    add_executable(ghub_headset_controller_tests presentation/HeadsetSettingsControllerTest.cpp)
    set_target_properties(ghub_headset_controller_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_headset_controller_tests PRIVATE
        ghub_headset_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_headset_controller_tests)
    add_test(NAME headset.HeadsetSettingsControllerTest COMMAND ghub_headset_controller_tests)
    set_tests_properties(headset.HeadsetSettingsControllerTest PROPERTIES TIMEOUT 30)
endif()

if(GHUB_BUILD_APP)
    add_executable(ghub_headset_status_viewmodel_tests presentation/HeadsetStatusViewModelTest.cpp)
    set_target_properties(ghub_headset_status_viewmodel_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_headset_status_viewmodel_tests PRIVATE
        ghub_headset_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_headset_status_viewmodel_tests)
    add_test(NAME headset.HeadsetStatusViewModelTest COMMAND ghub_headset_status_viewmodel_tests)
    set_tests_properties(headset.HeadsetStatusViewModelTest PROPERTIES TIMEOUT 30)
endif()

add_test(NAME headset.allowed_arguments_source
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/allowed_arguments_source.py" "${PROJECT_SOURCE_DIR}")
add_test(NAME headset.allowed_arguments_source_selftest
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/allowed_arguments_source.py" --self-test)
