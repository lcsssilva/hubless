add_test(NAME architecture.layers
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/check_layers.py" "${PROJECT_SOURCE_DIR}")
add_test(NAME architecture.layers_selftest
    COMMAND Python3::Interpreter -I "${CMAKE_CURRENT_SOURCE_DIR}/test_check_layers.py")
