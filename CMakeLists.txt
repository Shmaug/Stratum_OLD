cmake_minimum_required (VERSION 2.8)

option(ENABLE_DEBUG_LAYERS "Enable debug layers?" TRUE)
set(VKCAVE_HOME ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Directory of VkCave")

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
option(LINK_PERFTOOLS_PROFILER "Link gperftools profiler?" FALSE)
endif()

include(vkcave.cmake)

add_library(Engine SHARED
	"Content/Animation.cpp"
	"Content/AssetManager.cpp"
	"Content/Font.cpp"
	"Content/Material.cpp"
	"Content/Mesh.cpp"
	"Content/Shader.cpp"
	"Content/Texture.cpp"
	"Core/Buffer.cpp"
	"Core/CommandBuffer.cpp"
	"Core/DescriptorSet.cpp"
	"Core/Device.cpp"
	"Core/Framebuffer.cpp"
	"Core/Instance.cpp"
	"Core/PluginManager.cpp"
	"Core/RenderPass.cpp"
	"Core/Sampler.cpp"
	"Core/Socket.cpp"
	"Core/Window.cpp"
	"Input/InputManager.cpp"
	"Input/MouseKeyboardInput.cpp"
	"Interface/UICanvas.cpp"
	"Interface/UIElement.cpp"
	"Interface/UIImage.cpp"
	"Interface/UILabel.cpp"
	"Interface/UILayout.cpp"
	"Scene/Camera.cpp"
	"Scene/Gizmos.cpp"
	"Scene/Light.cpp"
	"Scene/MeshRenderer.cpp"
	"Scene/Scene.cpp"
	"Scene/Object.cpp"
	"Scene/SkinnedMeshRenderer.cpp"
	"Scene/TextRenderer.cpp"
	"ThirdParty/imp.cpp"
	"Util/Profiler.cpp" )
add_executable(ShaderCompiler "run/shadercompiler.cpp")
add_executable(VkCave "run/vkcave.cpp" "ThirdParty/json11.cpp")

set_target_properties(Engine VkCave ShaderCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine VkCave ShaderCompiler PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine VkCave ShaderCompiler PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/")

target_compile_definitions(Engine PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)
target_compile_definitions(ShaderCompiler PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)
target_compile_definitions(VkCave PUBLIC -DENGINE_CORE -DGLFW_INCLUDE_VULKAN)

target_include_directories(VkCave PUBLIC
	"${VKCAVE_HOME}"
	"${VKCAVE_HOME}/ThirdParty/assimp/include"
	"${VKCAVE_HOME}/ThirdParty/glfw/include" )
target_include_directories(Engine PUBLIC
	"${VKCAVE_HOME}"
	"${VKCAVE_HOME}/ThirdParty/assimp/include"
	"${VKCAVE_HOME}/ThirdParty/glfw/include" )
target_include_directories(ShaderCompiler PUBLIC
	"${VKCAVE_HOME}"
	"${VKCAVE_HOME}/ThirdParty/shaderc/include"
	"${VKCAVE_HOME}/ThirdParty/shaderc/third_party/spirv-cross/include")

if(WIN32)
	target_include_directories(VkCave PUBLIC
		"$ENV{VULKAN_SDK}/include" "${VKCAVE_HOME}/ThirdParty/assimp/include" "${VKCAVE_HOME}/ThirdParty/glfw/include" )
	target_include_directories(Engine PUBLIC 
		"$ENV{VULKAN_SDK}/include" "${VKCAVE_HOME}/ThirdParty/assimp/include" "${VKCAVE_HOME}/ThirdParty/glfw/include" )
	target_include_directories(ShaderCompiler PUBLIC 
		"$ENV{VULKAN_SDK}/include" "${VKCAVE_HOME}/ThirdParty/SPIRV-Cross/include")

	target_compile_definitions(VkCave PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	target_compile_definitions(Engine PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	target_compile_definitions(ShaderCompiler PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	
	target_link_libraries(ShaderCompiler
		"Ws2_32.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${VKCAVE_HOME}/ThirdParty/shaderc/lib/shaderc_shared.lib"
		"${VKCAVE_HOME}/ThirdParty/shaderc/third_party/spirv-cross/lib/spirv-cross-core.lib" )

	target_link_libraries(Engine
		"Ws2_32.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${VKCAVE_HOME}/ThirdParty/glfw/lib/glfw3.lib"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/assimp.lib"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )

	target_link_libraries(VkCave
		"Ws2_32.lib"
		"${PROJECT_BINARY_DIR}/lib/Engine.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${VKCAVE_HOME}/ThirdParty/glfw/lib/glfw3.lib"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/assimp.lib"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )
	if (CMAKE_BUILD_TYPE STREQUAL "Debug")
		target_link_libraries(Engine "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstaticd.lib")
		target_link_libraries(VkCave "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstaticd.lib")
	else()
		target_link_libraries(Engine "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstatic.lib")
		target_link_libraries(VkCave "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstatic.lib")
	endif()

	if (${ENABLE_DEBUG_LAYERS})
		target_link_libraries(Engine "$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
		target_link_libraries(VkCave "${VKCAVE_HOME}/ThirdParty/assimp/lib/zlibstaticd.lib")
	endif()

	configure_file("${VKCAVE_HOME}/ThirdParty/shaderc/bin/shaderc_shared.dll" "${PROJECT_BINARY_DIR}/bin/shaderc_shared.dll" COPYONLY)
else()
	target_link_libraries(ShaderCompiler
		stdc++fs
		pthread
		"libvulkan.so.1"
		"${VKCAVE_HOME}/ThirdParty/shaderc/lib64/libshaderc_combined.a"
		"${VKCAVE_HOME}/ThirdParty/shaderc/third_party/spirv-cross/libspirv-cross.a" )

	target_link_libraries(Engine
		stdc++fs
		pthread
		X11
		"libvulkan.so.1"
		"${VKCAVE_HOME}/ThirdParty/glfw/lib64/libglfw3.a"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/libassimp.a"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/libzlibstatic.a"
		"${VKCAVE_HOME}/ThirdParty/assimp/lib/libIrrXML.a" )
	if (${ENABLE_DEBUG_LAYERS})
		target_link_libraries(Engine "libVkLayer_utils.so")
	endif()

	if (${LINK_PERFTOOLS_PROFILER})
		target_compile_options(Engine PUBLIC -Wl,--no-as-needed -lprofiler -Wl,--as-needed)
	endif()

	target_link_libraries(VkCave "${PROJECT_BINARY_DIR}/bin/libEngine.so" "libvulkan.so.1")
endif(WIN32)

if (${ENABLE_DEBUG_LAYERS})
	target_compile_definitions(VkCave PUBLIC -DENABLE_DEBUG_LAYERS)
	target_compile_definitions(Engine PUBLIC -DENABLE_DEBUG_LAYERS)
endif()

# GLFW defines
target_compile_definitions(Engine PUBLIC -DGLFW_INCLUDE_VULKAN)
target_compile_definitions(VkCave PUBLIC -DGLFW_INCLUDE_VULKAN)

# Copy config file
configure_file("config.json" "${PROJECT_BINARY_DIR}/bin/config.json" COPYONLY)

# Create symbolic link to Assets folder so the executable can find assets
add_custom_command(
	TARGET VkCave POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" "${PROJECT_BINARY_DIR}/bin/Assets"
	DEPENDS "${PROJECT_BINARY_DIR}/bin/Assets"
	COMMENT "Linking ${CMAKE_CURRENT_SOURCE_DIR}/Assets/ to ${PROJECT_BINARY_DIR}/bin/Assets" )

# Compile shaders
add_shader_target(Shaders "Shaders/")
add_dependencies(VkCave Shaders Engine)

# Build all plugins
add_subdirectory("Plugins/")
