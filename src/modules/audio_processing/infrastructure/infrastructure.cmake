# Infraestrutura do EQ do host: libpipewire (ADR-0016). Só compila com GHUB_BUILD_APP.
find_package(PkgConfig REQUIRED)
pkg_check_modules(GHUB_PIPEWIRE REQUIRED IMPORTED_TARGET libpipewire-0.3>=1.6.9)

add_library(ghub_audio_processing_infrastructure STATIC)
target_sources(ghub_audio_processing_infrastructure PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireFilterChainArgs.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireGraphQueries.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireAudioGraph.cpp
    ${CMAKE_CURRENT_LIST_DIR}/PipeWireAudioLevelSource.cpp)
target_link_libraries(ghub_audio_processing_infrastructure PUBLIC
    ghub_audio_processing_application ghub_audio_processing_api PkgConfig::GHUB_PIPEWIRE)
ghub_set_warnings(ghub_audio_processing_infrastructure)
