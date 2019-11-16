cmake_minimum_required (VERSION 2.8)

add_library(MeshViewer MODULE "MeshViewer.cpp" "SplineRenderer.cpp")
link_plugin(MeshViewer)

if(WIN32)
target_link_libraries(MeshViewer "${PROJECT_BINARY_DIR}/bin/Plugins/libEnvironment.dll")
else()
target_link_libraries(MeshViewer "${PROJECT_BINARY_DIR}/bin/Plugins/libEnvironment.so")
endif()

add_dependencies(MeshViewer Environment)