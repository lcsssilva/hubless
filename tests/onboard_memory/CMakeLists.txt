add_executable(ghub_onboard_memory_domain_tests
    domain/OnboardSectorTest.cpp
    domain/OnboardDirectoryTest.cpp)
target_link_libraries(ghub_onboard_memory_domain_tests PRIVATE
    ghub_onboard_memory_domain ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_onboard_memory_domain_tests)
gtest_discover_tests(ghub_onboard_memory_domain_tests TEST_PREFIX "onboard_memory.")

add_executable(ghub_onboard_memory_application_tests
    application/ActiveSectorEditorTest.cpp)
target_link_libraries(ghub_onboard_memory_application_tests PRIVATE
    ghub_onboard_memory_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_onboard_memory_application_tests)
gtest_discover_tests(ghub_onboard_memory_application_tests TEST_PREFIX "onboard_memory.")

add_executable(ghub_onboard_memory_infrastructure_tests
    infrastructure/HidppOnboardMemoryTest.cpp
    infrastructure/FileOnboardBackupStoreTest.cpp)
target_link_libraries(ghub_onboard_memory_infrastructure_tests PRIVATE
    ghub_onboard_memory_infrastructure ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_onboard_memory_infrastructure_tests)
gtest_discover_tests(ghub_onboard_memory_infrastructure_tests TEST_PREFIX "onboard_memory.")

# ADR-0027: a escrita de setor só existe no caso de uso genérico e no adaptador HID++ deste módulo.
add_test(NAME onboard_memory.write_source
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/write_source.py" "${PROJECT_SOURCE_DIR}")
