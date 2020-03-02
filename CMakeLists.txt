cmake_minimum_required (VERSION 2.8)

option(ENABLE_DEBUG_LAYERS "Enable debug layers?" TRUE)
set(STRATUM_HOME ${CMAKE_CURRENT_SOURCE_DIR} CACHE PATH "Directory of Stratum")

include(stratum.cmake)

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
	"Scene/Camera.cpp"
	"Scene/ClothRenderer.cpp"
	"Scene/Gizmos.cpp"
	"Scene/GUI.cpp"
	"Scene/Light.cpp"
	"Scene/MeshRenderer.cpp"
	"Scene/Environment.cpp"
	"Scene/Scene.cpp"
	"Scene/Object.cpp"
	"Scene/ObjectBvh2.cpp"
	"Scene/SkinnedMeshRenderer.cpp"
	"Scene/TriangleBvh2.cpp"
	"ThirdParty/imp.cpp"
	"Util/Tokenizer.cpp"
	"Util/Profiler.cpp" )
add_executable(ShaderCompiler "Stratum/ShaderCompiler.cpp")
add_executable(Stratum "Stratum/Stratum.cpp" "ThirdParty/json11.cpp" "stratum.rc")

set_target_properties(Engine Stratum ShaderCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine Stratum ShaderCompiler PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/bin/")
set_target_properties(Engine Stratum ShaderCompiler PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/lib/")

target_compile_definitions(Engine PUBLIC -DENGINE_CORE)
target_compile_definitions(ShaderCompiler PUBLIC -DENGINE_CORE)
target_compile_definitions(Stratum PUBLIC -DENGINE_CORE)

target_include_directories(Stratum PUBLIC
	"${STRATUM_HOME}"
	"${STRATUM_HOME}/ThirdParty/assimp/include" )
target_include_directories(Engine PUBLIC
	"${STRATUM_HOME}"
	"${STRATUM_HOME}/ThirdParty/assimp/include" )
target_include_directories(ShaderCompiler PUBLIC
	"${STRATUM_HOME}"
	"${STRATUM_HOME}/ThirdParty/shaderc/include"
	"${STRATUM_HOME}/ThirdParty/shaderc/third_party/spirv-cross/include")

if(WIN32)
	target_include_directories(Stratum PUBLIC "$ENV{VULKAN_SDK}/include" "${STRATUM_HOME}/ThirdParty/assimp/include")
	target_include_directories(Engine PUBLIC "$ENV{VULKAN_SDK}/include" "${STRATUM_HOME}/ThirdParty/assimp/include")
	target_include_directories(ShaderCompiler PUBLIC "$ENV{VULKAN_SDK}/include" "${STRATUM_HOME}/ThirdParty/SPIRV-Cross/include")

	target_compile_definitions(Stratum PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	target_compile_definitions(Engine PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	target_compile_definitions(ShaderCompiler PUBLIC -DWINDOWS -DWIN32_LEAN_AND_MEAN -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
	
	target_link_libraries(ShaderCompiler
		"Ws2_32.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${STRATUM_HOME}/ThirdParty/shaderc/lib/shaderc_combined.lib"
		"${STRATUM_HOME}/ThirdParty/shaderc/third_party/spirv-cross/lib/spirv-cross-core.lib" )

	target_link_libraries(Engine
		"Ws2_32.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/assimp.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/zlibstatic.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )

	target_link_libraries(Stratum
		"Ws2_32.lib"
		"${PROJECT_BINARY_DIR}/lib/Engine.lib"
		"$ENV{VULKAN_SDK}/lib/vulkan-1.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/assimp.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/zlibstatic.lib"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/IrrXML.lib" )

	if (${ENABLE_DEBUG_LAYERS})
		target_link_libraries(Engine "$ENV{VULKAN_SDK}/lib/VkLayer_utils.lib")
	endif()
else()
	target_link_libraries(ShaderCompiler
		stdc++fs
		pthread
		"libvulkan.so.1"
		"${STRATUM_HOME}/ThirdParty/shaderc/lib64/libshaderc_combined.a"
		"${STRATUM_HOME}/ThirdParty/shaderc/third_party/spirv-cross/libspirv-cross.a" )

	target_link_libraries(Engine
		stdc++fs
		pthread
		"libvulkan.so.1"
		"libX11.so"
		"libXrandr.so"
		"libX11-xcb.so"
		"libxcb-keysyms.so"
		"libxcb-randr.so"
		"libz.so"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/libassimp.a"
		"${STRATUM_HOME}/ThirdParty/assimp/lib/libIrrXML.a" )

	target_link_libraries(Stratum "${PROJECT_BINARY_DIR}/bin/libEngine.so" "libvulkan.so.1")
endif(WIN32)

if (${ENABLE_DEBUG_LAYERS})
	target_compile_definitions(Stratum PUBLIC -DENABLE_DEBUG_LAYERS)
	target_compile_definitions(Engine PUBLIC -DENABLE_DEBUG_LAYERS)
endif()

# Create symbolic link to Assets folder so the executable can find assets
add_custom_command(
	TARGET Stratum POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/Assets/" "${PROJECT_BINARY_DIR}/bin/Assets"
	DEPENDS "${PROJECT_BINARY_DIR}/bin/Assets"
	COMMENT "Linking ${CMAKE_CURRENT_SOURCE_DIR}/Assets/ to ${PROJECT_BINARY_DIR}/bin/Assets" )

# Compile shaders
add_shader_target(Shaders "Shaders/")
add_dependencies(Stratum Shaders Engine)

# Build all plugins
add_subdirectory("Plugins/")
