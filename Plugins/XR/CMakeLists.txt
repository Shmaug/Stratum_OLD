cmake_minimum_required (VERSION 2.8)

add_library(XR MODULE "XR.cpp")
link_plugin(XR)


if(DEFINED ENV{OPENXR_HOME})
	message(STATUS "Found OPENXR_HOME: $ENV{OPENXR_HOME}")
else()
	message(FATAL_ERROR "Error: OPENXR_HOME not set!")
endif()

target_include_directories(XR PUBLIC "$ENV{OPENXR_HOME}/include")

if (WIN32)
	target_link_libraries(XR "$ENV{OPENXR_HOME}/lib/openxr_loader.lib")
else()
	target_link_libraries(XR "$ENV{OPENXR_HOME}/lib/openxr_loader.a")
endif()