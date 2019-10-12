cmake_minimum_required (VERSION 2.8)

add_library(Starmap SHARED "Starmap.cpp" "TelnetClient.cpp")
link_plugin(Starmap)