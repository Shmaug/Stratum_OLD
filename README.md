# Stratum

High performance modular plugin-based Vulkan rendering engine in C++17 with minimal dependencies.

## How to Build
run `setup.bat` or `setup.sh`, then build Stratum with CMake

# API Overview
- `Instance`
  - Wraps `VkInstance`
  - **Useful functions**:
    - `Instance::Device()`: The device being used by Stratum
    - `Instance::Window()`: The window being used by Stratum
    - `Instance::MaxFramesInFlight()`: Tells the total number of frames in flight on the CPU
- `Device`
  - Wraps `VkDevice`
  - Accessible through `Instance::Device()`
  - Tracks current frame data
    - Useful for grabbing a `Buffer` or `DescriptorSet` used for a particular render pass
  - **Useful functions**:
    - `Device::GetTempBuffer()`: Get a buffer that is valid for this frame
    - `Device::GetTempDescriptorSet`: Get a descriptor set that is valid for the current frame
    - `Device::MaxFramesInFlight()`: Tells the total number of frames in flight on the CPU
    - `Device::FrameContextIndex()`: Tells the index of the current frame. Between 0 and MaxFramesInFlight-1
- `Buffer`
  - Wraps `VkBuffer`
  - Represents a buffer of data on the GPU
  - Automatically handles staging buffers depending on the supplied usage flags
- `DescriptorSet`
  - Wraps `VkDescriptorSet`
  - Descriptor writes are buffered
    - Must call `DescriptorSet::FlushWrites()` to actually write them
- `CommandBuffer`
  - Wraps `VkCommandBuffer`
  - Passed in by most rendering functions
  - Tracks active `RenderPass`, `Camera`, `Material`, `Shader` vertex buffer, index buffer, and more to avoid unecessary API calls
  - **Useful functions**:
    - `CommandBuffer::BindShader()`: Binds a shader pipeline. Tracks active `Shader`, and `Camera`
    - `CommandBuffer::BindMaterrial()`: Binds the shader pipline from the material. Tracks active `Material`, `Shader`, and `Camera`
    - `CommandBuffer::BindVertexBuffer()`: Binds a vertex buffer and sets it as active
    - `CommandBuffer::BindIndexBuffer()`: Binds an index buffer and sets it as active
    - `CommandBuffer::BeginRenderPass()`: Begins a `RenderPass` and sets it as active.
    - `CommandBuffer::EndRenderPass()`: Ends a `RenderPass` and un-sets it as active.
    - `CommandBuffer::PushConstant()`: Pushes a PushConstant value. Checks to see if the provided shader supports the PushConstant before attempting to push.
- `RenderPass`
  - Wraps `VkRenderPass`
  - Active `RenderPass` tracked by `CommandBuffer`
  - Stores a `Framebuffer`
  - Automatically created by each `Camera` (unless manually supplied)
- `Framebuffer`
  - Wraps `VkFramebuffer`
  - Automatically created by each `RenderPass` (unless manually supplied)
- `Sampler`
  - Wraps `VkSampler`
- `Window`
  - Provides an abstraction for the window being used to render Stratum

# Scene Overview
## Note: Stratum uses a **left-handed** coordinate system!
- `Scene`
  - Stores a collection of Objects
    - See `Scene::AddObject()` and `Scene::RemoveObject()` (objects wont work unless they are first added to the scene)
  - Computes a binary BVH for the whole scene
    - Allows for raycasting for objects that implement `Object::Intersect()`
  - Computes active lights and shadows, which can be references with `Scene::LightBuffer()`, `Scene::ShadowBuffer()`, and `Scene::ShadowAtlas()`
  - Stores an `AssetManager`, `InputManager`, and `PluginManager`
  - Use `Scene::LoadModelScene()` to efficiently load multiple `MeshRenderer`s (or `SkinnedMeshRenderer`s) from one 3D file
    - Stores all vertices and indices in the same buffer
  - Computes timing
    - `Scene::TotalTime()`: Total time in seconds since Stratum has started
    - `Scene::DeltaTIme()`: Delta time in seconds between last frame and the current frame
