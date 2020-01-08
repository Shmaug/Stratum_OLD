# Stratum

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.

## How to Build

On Windows: install the LunarG VulkanSDK and make sure the VULKAN_SDK environment variable points to it.

On Linux: Install the `vulkan` and `vulkan-devel` packages.

Then, clone the submodules:

git submodule update --init

Build the submodules into their respective folders with `BUILD_SHARED_LIBS` turned `OFF` to link statically.
For Assimp, enable `ASSIMP_BUILD_ZLIB` unless  zlib is already installed. Also make sure to remove any
library suffixes (namely for Assimp).
Builduing any testing, examples, and any binary tools for the dependencies is not required and disabling them
might reduce compilation time for the dependencies.