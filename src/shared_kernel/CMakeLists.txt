# Somente STL: nenhuma dependência de Qt ou I/O (ADR-0002).
add_library(ghub_shared_kernel INTERFACE)
target_include_directories(ghub_shared_kernel INTERFACE "${PROJECT_SOURCE_DIR}/src")
target_compile_features(ghub_shared_kernel INTERFACE cxx_std_23)
