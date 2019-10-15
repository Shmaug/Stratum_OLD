# vkCAVE

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.


## How to Build

Install the LunarG VulkanSDK and make sure the VULKAN_SDK environment variable points to it (should be part of the install process).

Then, clone the submodules:

git submodule update --init


Use CMake to compile each submodule. Make sure each submodule's CMAKE_INSTALL_PREFIX is the same as the submodule's source directory.

For assimp:

CMAKE_DEBUG_POSTFIX=""

LIBRARY_SUFFIX=""

ASSIMP_BUILD_ZLIB=TRUE

BUILD_SHARED_LIBS=FALSE


For GLFW:

BUILD_SHARED_LIBS=FALSE


For SPIRV-Cross:

On CentOS, simply run "make", on Windows, build with CMake.

On Windows, shaderc must be built manually. It can be found in VULKAN_SDK/shaderc. Simply use CMake to build it into VULKAN_SDK/shaderc.
