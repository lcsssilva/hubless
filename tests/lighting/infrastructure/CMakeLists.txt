add_executable(ghub_lighting_infrastructure_tests
    HidppCommandPolicyTest.cpp
    HidppChannelTest.cpp
    HidppCapabilitiesTest.cpp
    HidrawDeviceTest.cpp
    HidppNodeLocatorTest.cpp
    G915PerKeyFramesTest.cpp
    G915EffectFramesTest.cpp
    G915HidppSessionTest.cpp
    G915HidppLightingTest.cpp)
target_include_directories(ghub_lighting_infrastructure_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ghub_lighting_infrastructure_tests PRIVATE
    ghub_lighting_infrastructure ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_lighting_infrastructure_tests)
gtest_discover_tests(ghub_lighting_infrastructure_tests TEST_PREFIX "lighting.")
