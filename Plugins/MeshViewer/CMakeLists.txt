cmake_minimum_required (VERSION 3.8)

add_library(MeshViewer SHARED "MeshViewer.cpp")
target_link_libraries(MeshViewer "${CMAKE_BINARY_DIR}/lib/Engine.lib")
set_target_properties(MeshViewer PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(MeshViewer PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(MeshViewer PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Plugins")
add_dependencies(VkCave MeshViewer)
add_dependencies(MeshViewer Engine)