cmake_minimum_required (VERSION 2.8)

add_library(PointCloud SHARED "PointCloud.cpp" "PointRenderer.cpp")
link_plugin(PointCloud)