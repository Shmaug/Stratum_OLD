cmake_minimum_required (VERSION 2.8)

add_library(BRDFView MODULE "BRDFView.cpp")
link_plugin(BRDFView)

add_shader_target(BRDFViewShaders "Shaders/")
add_dependencies(BRDFView BRDFViewShaders)