cmake_minimum_required (VERSION 2.8)

add_library(Raytracing MODULE "Raytracing.cpp")
link_plugin(Raytracing)

add_shader_target(RTShaders "Shaders/")
add_dependencies(Raytracing RTShaders)