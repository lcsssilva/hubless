add_subdirectory(domain)
add_subdirectory(application)
add_subdirectory(infrastructure)

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
