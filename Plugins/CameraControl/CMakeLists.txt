cmake_minimum_required (VERSION 3.8)

add_library(CameraControl SHARED "CameraControl.cpp")
target_link_libraries(CameraControl "${CMAKE_BINARY_DIR}/lib/Engine.lib")
set_target_properties(CameraControl PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(CameraControl PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(CameraControl PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Plugins")
add_dependencies(VkCave CameraControl)
add_dependencies(CameraControl Engine)