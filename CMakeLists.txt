cmake_minimum_required (VERSION 2.8)

include(vkcave.cmake)
include(shaders.cmake)

include_directories(".")

# Build engine library
add_library(Engine SHARED
	"Content/AssetManager.cpp"
	"Content/Font.cpp"
	"Content/Material.cpp"
	"Content/Mesh.cpp"
	"Content/Shader.cpp"
	"Content/Texture.cpp"
	"Core/Buffer.cpp"
	"Core/CommandBuffer.cpp"
	"Core/DescriptorPool.cpp"
	"Core/DescriptorSet.cpp"
	"Core/Device.cpp"
	"Core/DeviceManager.cpp"
	"Core/PluginManager.cpp"
	"Core/RenderPass.cpp"
	"Core/Sampler.cpp"
	"Core/Socket.cpp"
	"Core/Window.cpp"
	"Input/InputManager.cpp"
	"Input/MouseKeyboardInput.cpp"
	"Interface/TextButton.cpp"
	"Interface/UIElement.cpp"
	"Interface/UICanvas.cpp"
	"Interface/VerticalLayout.cpp"
	"Scene/Camera.cpp"
	"Scene/Light.cpp"
	"Scene/MeshRenderer.cpp"
	"Scene/Scene.cpp"
	"Scene/Object.cpp"
	"Scene/TextRenderer.cpp"
	"ThirdParty/imp.cpp"
	"Util/Profiler.cpp" )
target_compile_definitions(Engine PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)
set_target_properties(Engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/")

# Build shader compiler
add_executable(ShaderCompiler "shadercompiler.cpp")
target_compile_definitions(ShaderCompiler PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)
target_include_directories(ShaderCompiler PUBLIC "${CMAKE_SOURCE_DIR}/ThirdParty/SPIRV-Cross/include" )
set_target_properties(ShaderCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(ShaderCompiler PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(ShaderCompiler PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/")

# Build main executable
add_executable(VkCave "vkcave.cpp" "ThirdParty/json11.cpp")
target_compile_definitions(VkCave PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)
set_target_properties(VkCave PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(VkCave PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(VkCave PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/")

if(WIN32)
	# Disable warnings for ShaderCompiler
	set_target_properties(ShaderCompiler Engine VkCave PROPERTIES COMPILE_FLAGS "${BUILD_FLAGS} /wd26451") # uninitialzed struct variable
	# Link shaderc and spirv-cross to ShaderCompiler
	target_link_libraries(ShaderCompiler
		"$ENV{VULKAN_SDK}/shaderc/install/lib/shaderc_shared.lib"
		"${CMAKE_SOURCE_DIR}/ThirdParty/SPIRV-Cross/lib/spirv-cross-core.lib" )

	# Link engine to VkCave
	target_link_libraries(VkCave "${PROJECT_BINARY_DIR}/lib/Engine.lib" "Ws2_32.lib" )

	# Copy shaderc dll
	configure_file("$ENV{VULKAN_SDK}/shaderc/install/bin/shaderc_shared.dll" "${PROJECT_BINARY_DIR}/bin/shaderc_shared.dll" COPYONLY)
elseif(UNIX)
	# Link shaderc and spirv-cross to ShaderCompiler
	target_link_libraries(ShaderCompiler
		"$ENV{VULKAN_SDK}/lib/libshaderc_combined.a"
		"${CMAKE_SOURCE_DIR}/ThirdParty/SPIRV-Cross/libspirv-cross.a" )
	target_link_libraries(VkCave "${PROJECT_BINARY_DIR}/bin/libEngine.so")
	target_link_libraries(ShaderCompiler stdc++fs)
	target_link_libraries(VkCave stdc++fs)
else()
    message(FATAL_ERROR "Error: Not implemented!")
endif(WIN32)

# Copy config file
configure_file("config.json" "${PROJECT_BINARY_DIR}/bin/config.json" COPYONLY)

# Create symbolic link to Assets folder so the executable can find assets
add_custom_command(
	TARGET VkCave POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" "${PROJECT_BINARY_DIR}/bin/Assets"
	DEPENDS "${PROJECT_BINARY_DIR}/bin/Assets"
	COMMENT "Symbolic link assets folder from ${source} => ${destination}" )

# Compile shaders
add_shader_target(Shaders "Shaders/")

add_dependencies(VkCave Shaders Engine)

# Build all plugins
add_subdirectory("Plugins/")
