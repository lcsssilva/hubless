add_executable(ghub_audio_processing_application_tests
    ${CMAKE_CURRENT_LIST_DIR}/DefaultPreferenceTest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/HostEqualizerTest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/HostNoiseSuppressionTest.cpp)
target_link_libraries(ghub_audio_processing_application_tests PRIVATE
    ghub_audio_processing_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_audio_processing_application_tests)
gtest_discover_tests(ghub_audio_processing_application_tests TEST_PREFIX "audio_processing.")
