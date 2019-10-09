cmake_minimum_required (VERSION 3.8)

add_library(CameraControl SHARED "CameraControl.cpp")
link_plugin(CameraControl)