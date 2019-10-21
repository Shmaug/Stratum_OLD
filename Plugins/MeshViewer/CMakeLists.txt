cmake_minimum_required (VERSION 2.8)

add_library(MeshViewer MODULE "MeshViewer.cpp")
link_plugin(MeshViewer)

add_dependencies(MeshViewer CameraControl)