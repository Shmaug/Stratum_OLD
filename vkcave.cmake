if (CMAKE_VERSION VERSION_LESS "3.1")
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif ()
else ()
	set (CMAKE_CXX_STANDARD 17)
endif ()

# Check for dependencies
if(DEFINED ENV{VULKAN_SDK})
    message(STATUS "Found VULKAN_SDK")
else()
    message(FATAL_ERROR "Error: VULKAN_SDK not set!")
endif()

# Include dependencies
include_directories("$ENV{VULKAN_SDK}/include")

if(WIN32)
	if(DEFINED ENV{GLFW_HOME})
		message(STATUS "Found GLFW_HOME")
	else()
		message(FATAL_ERROR "Error: GLFW_HOME not set!")
		# https://www.glfw.org/download.html
	endif()

	if(DEFINED ENV{ASSIMP_HOME})
		message(STATUS "Found ASSIMP_HOME")
	else()
		message(FATAL_ERROR "Error: ASSIMP_HOME not set!")
		# http://www.assimp.org/index.php/downloads
	endif()

	if(DEFINED ENV{SPIRV_CROSS_HOME})
		message(STATUS "Found SPIRV_CROSS_HOME")
	else()
		message(FATAL_ERROR "Error: SPIRV_CROSS_HOME not set!")
		# https://github.com/KhronosGroup/SPIRV-Cross
	endif()

	include_directories(
		"$ENV{SPIRV_CROSS_HOME}/include"
		"$ENV{GLFW_HOME}/include"
		"$ENV{ASSIMP_HOME}/include" )
	add_definitions(-DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

	# Link vulkan and assimp
	link_libraries(
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"$ENV{ASSIMP_HOME}/lib/x64/assimp-vc140-mt.lib" )
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		link_libraries("$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
	endif()

	# Link GLFW
	if (MSVC)
		if (MSVC_TOOLSET_VERSION MATCHES 141)
			link_libraries("$ENV{GLFW_HOME}/lib-vc2017/glfw3.lib")
		else()
			link_libraries("$ENV{GLFW_HOME}/lib-vc2019/glfw3.lib")
		endif()
	else()
		link_libraries("$ENV{GLFW_HOME}/lib-mingw-w64/libglfw3.a")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812 /wd26451") # unscoped enum, arithmetic overflow
elseif (UNIX)
	# Link vulkan, assimp, and GLFW
	link_libraries(
		"$ENV{VULKAN_SDK}/lib/libvulkan.so"
		"libassimp.so.3"
		"libglfw.so.3" )
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		link_libraries("$ENV{VULKAN_SDK}/lib/libVkLayer_utils.a")
	endif()
else()
	message(FATAL_ERROR "Error: Not implemented!")
endif()

# GLFW defines
add_definitions(-DGLFW_INCLUDE_VULKAN)

function(link_plugin TARGET_NAME)
	if(WIN32)
		target_link_libraries(${TARGET_NAME} "${PROJECT_BINARY_DIR}/lib/Engine.lib")
	elseif(UNIX)
		target_link_libraries(${TARGET_NAME} "${PROJECT_BINARY_DIR}/lib/Engine.a")
	else()
		message(FATAL_ERROR "Error: Not implemented!")
	endif(WIN32)

	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/Plugins")

	add_dependencies(VkCave ${TARGET_NAME})
	add_dependencies(${TARGET_NAME} Engine)
endfunction()