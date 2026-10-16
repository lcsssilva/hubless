add_executable(ghub_audio_processing_domain_tests
    domain/EqualizerCurveTest.cpp
    domain/EqualizerPresetsTest.cpp
    domain/VoiceThresholdTest.cpp)
target_link_libraries(ghub_audio_processing_domain_tests PRIVATE ghub_audio_processing_domain GTest::gtest_main)
ghub_set_warnings(ghub_audio_processing_domain_tests)
gtest_discover_tests(ghub_audio_processing_domain_tests TEST_PREFIX "audio_processing.")

include(${CMAKE_CURRENT_SOURCE_DIR}/application/tests.cmake OPTIONAL)
if(GHUB_BUILD_APP)
    include(${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/tests.cmake OPTIONAL)
    include(${CMAKE_CURRENT_SOURCE_DIR}/presentation/tests.cmake OPTIONAL)
endif()
