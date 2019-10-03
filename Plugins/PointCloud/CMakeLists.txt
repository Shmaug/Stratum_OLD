cmake_minimum_required (VERSION 3.8)

add_library(PointCloud SHARED "PointCloud.cpp" "PointRenderer.cpp")
target_link_libraries(PointCloud "${CMAKE_BINARY_DIR}/lib/Engine.lib")
set_target_properties(PointCloud PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(PointCloud PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(PointCloud PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Plugins")
add_dependencies(VkCave PointCloud)
add_dependencies(PointCloud Engine)