cmake_minimum_required (VERSION 3.8)

add_library(Starmap SHARED "Starmap.cpp" "TelnetClient.cpp")
target_link_libraries(Starmap "${CMAKE_BINARY_DIR}/lib/Engine.lib")
set_target_properties(Starmap PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(Starmap PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
set_target_properties(Starmap PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Plugins")
add_dependencies(VkCave Starmap)
add_dependencies(Starmap Engine)