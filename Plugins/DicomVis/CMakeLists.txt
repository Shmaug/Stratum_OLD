cmake_minimum_required (VERSION 3.8)

if(DEFINED ENV{DCMTK_HOME})
    message(STATUS "Found DCMTK_HOME")

	add_library(DicomVis SHARED "DicomVis.cpp")
	target_link_libraries(DicomVis "${CMAKE_BINARY_DIR}/lib/Engine.lib")
	set_target_properties(DicomVis PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
	set_target_properties(DicomVis PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/Plugins")
	set_target_properties(DicomVis PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/Plugins")
	add_dependencies(VkCave DicomVis)
	add_dependencies(DicomVis Engine)

	target_link_libraries(DicomVis
		"$ENV{DCMTK_HOME}/lib/Debug/ofstd.lib"
		"$ENV{DCMTK_HOME}/lib/Debug/oflog.lib"
		"$ENV{DCMTK_HOME}/lib/Debug/dcmdata.lib"
		"$ENV{DCMTK_HOME}/lib/Debug/dcmimgle.lib" )

	target_include_directories(DicomVis PUBLIC "$ENV{DCMTK_HOME}/include")
else()
    message(WARNING "DCMTK_HOME not set! Not building DicomVis")
endif()
