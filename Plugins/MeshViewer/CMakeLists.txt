cmake_minimum_required (VERSION 2.8)

add_library(MeshViewer MODULE "MeshViewer.cpp" "SplineRenderer.cpp")
link_plugin(MeshViewer)