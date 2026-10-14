add_executable(ghub_camera_domain_tests
    domain/PixelFormatTest.cpp
    domain/FrameRateTest.cpp
    domain/VideoModeTest.cpp
    domain/VideoModeSummaryTest.cpp
    domain/PreviewModeTest.cpp
    domain/CameraControlTest.cpp)
target_link_libraries(ghub_camera_domain_tests PRIVATE ghub_camera_domain GTest::gtest_main)
ghub_set_warnings(ghub_camera_domain_tests)
gtest_discover_tests(ghub_camera_domain_tests TEST_PREFIX "camera.")

add_executable(ghub_camera_application_tests
    application/GetCameraStatusTest.cpp
    application/StartCameraPreviewTest.cpp
    application/SetCameraControlTest.cpp)
target_link_libraries(ghub_camera_application_tests PRIVATE
    ghub_camera_application ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_camera_application_tests)
gtest_discover_tests(ghub_camera_application_tests TEST_PREFIX "camera.")

add_executable(ghub_camera_api_tests api/CameraSettingsTest.cpp)
target_link_libraries(ghub_camera_api_tests PRIVATE ghub_camera_api ghub_test_fakes GTest::gtest_main)
ghub_set_warnings(ghub_camera_api_tests)
gtest_discover_tests(ghub_camera_api_tests TEST_PREFIX "camera.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_presentation_tests presentation/CameraViewModelTest.cpp)
    set_target_properties(ghub_camera_presentation_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_presentation_tests PRIVATE
        ghub_camera_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_camera_presentation_tests)
    add_test(NAME camera.CameraViewModelTest COMMAND ghub_camera_presentation_tests)
    set_tests_properties(camera.CameraViewModelTest PROPERTIES TIMEOUT 30)
endif()

add_executable(ghub_camera_infrastructure_tests
    infrastructure/V4l2CameraBackendTest.cpp
    infrastructure/V4l2VideoCaptureTest.cpp
    infrastructure/V4l2ControlAccessTest.cpp)
target_link_libraries(ghub_camera_infrastructure_tests PRIVATE
    ghub_camera_infrastructure GTest::gtest_main)
ghub_set_warnings(ghub_camera_infrastructure_tests)
gtest_discover_tests(ghub_camera_infrastructure_tests TEST_PREFIX "camera.")

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_background_tests infrastructure/BackgroundCameraReaderTest.cpp)
    set_target_properties(ghub_camera_background_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_background_tests PRIVATE ghub_camera_background Qt6::Test)
    ghub_set_warnings(ghub_camera_background_tests)
    add_test(NAME camera.BackgroundCameraReaderTest COMMAND ghub_camera_background_tests)
    set_tests_properties(camera.BackgroundCameraReaderTest PROPERTIES TIMEOUT 30)
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_frame_decoder_tests infrastructure/FrameDecoderTest.cpp)
    set_target_properties(ghub_camera_frame_decoder_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_frame_decoder_tests PRIVATE ghub_camera_preview Qt6::Test)
    ghub_set_warnings(ghub_camera_frame_decoder_tests)
    add_test(NAME camera.FrameDecoderTest COMMAND ghub_camera_frame_decoder_tests)
    set_tests_properties(camera.FrameDecoderTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_preview_worker_tests infrastructure/CameraPreviewWorkerTest.cpp)
    set_target_properties(ghub_camera_preview_worker_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_preview_worker_tests PRIVATE ghub_camera_preview ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_camera_preview_worker_tests)
    add_test(NAME camera.CameraPreviewWorkerTest COMMAND ghub_camera_preview_worker_tests)
    set_tests_properties(camera.CameraPreviewWorkerTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_preview_viewmodel_tests presentation/CameraPreviewViewModelTest.cpp)
    set_target_properties(ghub_camera_preview_viewmodel_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_preview_viewmodel_tests PRIVATE
        ghub_camera_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_camera_preview_viewmodel_tests)
    add_test(NAME camera.CameraPreviewViewModelTest COMMAND ghub_camera_preview_viewmodel_tests)
    set_tests_properties(camera.CameraPreviewViewModelTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_preview_item_tests presentation/CameraPreviewItemTest.cpp)
    set_target_properties(ghub_camera_preview_item_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_preview_item_tests PRIVATE ghub_camera_presentation Qt6::Test)
    ghub_set_warnings(ghub_camera_preview_item_tests)
    add_test(NAME camera.CameraPreviewItemTest COMMAND ghub_camera_preview_item_tests)
    set_tests_properties(camera.CameraPreviewItemTest PROPERTIES TIMEOUT 30 ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()

if(GHUB_BUILD_APP)
    find_package(Qt6 6.12 REQUIRED COMPONENTS Test)
    add_executable(ghub_camera_control_editor_tests presentation/CameraControlEditorTest.cpp)
    set_target_properties(ghub_camera_control_editor_tests PROPERTIES AUTOMOC ON)
    target_link_libraries(ghub_camera_control_editor_tests PRIVATE
        ghub_camera_presentation ghub_test_fakes Qt6::Test)
    ghub_set_warnings(ghub_camera_control_editor_tests)
    add_test(NAME camera.CameraControlEditorTest COMMAND ghub_camera_control_editor_tests)
    set_tests_properties(camera.CameraControlEditorTest PROPERTIES TIMEOUT 30)
endif()
