cmake_minimum_required (VERSION 2.8)

add_library(PointCloud MODULE "PointCloud.cpp" "PointRenderer.cpp")
link_plugin(PointCloud)

add_dependencies(PointCloud CameraControl)