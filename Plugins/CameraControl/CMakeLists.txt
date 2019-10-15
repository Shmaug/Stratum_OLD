cmake_minimum_required (VERSION 2.8)

add_library(CameraControl MODULE "CameraControl.cpp")
link_plugin(CameraControl)