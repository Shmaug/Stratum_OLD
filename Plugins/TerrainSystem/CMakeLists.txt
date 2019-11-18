cmake_minimum_required (VERSION 2.8)

add_library(TerrainSystem MODULE "TerrainSystem.cpp" "TerrainRenderer.cpp")
link_plugin(TerrainSystem)

if(WIN32)
#target_link_libraries(TerrainSystem "${PROJECT_BINARY_DIR}/bin/Plugins/Environment.dll")
else()
target_link_libraries(TerrainSystem "${PROJECT_BINARY_DIR}/bin/Plugins/libEnvironment.so")
endif()

add_dependencies(TerrainSystem Environment)