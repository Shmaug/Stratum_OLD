Stratum

Trevor Hedstrom

Stratum is a high-performance Vulkan rendering engine with multi-GPU support, and minimal dependencies. It provides developers with rapid
prototyping capabilities and easy extensibility for all varieties of rendering implementations.

# Features
* Math library allowing for auto-vectorized SIMD math on the CPU, that mimics HLSL allowing for shared include files between shaders and C++
* Automatic (no config files!) plugin loading with prioritized sorting
* Easily extensible scene engine, provides suppor for arbitrary renderers and lights
* Shadow system providing arbitrary amoutns of cascaded directional shadows, point shadows, and spot shadows via one large shadow atlas
* Shader compiler with macro switches and reflection for easy parameter handling

## Screenshot: Terrain System
This system renders terrain with dynamic LOD, as well as providing camera movement in a traditional FPS-style.
The terrain is generated on the GPU via a custom vertex shader (similar to displacement mapping), but the math is re-used on the CPU
to represent the terrain without having to transfer data between the CPU and GPU.
![Terrain](https://i.imgur.com/2gOm7r0.png)