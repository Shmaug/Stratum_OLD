if (CMAKE_VERSION VERSION_LESS "3.1" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
else()
	set (CMAKE_CXX_STANDARD 17)
endif()

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
	if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
	endif()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffast-math -fassociative-math -ftree-vectorize")
endif()
if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4267 /wd4244 /wd26451 /wd26812")
	if (CMAKE_BUILD_TYPE STREQUAL "Release" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Oi /Qpar")
	endif()
endif()

function(link_plugin TARGET_NAME)
	target_include_directories(${TARGET_NAME} PUBLIC
		"${STRATUM_HOME}"
		"${STRATUM_HOME}/ThirdParty/assimp/include" )

	if(WIN32)
		if(DEFINED ENV{VULKAN_SDK})
			message(STATUS "Found VULKAN_SDK: $ENV{VULKAN_SDK}")
		else()
			message(FATAL_ERROR "Error: VULKAN_SDK not set!")
		endif()
		
		target_include_directories(${TARGET_NAME} PUBLIC
			"$ENV{VULKAN_SDK}/include"
			"${STRATUM_HOME}/ThirdParty/assimp/include" )
		target_compile_definitions(${TARGET_NAME} PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)

		# Link vulkan and assimp
		target_link_libraries(${TARGET_NAME}
			"${PROJECT_BINARY_DIR}/lib/Engine.lib"
			"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
			"${STRATUM_HOME}/ThirdParty/assimp/lib/assimp.lib"
			"${STRATUM_HOME}/ThirdParty/assimp/lib/zlibstatic.lib"
			"${STRATUM_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )

		if (${ENABLE_DEBUG_LAYERS})
			target_link_libraries(${TARGET_NAME} "$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
		endif()
	endif(WIN32)
	
	if (${ENABLE_DEBUG_LAYERS})
		target_compile_definitions(${TARGET_NAME} PUBLIC -DENABLE_DEBUG_LAYERS)
	endif()
	
	set_target_properties(${TARGET_NAME} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/Plugins")
	set_target_properties(${TARGET_NAME} PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/Plugins")

	add_dependencies(Stratum ${TARGET_NAME})
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
		set(SPIRV "${PROJECT_BINARY_DIR}/bin/Shaders/${FILE_NAME}.stm")

		add_custom_command(
			OUTPUT ${SPIRV}
			COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/bin/Shaders/"
			COMMAND "${PROJECT_BINARY_DIR}/bin/ShaderCompiler" ${SHADER} ${SPIRV} "${STRATUM_HOME}/Shaders"
			DEPENDS ${SHADER})

		list(APPEND SPIRV_BINARY_FILES ${SPIRV})
	endforeach(SHADER)

	add_custom_target(${TARGET_NAME} DEPENDS ${SPIRV_BINARY_FILES})
	
	add_dependencies(${TARGET_NAME} ShaderCompiler)
endfunction()