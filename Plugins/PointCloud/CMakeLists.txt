cmake_minimum_required (VERSION 2.8)

add_library(PointCloud MODULE "PointCloud.cpp" "PointRenderer.cpp")
link_plugin(PointCloud)

target_link_libraries(PointCloud "${PROJECT_BINARY_DIR}/bin/Plugins/libCameraControl.so")