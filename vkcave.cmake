if (CMAKE_VERSION VERSION_LESS "3.1")
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif ()
else ()
	set (CMAKE_CXX_STANDARD 17)
endif ()

if(DEFINED ENV{VULKAN_SDK})
    message(STATUS "Found VULKAN_SDK: $ENV{VULKAN_SDK}")
else()
    message(FATAL_ERROR "Error: VULKAN_SDK not set!")
endif()

include_directories(
	"$ENV{VULKAN_SDK}/include"
	"${CMAKE_SOURCE_DIR}/ThirdParty/assimp/include"
	"${CMAKE_SOURCE_DIR}/ThirdParty/glfw/include" )

if(WIN32)
	add_definitions(-DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

	# Link vulkan and assimp
	link_libraries(
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"ThirdParty/assimp/lib/x64/assimp-vc140-mt.lib" )
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		link_libraries("$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
	endif()

	# Link GLFW
	if (MSVC)
		if (MSVC_TOOLSET_VERSION MATCHES 141)
			link_libraries("ThirdParty/glfw/lib-vc2017/glfw3.lib")
		else()
			link_libraries("ThirdParty/glfw/lib-vc2019/glfw3.lib")
		endif()
	else()
		link_libraries("ThirdParty/glfw/lib-mingw-w64/libglfw3.a")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812 /wd26451") # unscoped enum, arithmetic overflow
elseif (UNIX)
	# Link vulkan, assimp, and GLFW
	link_libraries(
		"$ENV{VULKAN_SDK}/lib/libvulkan.so"
		"${CMAKE_SOURCE_DIR}/ThirdParty/assimp/lib/libassimp.a"
		"${CMAKE_SOURCE_DIR}/ThirdParty/assimp/lib/libzlibstatic.a"
		"${CMAKE_SOURCE_DIR}/ThirdParty/assimp/lib/libIrrXML.a"
		"${CMAKE_SOURCE_DIR}/ThirdParty/glfw/lib64/libglfw3.a" )
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		link_libraries("$ENV{VULKAN_SDK}/lib/libVkLayer_utils.a")
	endif()

	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lpthread -lX11")
else()
	message(FATAL_ERROR "Error: Not implemented!")
endif()

function(link_plugin TARGET_NAME)
	if(WIN32)
		target_link_libraries(${TARGET_NAME} "${PROJECT_BINARY_DIR}/lib/Engine.lib")
	elseif(UNIX)
		target_link_libraries(${TARGET_NAME} "${PROJECT_BINARY_DIR}/bin/libEngine.so")
	else()
		message(FATAL_ERROR "Error: Not implemented!")
	endif(WIN32)

	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/Plugins")

	# GLFW defines
	target_compile_definitions(${TARGET_NAME} PUBLIC -DGLFW_INCLUDE_VULKAN)

	target_link_libraries(${TARGET_NAME} stdc++fs)

	add_dependencies(VkCave ${TARGET_NAME})
	add_dependencies(${TARGET_NAME} Engine)
endfunction()