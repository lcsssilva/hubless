add_executable(ghub_lighting_domain_tests
    EffectParametersTest.cpp
    KeyboardEffectTest.cpp
    HeadsetEffectTest.cpp
    G915KeyCatalogTest.cpp
    KeyColorMapTest.cpp
    KeyboardStatusTest.cpp
    AnimationFramesTest.cpp
    ScreenRegionsTest.cpp
    GameModeKeysTest.cpp
    StoredEffectTest.cpp
    G915ProfileSettingsTest.cpp)
target_link_libraries(ghub_lighting_domain_tests PRIVATE ghub_lighting_domain ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_lighting_domain_tests)
gtest_discover_tests(ghub_lighting_domain_tests TEST_PREFIX "lighting.")
