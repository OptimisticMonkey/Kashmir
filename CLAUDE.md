# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & run

CMake project, Windows / MSVC generator. The `build/` directory is already configured.

```bash
cmake --build build --config Debug      # incremental build (engine + shaders)
cmake --build build --config Release
cmake -S . -B build                     # reconfigure if CMakeLists changes
```

Output: `bin/Debug/engine.exe`. Runtime DLLs (SDL3, etc.) are copied to that directory by a `POST_BUILD` step on the `engine` target. The VS Code task `CMake: Build Debug` is the default build task; `launch.json` sets `cwd` to `bin/Debug`.

There is no test suite.

## Asset & shader paths — critical

The engine resolves paths relative to the **launch CWD**, which is `bin/Debug/`. Code uses literal Windows relative paths like `..\\..\\assets\\Suzanne.glb` and `../../shaders/mesh.frag.spv`. If you change the working directory or output layout you will break asset loading silently (some loads use `assert(...has_value())`).

Shaders are compiled to SPIR-V by a CMake custom target `Shaders` that invokes `glslangValidator` (found via `$VULKAN_SDK/Bin/`). It globs `shaders/*.{frag,vert,comp}` — HLSL files (`.hlsl`) in `shaders/` are NOT auto-compiled by this glob and must be compiled manually if used. New GLSL shaders are picked up only after re-running CMake configure.

## Dependencies

- **SDL3** is consumed via `find_package(SDL3 CONFIG REQUIRED)` from `$VULKAN_SDK/cmake`. The Vulkan SDK ships SDL3; `third_party/SDL/` (SDL2 sources) is intentionally NOT built — its `add_subdirectory` is commented out in `third_party/CMakeLists.txt`.
- Built in-tree from `third_party/`: vk-bootstrap, VMA (header-only), glm (header-only), fmt, fastgltf, stb_image (header-only), imgui (with `imgui_impl_sdl3` + `imgui_impl_vulkan` backends).
- `GLM_FORCE_DEPTH_ZERO_TO_ONE` is defined for the engine target — projection matrices match Vulkan's [0,1] depth range.
- Top-level CMake sets C++23, but the `engine` target overrides to C++20. vk-bootstrap is also C++20.

## Architecture

Single-binary engine following the vkguide.dev structure. Entry point is `src/main.cpp` → `VulkanEngine::{init, run, cleanup}`.

**`VulkanEngine`** ([src/vk_engine.h](src/vk_engine.h)) owns essentially all Vulkan state. There is a global `loadedEngine` and a `VulkanEngine::Get()` accessor. Init order matters: `init_vulkan → init_swapchain → init_commands → init_sync_structures → init_descriptors → init_pipelines → init_imgui → init_default_data`. The pipeline init fans out to `init_background_pipelines` (compute), `init_update_transform_pipeline` (compute), `init_triangle_pipeline`, `init_mesh_pipeline`, `metalRoughMaterial.build_pipelines`, and `init_ground_pipeline` — order matters because `init_default_data` consumes the material pipelines.

**Per-frame state.** `FRAME_OVERLAP = 2`: command pool/buffer, swapchain & render semaphores, render fence, frame-local `DeletionQueue`, and a `DescriptorAllocatorGrowable`. `get_current_frame()` indexes by `_frameNumber % FRAME_OVERLAP`. Present semaphores are stored *per swapchain image* (`_presentSemaphores`) and are rebuilt when the swapchain is recreated — do not assume they map 1:1 to frames.

**Resource cleanup** uses `DeletionQueue` (a deque of `std::function<void()>` flushed in reverse). There is one `_mainDeletionQueue` for engine-lifetime resources and one per `FrameData` for per-frame transients. Always push the destroy lambda right after creation.

**Render flow per frame** (in `draw()` at `src/vk_engine.cpp:1119`):
1. `update_scene()` — refreshes camera, fills `GPUSceneData` (view/proj/lights/cameraPos), spawns Suzanne instances over time, calls `Node::Draw` to populate `mainDrawContext.OpaqueSurfaces`, and queues the ground.
2. `update_transform(cmd)` — compute dispatch (`update_transform.comp`) writes per-instance world matrices into the instance transform buffer.
3. `draw_background(cmd)` — selected `ComputeEffect` (gradient/sky/etc.) writes the draw image. Selectable via ImGui.
4. `draw_geometry(cmd)` — opaque pass over `OpaqueSurfaces`.
5. `draw_imgui(cmd, swapchainView)`.
6. Blit/copy to swapchain image and present.

**GPU-driven data.** Vertex buffers and per-instance transform buffers are addressed via `VkDeviceAddress`, passed through `GPUDrawPushConstants` (worldMatrix + vertexBuffer + instanceTransformBuffer). The compute shader writes into the same instance transform buffer the vertex shader reads from. `MAX_INSTANCE_COUNT = 1000` (`src/vk_engine.cpp:1663`) is the hard cap on instances per mesh.

**Materials.** `GLTFMetallic_Roughness` builds opaque + transparent pipelines sharing a `materialLayout` (UBO + 2 combined image samplers). Loaded GLTF surfaces get a `MaterialInstance` per surface via `write_material`. The ground uses a separate `_groundPipeline` but reuses the same descriptor set (`defaultData`) — the ground frag/vert shaders are bespoke (`shaders/ground.{vert,frag}`) and the ground vertex shader does not consume the instance transform buffer (it's single-instance).

**Scene graph.** `Node` (in [src/vk_types.h](src/vk_types.h)) is the renderable base; `MeshNode` is a leaf with a `MeshAsset`. `LoadedGLTF` ([src/vk_loader.h](src/vk_loader.h)) is the full-scene loader (textures, materials, samplers, descriptor pool); `loadGltfMeshes` is a lighter loader that returns just `MeshAsset`s. The currently rendered scene is built ad-hoc in `update_scene` against `loadedNodes["Suzanne"]` plus the ground; the `loadedScenes` map exists for `LoadedGLTF` use but is currently unused (commented out).

**Lighting.** Hemispheric ambient (`ambientColor` = sky, `groundColor` = ground) plus a single directional `sunlightDirection`/`sunlightColor` — see `GPUSceneData` and `update_scene`.

**Input.** `Camera::processSDLEvent` handles keyboard/mouse; gamepad left-stick is polled directly in the run loop (`SDL_GetGamepadAxis`) with an 8000-unit deadzone, written to `_padLeftAxis` for the camera to consume in `update()`.

## Code style

`.clang-format` enforces Microsoft-based style: Allman braces, 4-space indent, 120 col, left-aligned pointers, namespaces indented, no include reordering (`SortIncludes: false` — manual include order is intentional, especially for Vulkan + VMA + GLM + SDL ordering).

`VK_CHECK` (in `vk_types.h`) `abort()`s on Vulkan errors — this is the standard error-handling pattern, do not replace it with exceptions or silent fallbacks.
