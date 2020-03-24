cmake_minimum_required (VERSION 2.8)

add_library(OpenVR MODULE "OpenVRDevice.cpp" "OpenVR.cpp")
link_plugin(OpenVR)

if(DEFINED ENV{OPENVR_HOME})
	message(STATUS "Found OPENVR_HOME: $ENV{OPENVR_HOME}")
else()
	message(FATAL_ERROR "Error: OPENVR_HOME not set!")
endif()

target_include_directories(OpenVR PUBLIC "$ENV{OPENVR_HOME}/headers")

if (WIN32)
	target_link_libraries(OpenVR PUBLIC "$ENV{OPENVR_HOME}/lib/win64/openvr_api.lib")
	configure_file("$ENV{OPENVR_HOME}/bin/win64/openvr_api.dll" "${PROJECT_BINARY_DIR}/bin/openvr_api.dll" COPYONLY)
else()
	target_link_libraries(OpenVR PUBLIC "$ENV{OPENVR_HOME}/lib/linux64/libopenvr_api.so")
endif()