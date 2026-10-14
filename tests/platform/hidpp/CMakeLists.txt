add_executable(ghub_platform_hidpp_tests
    HidppChannelPolicyTest.cpp
    HidrawDeviceTest.cpp)
target_link_libraries(ghub_platform_hidpp_tests PRIVATE ghub_platform_hidpp ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_platform_hidpp_tests)
gtest_discover_tests(ghub_platform_hidpp_tests TEST_PREFIX "platform.hidpp.")
