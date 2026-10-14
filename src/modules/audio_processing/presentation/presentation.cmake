# Apresentação: controlador QObject sobre o caso de uso; operações em thread de trabalho (QtConcurrent).
find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent GLOBAL)
add_library(ghub_audio_processing_presentation STATIC)
target_sources(ghub_audio_processing_presentation PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/HostEqualizerController.hpp
    ${CMAKE_CURRENT_LIST_DIR}/HostEqualizerController.cpp
    ${CMAKE_CURRENT_LIST_DIR}/NoiseSuppressionController.hpp
    ${CMAKE_CURRENT_LIST_DIR}/NoiseSuppressionController.cpp)
set_target_properties(ghub_audio_processing_presentation PROPERTIES AUTOMOC ON)
target_link_libraries(ghub_audio_processing_presentation PUBLIC ghub_audio_processing_application ghub_audio_processing_api Qt6::Core
                                                          PRIVATE Qt6::Concurrent)
ghub_set_warnings(ghub_audio_processing_presentation)
