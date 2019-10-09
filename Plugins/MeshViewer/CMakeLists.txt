cmake_minimum_required (VERSION 3.8)

add_library(MeshViewer SHARED "MeshViewer.cpp")
link_plugin(MeshViewer)