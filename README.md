# vkCAVE

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.


## How to Build

Install the LunarG VulkanSDK and make sure the VULKAN_SDK environment variable points to it (should be part of the install process).

Then, clone the submodules:

git submodule update --init

On Windows, the submodules need to be build from source.  Make sure each submodule's CMAKE_INSTALL_PREFIX is the same as the submodule's source directory.
shaderc contains SPIRV-Cross in shaderc/third_party, this must be build from source as well. For linux: simply run make in shaderc/third_party/spirv_cross. On Windows, use cmake
to build it into shaderc/third_party/spirv_cross.

### CMake Settings

Assimp:

CMAKE_DEBUG_POSTFIX=""

LIBRARY_SUFFIX=""

ASSIMP_BUILD_ZLIB=TRUE

BUILD_SHARED_LIBS=FALSE


GLFW:

BUILD_SHARED_LIBS=FALSE


## TODO

- Proper scene BVH
- Shadow mapping
- Move all camera uniforms to a single buffer
- Tiled lighting
- Atmospheric scattering
- Linearly Transformed Cosine based area lights
- Faster unordered_map?
- Dynamic per-frame GPU memory allocator