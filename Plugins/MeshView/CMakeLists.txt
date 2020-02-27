cmake_minimum_required (VERSION 2.8)

add_library(MeshView MODULE "MeshView.cpp" "ClothRenderer.cpp")
link_plugin(MeshView)

add_shader_target(MeshViewShaders "Shaders/")
add_dependencies(MeshView MeshViewShaders)