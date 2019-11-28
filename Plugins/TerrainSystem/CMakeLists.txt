cmake_minimum_required (VERSION 2.8)

add_library(TerrainSystem MODULE "TerrainSystem.cpp" "TerrainRenderer.cpp")
link_plugin(TerrainSystem)