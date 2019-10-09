cmake_minimum_required (VERSION 3.8)

add_library(PointCloud SHARED "PointCloud.cpp" "PointRenderer.cpp")
link_plugin(PointCloud)