- `Object`
  - Base class for all Scene Objects. Stores a Position, Rotation, and Scale that is used to compute an object-to-parent matrix (see `Object::ObjectToParent()`). Objects can have other Objects within them as children, allowing for hierarchical transforms.
  - Almost completely virtual, designed to be inhereted from
  - Supports "masking" (see `Object::LayerMask()`) in order to classify different objects
- `Camera`
  - Inherets `Object`. Computes View matrices based on its transform as an `Object`
  - Represents a camera in 3D space. Provides functionality for stereo rendering, and more
  - **Note: For precision, Camera view matrices are always centered at the origin.** Thus the 
- `Renderer`
  - Base class for all Renderers. Inherit and override this to implement a custom renderer.
- `MeshRenderer`
  - Renders a `Mesh` with a `Material`. The Scene will try to batch together MeshRenderers that use the same Mesh, Material and RenderQueue and draw them using Instancing.
- `SkinnedMeshRenderer`
  - Renders a skinned `Mesh` with a `Material`. Inherets `MeshRenderer`, computes skinning from an `AnimationRig`.
- `ClothRenderer`
  - Renders a `Mesh` simulating soft-body spring physics along triangle edges, and aerodynamic drag along triangle faces.
- `Light`
  - Defines a light within the Scene's lighting system.
- `Gizmos`
  - Gizmos are a way to quickly draw simple 3D shapes such as boxes and lines, and are only drawn during the main pass when `Scene::DrawGizmos()` is `true`. To use these properly, only call `Gizmos` functions during `DrawGizmos()` events.
- `GUI`
  - Provides a basic immediate-mode GUI system, similar to Unity's EditorGUI. It can draw GUI content in world-space and in screen-space.

# Content Overview
- `Asset`
  - Represents any Asset loadable by the `AssetManager`
  - Used internally by the `AssetManager`
  - Inherit this if you intend to implement a custom Asset type (custom asset loaders within `AssetManager` planned)
- `AssetManager`
  - Loads and tracks Assets. Assets loaded using this do **not** have to be manually deleted. Use this to load common assets.
- `Mesh`
  - Stores a collection of vertices and indices on the GPU
    - This means vertices and indices are not accessible unless otherwise stored
  - Also stores weight data and shape key data for animations
  - Also can store a triangle BVH (for raycasting)
  - Static functions for creating cubes and planes (`Mesh::CreateCube()` and `Mesh::CreatePlane()`)
- `Font`
  - Represents a rasterized TrueType (*.ttf) font at a specific pixel size
  - Can draw strings in the world or on the screen with `DrawString()` or `DrawString()`
- `Texture`
  - Stores a texture on the GPU
  - Can compute mipmaps in the constructor
- `Shader`
  - Represents a shader compiled with Stratums ShaderCompiler. The ShaderCompiler uses reflection to determine the layout, passes, and other metadata included within shaders
  - Stores both compute and graphics shaders
  - Use `GetGraphics()` and `GetCompute()` to get usable shader *variants*.
- `Material`
  - Represents a Shader with a collection of parameters. Used by `MeshRenderer`

# Shader Overview
Stratum provides a custom shader compiler. It uses SPIRV reflection and custom directives to support automatic generation of data such as descriptor and pipeline layouts. It also provides macro variants, similar to Unity. Here is a list of all supported directives:
- `#pragma vertex <function> <main/depth>`
  - Specifies vertex shader entrypoint. `pass` is optional, and defaults to `main`
- `#pragma fragment <function> <main/depth>`
  - Specifies fragment shader entrypoint. `pass` is optional, and defaults to `main`
- `#pragma kernel <function>`
  - Specifies a kernel entrypoint for a compute shader
- `#pragma multi_compile <keyword1> <keyword2> ...`
  - Specifies shader *variants*. The shader is compiled multiple times, defining a different keyword each time. These different compilations are referred to as *variants*.
- `#pragma render_queue <number>`
  - Specifies the render queue to be used by this shader. Used by `Material`.
- `#pragma color_mask <mask>`
  - Specifies a color write mask for the fragment shader. `<mask>` must contain subset of the characters `rgba`. Examples:
    - `#pragma color_mask rgb`
    - `#pragma color_mask rgba`
    - `#pragma color_mask rg`
