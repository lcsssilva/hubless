add_executable(ghub_diagnostics_domain_tests
    domain/DiagnosticTypesTest.cpp)
target_link_libraries(ghub_diagnostics_domain_tests PRIVATE ghub_diagnostics_domain GTest::gtest_main)
ghub_set_warnings(ghub_diagnostics_domain_tests)
gtest_discover_tests(ghub_diagnostics_domain_tests TEST_PREFIX "diagnostics.")

add_executable(ghub_diagnostics_application_tests
    application/DiagnosticLogTest.cpp
    application/ExportDiagnosticReportTest.cpp
    application/RenderDiagnosticReportTest.cpp)
target_link_libraries(ghub_diagnostics_application_tests PRIVATE
    ghub_diagnostics_application ghub_shared_kernel GTest::gtest_main)
ghub_set_warnings(ghub_diagnostics_application_tests)
gtest_discover_tests(ghub_diagnostics_application_tests TEST_PREFIX "diagnostics.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)

    add_executable(ghub_diagnostics_message_sink_tests
        infrastructure/QtMessageLogSinkTest.cpp)
    set_target_properties(ghub_diagnostics_message_sink_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_diagnostics_message_sink_tests PRIVATE
        ghub_diagnostics_infrastructure Qt6::Test)
    ghub_set_warnings(ghub_diagnostics_message_sink_tests)
    add_test(NAME diagnostics.QtMessageLogSinkTest COMMAND ghub_diagnostics_message_sink_tests)
    set_tests_properties(diagnostics.QtMessageLogSinkTest PROPERTIES TIMEOUT 30)

    add_executable(ghub_diagnostics_report_writer_tests
        infrastructure/XdgReportWriterTest.cpp)
    set_target_properties(ghub_diagnostics_report_writer_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_diagnostics_report_writer_tests PRIVATE
        ghub_diagnostics_infrastructure Qt6::Test)
    ghub_set_warnings(ghub_diagnostics_report_writer_tests)
    add_test(NAME diagnostics.XdgReportWriterTest COMMAND ghub_diagnostics_report_writer_tests)
    set_tests_properties(diagnostics.XdgReportWriterTest PROPERTIES TIMEOUT 30)

    add_executable(ghub_diagnostics_view_model_tests
        presentation/DiagnosticsViewModelTest.cpp)
    set_target_properties(ghub_diagnostics_view_model_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_diagnostics_view_model_tests PRIVATE
        ghub_diagnostics_presentation Qt6::Test)
    ghub_set_warnings(ghub_diagnostics_view_model_tests)
    add_test(NAME diagnostics.DiagnosticsViewModelTest COMMAND ghub_diagnostics_view_model_tests)
    set_tests_properties(diagnostics.DiagnosticsViewModelTest PROPERTIES TIMEOUT 30)
endif()
