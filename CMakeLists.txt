cmake_minimum_required (VERSION 3.8)

project(VkCave)

set(CMAKE_CXX_STANDARD 17)

# Check for dependencies

if(DEFINED ENV{VULKAN_SDK})
    message(STATUS "Found VULKAN_SDK")
else()
    message(FATAL_ERROR "Error: VULKAN_SDK not set!")
endif()

if(DEFINED ENV{SPIRV_CROSS_HOME})
    message(STATUS "Found SPIRV_CROSS_HOME")
else()
    message(FATAL_ERROR "Error: SPIRV_CROSS_HOME not set!")
endif()

if(DEFINED ENV{GLFW_HOME})
    message(STATUS "Found GLFW_HOME")
else()
    message(FATAL_ERROR "Error: GLFW_HOME not set!")
endif()

if(DEFINED ENV{GLM_HOME})
    message(STATUS "Found GLM_HOME")
else()
    message(FATAL_ERROR "Error: GLM_HOME not set!")
endif()

if(DEFINED ENV{ASSIMP_HOME})
    message(STATUS "Found ASSIMP_HOME")
else()
    message(FATAL_ERROR "Error: ASSIMP_HOME not set!")
endif()

# Include dependencies
include_directories(
	"$ENV{VULKAN_SDK}/include"
	"$ENV{SPIRV_CROSS_HOME}/include"
	"$ENV{GLFW_HOME}/include"
	"$ENV{GLM_HOME}/include"
	"$ENV{ASSIMP_HOME}/include"
	"." )

if(WIN32)
	set(WINSDK_VERSION "10.0.18362.0")
	# Link Windows SDK
	# This should happen automatically...
	include_directories(
		"$ENV{WinSDK}/Include/${WINSDK_VERSION}/um"
		"$ENV{WinSDK}/Include/${WINSDK_VERSION}/ucrt"
		"$ENV{WinSDK}/Include/${WINSDK_VERSION}/shared" )
	link_directories(
		"$ENV{WinSDK}/Lib/${WINSDK_VERSION}/um/x64"
		"$ENV{WinSDK}/Lib/${WINSDK_VERSION}/ucrt/x64" )
		
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

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd26812 /wd4251") # unscoped enum, arithmetic overflow
endif()

# GLM and GLFW defines
add_definitions(
	-DGLM_FORCE_RADIANS
	-DGLM_FORCE_DEPTH_ZERO_TO_ONE
	-DGLM_FORCE_LEFT_HANDED
	-DGLM_FORCE_SSE2
	-DGLM_ENABLE_EXPERIMENTAL
	-DGLFW_INCLUDE_VULKAN )

# Build shader compiler
add_executable(ShaderCompiler "shadercompiler.cpp")
set_target_properties(ShaderCompiler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin/")
set_target_properties(ShaderCompiler PROPERTIES LIBRARY_OUTPUT_DIRECTORY "bin/")
set_target_properties(ShaderCompiler PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "lib/")

# Build engine library
add_library(Engine SHARED
	"Content/AssetDatabase.cpp"
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
	"Scene/MeshRenderer.cpp"
	"Scene/Scene.cpp"
	"Scene/Object.cpp"
	"Scene/TextRenderer.cpp"
	"ThirdParty/imp.cpp"
	"Util/Profiler.cpp" )
target_compile_definitions(Engine PUBLIC -DENGINE_CORE)
set_target_properties(Engine PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin/")
set_target_properties(Engine PROPERTIES LIBRARY_OUTPUT_DIRECTORY "bin/")
set_target_properties(Engine PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "lib/")

# Build main executable
add_executable(VkCave "vkcave.cpp" "ThirdParty/json11.cpp")
set_target_properties(VkCave PROPERTIES RUNTIME_OUTPUT_DIRECTORY "bin/")
set_target_properties(VkCave PROPERTIES LIBRARY_OUTPUT_DIRECTORY "bin/")
set_target_properties(VkCave PROPERTIES ARCHIVE_OUTPUT_DIRECTORY "lib/")

if(WIN32)
	# Disable warnings
	set_target_properties(ShaderCompiler Engine VkCave PROPERTIES COMPILE_FLAGS "${BUILD_FLAGS} /wd26451") # uninitialzed struct variable

	# Link engine
	target_link_libraries(VkCave "Ws2_32.lib" "${CMAKE_BINARY_DIR}/lib/Engine.lib")
	# Link shaderc and spirv-cross
	target_link_libraries(ShaderCompiler
		"$ENV{VULKAN_SDK}/shaderc/install/lib/shaderc_shared.lib"
		"$ENV{SPIRV_CROSS_HOME}/lib/spirv-cross-core.lib" )

	# Copy required DLLs
	configure_file("$ENV{ASSIMP_HOME}/bin/x64/assimp-vc140-mt.dll" "bin/assimp-vc140-mt.dll" COPYONLY)
	configure_file("$ENV{VULKAN_SDK}/shaderc/install/bin/shaderc_shared.dll" "bin/shaderc_shared.dll" COPYONLY)
elseif(UNIX)
    message(FATAL_ERROR "Error: Not implemented!")
else()
    message(FATAL_ERROR "Error: Not implemented!")
endif(WIN32)

# Copy config file
configure_file("config.json" "bin/config.json" COPYONLY)

# Build all plugins
add_subdirectory("Plugins/")

# Create symbolic link to Assets folder so the executable can find assets
add_custom_command(
	TARGET VkCave POST_BUILD
	COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/Assets/" "${PROJECT_BINARY_DIR}/bin/Assets"
	DEPENDS "${PROJECT_BINARY_DIR}/bin/Assets"
	COMMENT "Symbolic link assets folder from ${source} => ${destination}" )

# Compile shaders in Shaders/* using ShaderCompiler
file(GLOB_RECURSE SHADER_SOURCES
    "Shaders/*.frag"
    "Shaders/*.vert"
    "Shaders/*.glsl"
    "Shaders/*.hlsl" )

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

add_custom_target(Shaders DEPENDS ${SPIRV_BINARY_FILES})
add_dependencies(Shaders ShaderCompiler)
add_dependencies(VkCave Shaders)
add_dependencies(VkCave Engine)