- `#pragma zwrite <true/false>`
  - Specifies whether this shader writes to the zbuffer
- `#pragma ztest <true/false>`
  - Specifies whether this shader uses depth-testing
- `#pragma depth_op <less/greater/lequal/gequal/equal/nequal/never/always`>`
  - Specifies the depth compare op.
- `#pragma cull <front/back/false>`
  - Specifies the culling mode
- `#pragma fill <solid/line/point>`
  - Specifes the fill mode
- `#pragma blend <opaque/alpha/add/multiply>`
  - Specifies the blend mode
- `#pragma array <name> <number>`
  - Specifies that the descriptor named `<name>` is an array of size `<number>`. This is used in addition to specifying the descriptor as an array in native syntax (GLSL or HLSL)
- `#pragma static_sampler <name> <magFilter=linear> <minFilter=linear> <filter=linear> <addressModeU=repeat> <addressModeV=repeat> <addressModeW=repeat> <addressMode=repeat> <maxAnisotropy=2> <borderColor=int_opaque_black> <unnormalizedCoordinates=false> <compareOp=always> <mipmapMode=linear> <minLod=0> <maxLod=12> <mipLodBias=0>`
  - Specifies that the sampler descriptor named `<name>` is a static/immutable sampler. All arguments after `<name>` are optional and defaulted to the above values, and can be specified as `argument=value` Examples:
    - `#pragma static_sampler ShadowSampler maxAnisotropy=0 maxLod=0 addressMode=clamp_border borderColor=float_opaque_white compareOp=less`

# Frame Overview
Each frame follows the following sequence of events:
- `InputDevice::NextFrame()`
- Acquire SwapChain image
- Get a `CommandBuffer`
- Scene Update
  - Fixed Update Loop
      - `Object::FixedUpdate()`
      - `Plugin::FixedUpdate()`
  - `Plugin::PreUpdate()`
  - `Plugin::Update()`
  - `Plugin::PostUpdate()`
- Render
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
- Execute `CommandBuffer`
- `Plugin::PrePresent()`
- Wait for GPU to finish the oldest buffered frame before continuing (triple-buffering)

## Render Pass Overview
Each "Render `<PASS>`" call above follows the following sequence of events:
- Use Scene BVH to find Renderers in view
- Sort Renderers based on `RenderQueue`
- `Camera::PreRender()` (Updates Camera Framebuffer and Viewport)
- `Plugin::PreRender()`
- `Renderer::PreRender()`
- Begin RenderPass, clear Framebuffer if `clear` is `true`
- `Camera::Set()` (Updates Camera Uniform buffer and sets Viewport and Scissor)
- `Plugin::PreRenderScene()`
- Render Skybox (only for `PASS_MAIN`)
- Render loop
  - For `MeshRenderer`s with the same `RenderQueue`, `Material`, and `Mesh` (given the `Material`'s shader supports Instancing by using the `Instances` uniform):
    - `MeshRenderer::DrawInstanced()`
  - For other `Renderer`s:
    - `Renderer::Draw()`
- Draw Gizmos (only if pass is `PASS_MAIN` and `Scene::DrawGizmos` is `true`)
  - `Object::DrawGizmos()`
  - `Plugin::DrawGizmos()`
- Draw GUI (only for `PASS_MAIN`)
- `Plugin::PostRenderScene()`
- End RenderPass

## Stereo Rendering
`Camera`s have a `StereoMode` property which is implemented as such:
- While rendering a `Renderer`:
  - For each `STEREO_EYE`
    - Call `Camera::SetStereo()`
      - This sets the correct Scissor and `StereoEye` push constant
    - Render your Renderer
      - See `Render()` in `Scene/MeshRenderer.cpp` for an example
- View matrices for each eye computed by multiplying the Camera's non-stereo View matrix by the EyeTransform of each eye (see `Camera::EyeTransform`)
- Projection matrices are either manually supplied (see `Camera::Projection`) or computed from the `Near`, `Far`, and `FieldOfView` parameters (or `OrthographicSize` if `Orthographic` is set)