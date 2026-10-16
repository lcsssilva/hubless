add_subdirectory(domain)
add_subdirectory(application)
add_subdirectory(infrastructure)

add_executable(ghub_lighting_api_tests api/KeyboardEffectSettingsTest.cpp)
target_link_libraries(ghub_lighting_api_tests PRIVATE ghub_lighting_api GTest::gtest_main)
ghub_set_warnings(ghub_lighting_api_tests)
gtest_discover_tests(ghub_lighting_api_tests TEST_PREFIX "lighting.")

add_test(NAME lighting.no_forbidden_feature_source
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/no_forbidden_feature_source.py" "${PROJECT_SOURCE_DIR}")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_lighting_presentation_tests presentation/KeyboardLightingControllerTest.cpp)
    set_target_properties(ghub_lighting_presentation_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_presentation_tests PRIVATE
        ghub_lighting_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_lighting_presentation_tests)
    add_test(NAME lighting.KeyboardLightingControllerTest COMMAND ghub_lighting_presentation_tests)
    set_tests_properties(lighting.KeyboardLightingControllerTest PROPERTIES TIMEOUT 30)
endif()

if(GHUB_BUILD_APP)
    add_executable(ghub_lighting_status_viewmodel_tests presentation/KeyboardStatusViewModelTest.cpp)
    set_target_properties(ghub_lighting_status_viewmodel_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_status_viewmodel_tests PRIVATE
        ghub_lighting_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_lighting_status_viewmodel_tests)
    add_test(NAME lighting.KeyboardStatusViewModelTest COMMAND ghub_lighting_status_viewmodel_tests)
    set_tests_properties(lighting.KeyboardStatusViewModelTest PROPERTIES TIMEOUT 30)
endif()

if(GHUB_BUILD_APP)
    add_executable(ghub_lighting_animation_controller_tests presentation/SoftwareAnimationControllerTest.cpp)
    set_target_properties(ghub_lighting_animation_controller_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_animation_controller_tests PRIVATE
        ghub_lighting_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_lighting_animation_controller_tests)
    add_test(NAME lighting.SoftwareAnimationControllerTest COMMAND ghub_lighting_animation_controller_tests)
    set_tests_properties(lighting.SoftwareAnimationControllerTest PROPERTIES TIMEOUT 60)
endif()

if(GHUB_BUILD_APP)
    add_executable(ghub_lighting_controls_controller_tests presentation/KeyboardControlsControllerTest.cpp)
    set_target_properties(ghub_lighting_controls_controller_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_lighting_controls_controller_tests PRIVATE
        ghub_lighting_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_lighting_controls_controller_tests)
    add_test(NAME lighting.KeyboardControlsControllerTest COMMAND ghub_lighting_controls_controller_tests)
    set_tests_properties(lighting.KeyboardControlsControllerTest PROPERTIES TIMEOUT 30)
endif()
