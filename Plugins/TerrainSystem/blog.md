Stratum

Trevor Hedstrom

Stratum is a high-performance Vulkan rendering engine with multi-GPU support, and minimal dependencies. It provides developers with rapid
prototyping capabilities and easy extensibility for all varieties of rendering implementations.

# Features
* Automatic (no config files!) plugin loading with prioritized sorting
* Shader compiler with macro switches and reflection for easy parameter handling
* Math library allowing for auto-vectorized SIMD math on the CPU, that mimics HLSL allowing for shared include files between shaders and C++
* Easily extensible scene engine, provides suppor for arbitrary renderers and lights, with instanced batching support for normal mesh renderers
* Shadow system providing arbitrary amoutns of cascaded directional shadows, point shadows, and spot shadows via one large shadow atlas
* Global environment system to provide atmospheric and environment data to each renderer (even custom ones) to create a homogeneous scene style

## Terrain System Plugin
This system renders terran with dynamic LOD, as well as providing camera movement in a traditional FPS-style.
The terrain is generated on the GPU via a custom vertex shader (similar to displacement mapping), with no vertex buffers used.
Various index buffers are used to fan triangles between LOD levels to prevent cracks. The same math used to generate terrain height
is re-used on the CPU to represent the terrain without having to transfer data between the CPU and GPU.
![Terrain](https://i.imgur.com/9yNSXfn.png)

## Cascaded Shadow Maps with PCF
Cascaded Shadow Maps are a common approach to shadow large areas with minimal projective aliasing. A precomputed Poisson lookup table
is used to perform a 7x7 24-tap PCF filter when sampling the shadowmap, which creates smooth edges, hiding texel aliasing and providing
the appearance of a penumbra.
![Shadows](https://i.imgur.com/0MMcHFw.png)

## Precomputed Realtime Atmospheric Scattering
Realtime atmospheric scattering requires integrating light bounces along the view ray. This integral is not solvable analytically, and thus
must be solved iteravely, per-pixel. To accelerate this, scattering is broken into inscattering and outscattering (aka extinction) and their intensities
are precomputed the beginning of the frame to a small 3D texture. This texture serves as a lookup table, whos indices are pixel coordinate and depth.
Sampling these lookup tables at the appropriate depths using simple bilinear filtering during rendering provides massive speedups. Additionally, the process
of computing the lookup tables per-frame can be accelerated by precomputing other integrals, such as the optical density of the atmosphere at a given height and view azimuth.

In total, there are 2 lookup tables generated at the start of the program:
* Particle/Atmosphere Density by view azimuth
* Skybox Rayleigh and Mei scattering (i.e. total scattering at infinite depth) by camera height in atmosphere, view azimuth, and sun azimuth (this table is broken into 2 textures to store Rayleigh and Mei scattering)

Additionally, there are 3 lookup tables generated at the start of each frame:
* Inscattering by pixel coordinate and depth
* Outscattering (extinction) by pixel coordinate and depth
* Scattering attenuation by pixel coordinate

The last lookup table (and most expensive to compute) roughly approximates volumetric light shafts by sampling the sun's shadow maps for each pixel (this is done at half-resolution)
along the view ray and accumulating the total shadow attenuation. This is simply multiplied into the final inscattering value for each pixel.
![Scattering](https://i.imgur.com/6VPDNMi.png)
![Sky](https://i.imgur.com/vxACLtK.png)
![Light Shafts](https://i.imgur.com/Agn5XNF.png)

## Day/Night Cycle
Since the amospheric scattering is computed in realtime, a fully dynamic day/night cycle is possible via rotating the sun and moon lights as well as rotating a star cubemap to
simulate the rotation of the world.
![Night Time](https://i.imgur.com/cQMBzWx.png)