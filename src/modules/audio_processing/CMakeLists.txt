# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0016).
add_library(ghub_audio_processing_domain STATIC)
target_sources(ghub_audio_processing_domain PRIVATE
    domain/EqualizerCurve.cpp
    domain/VoiceThreshold.cpp)
target_link_libraries(ghub_audio_processing_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_audio_processing_domain)

# Aplicação, infraestrutura (libpipewire) e apresentação: um arquivo por camada.
include(${CMAKE_CURRENT_SOURCE_DIR}/application/application.cmake OPTIONAL)
if(GHUB_BUILD_APP)
    include(${CMAKE_CURRENT_SOURCE_DIR}/infrastructure/infrastructure.cmake OPTIONAL)
    include(${CMAKE_CURRENT_SOURCE_DIR}/presentation/presentation.cmake OPTIONAL)
endif()
