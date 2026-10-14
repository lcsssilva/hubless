# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0019).
add_library(ghub_diagnostics_domain STATIC)
target_sources(ghub_diagnostics_domain PRIVATE
    domain/DiagnosticTypes.cpp)
target_link_libraries(ghub_diagnostics_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_diagnostics_domain)

# Camada de aplicação: ports, buffer circular, função pura e caso de uso; sem Qt nem I/O.
add_library(ghub_diagnostics_application STATIC)
target_sources(ghub_diagnostics_application PRIVATE
    application/DiagnosticLog.hpp
    application/DiagnosticLog.cpp
    application/DiagnosticSink.hpp
    application/ExportDiagnosticReport.hpp
    application/ExportDiagnosticReport.cpp
    application/RenderDiagnosticReport.hpp
    application/RenderDiagnosticReport.cpp
    application/ReportWriter.hpp)
target_link_libraries(ghub_diagnostics_application PUBLIC ghub_diagnostics_domain ghub_shared_kernel)
ghub_set_warnings(ghub_diagnostics_application)

# Infraestrutura (tratador de mensagens do Qt e gravação XDG) e apresentação: só com Qt (o preset tsan
# não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core GLOBAL)
    add_library(ghub_diagnostics_infrastructure STATIC)
    target_sources(ghub_diagnostics_infrastructure PRIVATE
        infrastructure/QtMessageLogSink.hpp
        infrastructure/QtMessageLogSink.cpp
        infrastructure/XdgReportWriter.hpp
        infrastructure/XdgReportWriter.cpp)
    target_link_libraries(ghub_diagnostics_infrastructure PUBLIC ghub_diagnostics_application Qt6::Core)
    ghub_set_warnings(ghub_diagnostics_infrastructure)
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core GLOBAL)
    add_library(ghub_diagnostics_presentation STATIC)
    target_sources(ghub_diagnostics_presentation PRIVATE
        presentation/DiagnosticsViewModel.hpp
        presentation/DiagnosticsViewModel.cpp)
    set_target_properties(ghub_diagnostics_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_diagnostics_presentation PUBLIC ghub_diagnostics_application Qt6::Core)
    ghub_set_warnings(ghub_diagnostics_presentation)
endif()
