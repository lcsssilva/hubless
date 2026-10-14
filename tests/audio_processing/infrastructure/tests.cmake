add_executable(ghub_audio_processing_pw_pure_tests
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireFilterChainArgsTest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireGraphQueriesTest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/NoiseSuppressionArgsTest.cpp
    ${CMAKE_CURRENT_LIST_DIR}/NoiseSuppressionQueriesTest.cpp)
target_link_libraries(ghub_audio_processing_pw_pure_tests PRIVATE
    ghub_audio_processing_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_audio_processing_pw_pure_tests)
gtest_discover_tests(ghub_audio_processing_pw_pure_tests TEST_PREFIX "audio_processing.PipeWire"
    PROPERTIES TIMEOUT 30)

add_executable(ghub_audio_processing_pw_integration_tests
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireAudioGraphIntegrationTest.cpp)
target_link_libraries(ghub_audio_processing_pw_integration_tests PRIVATE
    ghub_audio_processing_infrastructure GTest::gtest)
ghub_set_warnings(ghub_audio_processing_pw_integration_tests)
gtest_discover_tests(ghub_audio_processing_pw_integration_tests TEST_PREFIX "audio_processing.PipeWireIntegration."
    PROPERTIES TIMEOUT 60)

# INC-010 parte 2: o plugin RNNoise roda dentro do processo do teste (LADSPA_PATH do próprio processo).
add_executable(ghub_audio_processing_noise_integration_tests
    ${CMAKE_CURRENT_LIST_DIR}/NoiseSuppressionIntegrationTest.cpp)
target_link_libraries(ghub_audio_processing_noise_integration_tests PRIVATE
    ghub_audio_processing_infrastructure GTest::gtest)
ghub_set_warnings(ghub_audio_processing_noise_integration_tests)
gtest_discover_tests(ghub_audio_processing_noise_integration_tests TEST_PREFIX "audio_processing.NoiseIntegration."
    PROPERTIES TIMEOUT 60)

# R9: o módulo só escreve o que o ADR-0016 permite (INC-010).
add_test(NAME audio_processing.allowed_writes_source
    COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tests/audio_processing/allowed_writes_source.py" "${PROJECT_SOURCE_DIR}")
add_test(NAME audio_processing.allowed_writes_source_selftest
    COMMAND Python3::Interpreter -I "${PROJECT_SOURCE_DIR}/tests/audio_processing/allowed_writes_source.py" --self-test)
set_tests_properties(audio_processing.allowed_writes_source audio_processing.allowed_writes_source_selftest
    PROPERTIES TIMEOUT 30)
