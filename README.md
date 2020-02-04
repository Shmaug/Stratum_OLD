# Stratum

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.

## How to Build

On Windows: install the LunarG VulkanSDK and make sure the VULKAN_SDK environment variable points to it.

On Linux: Install the `vulkan` and `vulkan-devel` packages.

Then, clone the submodules:

`git submodule update --init`

Build the submodules into their respective folders using CMake with `BUILD_SHARED_LIBS` turned `OFF` to link statically. For Assimp, enable `ASSIMP_BUILD_ZLIB` unless zlib is already installed and accessible by Stratum. Also make sure to remove any library suffixes (namely for Assimp).

Builduing any testing, examples, and any binary tools for the dependencies is not required and disabling them might reduce compilation time for the dependencies.

# Frame Overview
- `InputDevice::NextFrame()`
- Acquire SwapChain image
- Scene Update
  - `Plugin::PreUpdate()`
  - `Plugin::Update()`
  - `Plugin::PostUpdate()`
- Render
  - Get a `CommandBuffer`
  - Scene PreFrame
    - `Renderer::PreFrame()`
    - Sort Cameras, use highest-priority Camera as the main camera
    - Compute active lights & shadow cameras
    - For each shadow-casting light Render `PASS_DEPTH`
    - Resolve ShadowAtlas
  - Render `PASS_MAIN` for each camera (highest priorty first) 
  - Resolve cameras
  - `Plugin::PostProcess()`
  - Copy cameras with `TargetWindow` set to the screen
  - `Camera::PostRender()`
  - Execute the `CommandBuffer`
- Wait for GPU to finish the oldest buffered frame before continuing (triple-buffering)
## Render Pass Overview
For each "Render `<PASS>`" call above, the following occurs:
- Use Scene BVH to find Renderers in view
- Sort Renderers based on `RenderQueue`
- `Camera::PreRender()` (Updates Camera Framebuffer and Viewport)
- `Plugin::PreRender()`
- `Renderer::PreRender()`
- Begin RenderPass
- Clear Framebuffer (if `clear = true`)
- `Camera::Set()` (Updates Camera Uniform buffer and sets Viewport and Scissor)
- `Plugin::PreRenderScene()`
- Render Skybox (only for `PASS_MAIN`)
- Render loop
  - For `MeshRenderer`s with the same `RenderQueue`, `Material`, and `Mesh` (given the `Material`'s shader supports Instancing by using the `Instances` uniform):
    - `MeshRenderer::DrawInstanced()`
  - For other `Renderer`s:
    - `Renderer::Draw()`
- Draw Gizmos (only for `PASS_MAIN` and `Scene::DrawGizmos` is `true`)
  - `Object::DrawGizmos()`
  - `Plugin::DrawGizmos()`
- `Plugin::PostRenderScene()`
- End RenderPass