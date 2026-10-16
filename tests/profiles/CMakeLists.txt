add_executable(ghub_profiles_domain_tests
    domain/ApplicationIdTest.cpp
    domain/EqualizerGainsTest.cpp
    domain/ProfileLibraryTest.cpp
    domain/ProfileNameTest.cpp
    domain/ProfileTest.cpp)
target_link_libraries(ghub_profiles_domain_tests PRIVATE ghub_profiles_domain GTest::gtest_main)
ghub_set_warnings(ghub_profiles_domain_tests)
gtest_discover_tests(ghub_profiles_domain_tests TEST_PREFIX "profiles.")

add_executable(ghub_profiles_application_tests
    application/ApplyProfileTest.cpp
    application/AutomaticProfileSwitcherTest.cpp
    application/ProfileBookTest.cpp)
target_link_libraries(ghub_profiles_application_tests PRIVATE
    ghub_profiles_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_profiles_application_tests)
gtest_discover_tests(ghub_profiles_application_tests TEST_PREFIX "profiles.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core)
    add_executable(ghub_profiles_infrastructure_tests infrastructure/JsonProfileRepositoryTest.cpp
        infrastructure/JsonProfileTransferTest.cpp)
    target_link_libraries(ghub_profiles_infrastructure_tests PRIVATE
        ghub_profiles_infrastructure Qt6::Core GTest::gtest_main)
    ghub_set_warnings(ghub_profiles_infrastructure_tests)
    gtest_discover_tests(ghub_profiles_infrastructure_tests TEST_PREFIX "profiles.")

    add_executable(ghub_profiles_adapters_tests infrastructure/ProfileAdaptersTest.cpp)
    target_link_libraries(ghub_profiles_adapters_tests PRIVATE
        ghub_profiles_infrastructure ghub_test_fakes GTest::gtest_main)
    ghub_set_warnings(ghub_profiles_adapters_tests)
    gtest_discover_tests(ghub_profiles_adapters_tests TEST_PREFIX "profiles.")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_profiles_controller_tests presentation/ProfilesControllerTest.cpp)
    set_target_properties(ghub_profiles_controller_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_profiles_controller_tests PRIVATE
        ghub_profiles_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_profiles_controller_tests)
    add_test(NAME profiles.ProfilesControllerTest COMMAND ghub_profiles_controller_tests)
    set_tests_properties(profiles.ProfilesControllerTest PROPERTIES TIMEOUT 30)
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core DBus Test)
    add_executable(ghub_profiles_kwin_tests infrastructure/KWinActiveApplicationSourceTest.cpp)
    set_target_properties(ghub_profiles_kwin_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_profiles_kwin_tests PRIVATE
        ghub_profiles_infrastructure Qt6::Core Qt6::DBus Qt6::Test)
    ghub_set_warnings(ghub_profiles_kwin_tests)
    # Barramento privado iniciado pelo teste; o endereço da sessão real é invalidado de propósito.
    add_test(NAME profiles.KWinActiveApplicationSourceTest COMMAND ghub_profiles_kwin_tests)
    set_tests_properties(profiles.KWinActiveApplicationSourceTest PROPERTIES TIMEOUT 60
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent/ghub-no-session-bus")
endif()
