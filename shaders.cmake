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