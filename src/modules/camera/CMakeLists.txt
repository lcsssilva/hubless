# Camada de domínio: somente STL + shared_kernel (ADR-0002, ADR-0004).
add_library(ghub_camera_domain STATIC)
target_sources(ghub_camera_domain PRIVATE
    domain/PixelFormat.cpp
    domain/FrameRate.cpp
    domain/VideoMode.cpp
    domain/VideoModeSummary.cpp
    domain/PreviewMode.cpp
    domain/CameraControl.cpp)
target_link_libraries(ghub_camera_domain PUBLIC ghub_shared_kernel)
ghub_set_warnings(ghub_camera_domain)

# Camada de aplicação: casos de uso e ports; sem Qt nem I/O.
add_library(ghub_camera_application STATIC)
target_sources(ghub_camera_application PRIVATE
    application/GetCameraStatus.cpp
    application/StartCameraPreview.cpp
    application/SetCameraControl.cpp)
target_link_libraries(ghub_camera_application PUBLIC ghub_camera_domain ghub_device_access_api)
ghub_set_warnings(ghub_camera_application)

# API pública (ADR-0020): fachada de leitura/aplicação dos controles, para outros módulos.
add_library(ghub_camera_api STATIC)
target_sources(ghub_camera_api PRIVATE api/CameraSettings.cpp)
target_link_libraries(ghub_camera_api PUBLIC ghub_camera_application)
ghub_set_warnings(ghub_camera_api)

# Infraestrutura: adaptador V4L2 somente leitura (ADR-0008); filesystem e ioctl permitidos aqui.
add_library(ghub_camera_infrastructure STATIC)
target_sources(ghub_camera_infrastructure PRIVATE
    infrastructure/V4l2CameraBackend.cpp
    infrastructure/V4l2CaptureNode.cpp
    infrastructure/V4l2ControlReading.cpp
    infrastructure/V4l2ControlAccess.cpp
    infrastructure/V4l2VideoCapture.cpp
    infrastructure/LinuxV4l2Api.cpp)
target_link_libraries(ghub_camera_infrastructure PUBLIC ghub_camera_application ghub_device_inventory_api)
ghub_set_warnings(ghub_camera_infrastructure)

# Leitura em thread de trabalho (Qt; o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Concurrent GLOBAL)
    add_library(ghub_camera_background STATIC)
    target_sources(ghub_camera_background PRIVATE
        infrastructure/BackgroundCameraReader.hpp
        infrastructure/BackgroundCameraReader.cpp)
    set_target_properties(ghub_camera_background PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_background PUBLIC ghub_camera_application Qt6::Core PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_camera_background)
endif()

# Pré-visualização (Qt): decodificador de quadros e thread de trabalho (o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui)
    add_library(ghub_camera_preview STATIC)
    target_sources(ghub_camera_preview PRIVATE
        infrastructure/FrameDecoder.hpp
        infrastructure/FrameDecoder.cpp
        infrastructure/CameraPreviewWorker.hpp
        infrastructure/CameraPreviewWorker.cpp)
    set_target_properties(ghub_camera_preview PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_preview PUBLIC ghub_camera_application Qt6::Core Qt6::Gui)
    ghub_set_warnings(ghub_camera_preview)
endif()

# Apresentação: ViewModels QObject expostos ao QML (só com Qt; o preset tsan não compila Qt).
if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui Quick Concurrent)
    add_library(ghub_camera_presentation STATIC)
    target_sources(ghub_camera_presentation PRIVATE
        presentation/CameraViewModel.hpp
        presentation/CameraViewModel.cpp
        presentation/CameraControlEditor.hpp
        presentation/CameraControlEditor.cpp
        presentation/CameraPreviewViewModel.hpp
        presentation/CameraPreviewViewModel.cpp
        presentation/CameraPreviewItem.hpp
        presentation/CameraPreviewItem.cpp)
    set_target_properties(ghub_camera_presentation PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_presentation PUBLIC ghub_camera_application Qt6::Core Qt6::Gui Qt6::Quick
                                                      PRIVATE Qt6::Concurrent)
    ghub_set_warnings(ghub_camera_presentation)
endif()
