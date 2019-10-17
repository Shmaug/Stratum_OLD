if (CMAKE_VERSION VERSION_LESS "3.1")
	if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
		set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
	endif ()
else()
	set (CMAKE_CXX_STANDARD 17)
endif()

if (NOT WIN32)
	include(GNUInstallDirs)
endif()

function(link_plugin TARGET_NAME)
	target_include_directories(${TARGET_NAME} PUBLIC
		"${VKCAVE_HOME}"
		"${VKCAVE_HOME}/ThirdParty/assimp/include"
		"${VKCAVE_HOME}/ThirdParty/glfw/include" )

	if(WIN32)
		if(DEFINED ENV{VULKAN_SDK})
			message(STATUS "Found VULKAN_SDK: $ENV{VULKAN_SDK}")
		else()
			message(FATAL_ERROR "Error: VULKAN_SDK not set!")
		endif()

		target_include_directories(${TARGET_NAME} PUBLIC
			"$ENV{VULKAN_SDK}/include"
			"${VKCAVE_HOME}/ThirdParty/assimp/include"
			"${VKCAVE_HOME}/ThirdParty/glfw/include" )
		target_compile_definitions(${TARGET_NAME} PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX)

		# Link vulkan and assimp
		target_link_libraries(${TARGET_NAME}
			"${PROJECT_BINARY_DIR}/lib/Engine.lib"
			"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
			"${VKCAVE_HOME}/ThirdParty/glfw/lib/glfw3.lib"
			"${VKCAVE_HOME}/ThirdParty/assimp/lib/assimp.lib"
			"${VKCAVE_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )
		if (CMAKE_BUILD_TYPE STREQUAL "Debug")
			target_link_libraries(${TARGET_NAME} "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstaticd.lib")
		else()
			target_link_libraries(${TARGET_NAME} "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstatic.lib")
		endif()

		if (${ENABLE_DEBUG_LAYERS})
			target_link_libraries(${TARGET_NAME} "$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
		endif()

		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812 /wd26451") # unscoped enum, arithmetic overflow
	else()
		target_link_directories(${TARGET_NAME} PUBLIC "${VKCAVE_HOME}/ThirdParty/assimp/lib" "${VKCAVE_HOME}/ThirdParty/glfw/lib64")
		target_link_libraries(${TARGET_NAME}
			stdc++fs
			pthread
			X11
			"${PROJECT_BINARY_DIR}/bin/libEngine.so"
			"libvulkan.so"
			"libassimp.so"
			"libzlibstatic.a"
			"libIrrXML.a"
			"libglfw3.a" )
		if (${ENABLE_DEBUG_LAYERS})
			target_link_libraries(${TARGET_NAME} "libVkLayer_utils.so")
		endif()
	endif(WIN32)
	
	if (${ENABLE_DEBUG_LAYERS})
		target_compile_definitions(${TARGET_NAME} PUBLIC -DENABLE_DEBUG_LAYERS)
	endif()
	# GLFW defines
	target_compile_definitions(${TARGET_NAME} PUBLIC -DGLFW_INCLUDE_VULKAN)
	
	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/Plugins")

	add_dependencies(VkCave ${TARGET_NAME})
	add_dependencies(${TARGET_NAME} Engine)
endfunction()

function(add_shader_target TARGET_NAME FOLDER_PATH)
	# Compile shaders in Shaders/* using ShaderCompiler
	file(GLOB_RECURSE SHADER_SOURCES
		"${FOLDER_PATH}*.frag"
		"${FOLDER_PATH}*.vert"
		"${FOLDER_PATH}*.glsl"
		"${FOLDER_PATH}*.hlsl" )

	foreach(SHADER ${SHADER_SOURCES})
		get_filename_component(FILE_NAME ${SHADER} NAME_WE)
		set(SPIRV "${PROJECT_BINARY_DIR}/bin/Shaders/${FILE_NAME}.shader")

		add_custom_command(
			OUTPUT ${SPIRV}
			COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/bin/Shaders/"
			COMMAND "${PROJECT_BINARY_DIR}/bin/ShaderCompiler" ${SHADER} ${SPIRV}
			DEPENDS ${SHADER})

		list(APPEND SPIRV_BINARY_FILES ${SPIRV})
	endforeach(SHADER)

	add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV_BINARY_FILES})
	
	add_dependencies(${TARGET_NAME} ShaderCompiler)
endfunction()