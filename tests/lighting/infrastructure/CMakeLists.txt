add_executable(ghub_lighting_infrastructure_tests
    LightingHidppPolicyTest.cpp
    HidppChannelTest.cpp
    HidppCapabilitiesTest.cpp
    G915PerKeyFramesTest.cpp
    G915EffectFramesTest.cpp
    G915HidppSessionTest.cpp
    G915HidppControlSessionTest.cpp
    G915HidppLightingTest.cpp
    G915HidppControlTest.cpp
    HidppKeyboardStatusSourceTest.cpp)
target_include_directories(ghub_lighting_infrastructure_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ghub_lighting_infrastructure_tests PRIVATE
    ghub_lighting_infrastructure ghub_platform_hidpp ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_lighting_infrastructure_tests)
gtest_discover_tests(ghub_lighting_infrastructure_tests TEST_PREFIX "lighting.")

# INC-021 R4: portal ScreenCast e fonte de cores da tela, só com fakes (nunca o portal nem a tela reais).
if(GHUB_BUILD_APP)
    add_executable(ghub_lighting_screen_tests ScreenCastTest.cpp QtDBusScreenCastPortalTest.cpp)
    target_link_libraries(ghub_lighting_screen_tests PRIVATE
        ghub_lighting_screen_infrastructure ghub_test_fakes GTest::gtest_main)
    ghub_set_warnings(ghub_lighting_screen_tests)
    gtest_discover_tests(ghub_lighting_screen_tests TEST_PREFIX "lighting.Screen." PROPERTIES TIMEOUT 30)
endif()

# INC-021 R4: fluxo de vídeo só contra o daemon PipeWire privado (helper do INC-010).
if(GHUB_BUILD_APP)
    add_executable(ghub_lighting_screen_pw_tests PipeWireScreenStreamIntegrationTest.cpp)
    target_include_directories(ghub_lighting_screen_pw_tests PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/audio_processing/infrastructure")
    target_link_libraries(ghub_lighting_screen_pw_tests PRIVATE
        ghub_lighting_screen_infrastructure GTest::gtest_main)
    ghub_set_warnings(ghub_lighting_screen_pw_tests)
    gtest_discover_tests(ghub_lighting_screen_pw_tests TEST_PREFIX "lighting.ScreenPw." PROPERTIES TIMEOUT 60)
endif()
