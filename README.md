# Stratum

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.

## How to Build

Install the LunarG VulkanSDK and make sure the VULKAN_SDK environment variable points to it. Then, clone the submodules:

git submodule update --init

Build the submodules into their respective folders.
Make sure to build them to be linked statically. For Assimp, enable ASSIMP_BUILD_ZLIB unless 
zlib is already installed. It is recommended to disable testing, examples, and any binary tools
that the submodules might build as they are unneccesary. Also make sure to remove any library suffixes
(namely for Assimp).

## TODO

- Proper scene BVH
- Fix multi-gpu swapchain creation on Linux
- Linearly Transformed Cosine based area lights
- Faster unordered_map?