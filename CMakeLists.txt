# CMakeList.txt : CMake project for vulkan_guide, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)
set(CMAKE__EXPORT_COMPILE_COMMANDS ON)
project (Kashmir VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Vulkan REQUIRED)

# SDL3 ships with the Vulkan SDK.
find_package(SDL3 CONFIG REQUIRED HINTS "$ENV{VULKAN_SDK}/cmake" "$ENV{VULKAN_SDK}")

add_subdirectory(third_party)

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/bin")

add_subdirectory(src)

find_program(GLSL_VALIDATOR glslangValidator HINTS /usr/bin /usr/local/bin $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)
find_program(SLANGC slangc HINTS $ENV{VULKAN_SDK}/Bin/ $ENV{VULKAN_SDK}/Bin32/)

set(SPIRV_BINARY_FILES "")

# --- Slang sources ---------------------------------------------------------
# Each pipeline lives in ONE .slang file holding all of its stages as named
# entry points (vertexMain / fragmentMain / meshMain / taskMain / main for
# standalone compute). We compile one SPIR-V module per (entry point, stage) and
# deliberately OMIT -fvk-use-entrypoint-name so slangc emits the SPIR-V
# OpEntryPoint as "main" — the engine loads every module with pName "main".
# `common.slang` is imported by other shaders and is never compiled standalone.
#
#   compile_slang(<src.slang> <entry> <stage> <profile> <output.spv>)
#
# Stages with mesh-shader features (mesh / amplification) need SPIR-V 1.6;
# everything else compiles at 1.5. Because each invocation only emits the call
# graph of the requested entry point, vertex/fragment stages in a file that also
# contains mesh/task entries still compile cleanly at 1.5.
function(compile_slang SRC ENTRY STAGE PROFILE OUT_NAME)
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${OUT_NAME}")
    message(STATUS "Slang: ${SRC} [${ENTRY}/${STAGE}] -> ${OUT_NAME} (profile=${PROFILE})")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${SLANGC} ${PROJECT_SOURCE_DIR}/shaders/${SRC}
            -profile ${PROFILE}
            -target spirv
            -emit-spirv-directly
            -entry ${ENTRY}
            -stage ${STAGE}
            -I ${PROJECT_SOURCE_DIR}/shaders
            -o ${SPIRV}
        DEPENDS ${PROJECT_SOURCE_DIR}/shaders/${SRC}
                ${PROJECT_SOURCE_DIR}/shaders/common.slang)
    set(SPIRV_BINARY_FILES ${SPIRV_BINARY_FILES} ${SPIRV} PARENT_SCOPE)
endfunction()

# Combined graphics shaders (vertex/fragment at 1_5; mesh/task at 1_6).
compile_slang(mesh.slang   vertexMain   vertex        spirv_1_5 mesh.vert.spv)
compile_slang(mesh.slang   fragmentMain fragment      spirv_1_5 mesh.frag.spv)
compile_slang(mesh.slang   meshMain     mesh          spirv_1_6 mesh.mesh.spv)
compile_slang(mesh.slang   taskMain     amplification spirv_1_6 mesh.task.spv)
compile_slang(shadow.slang vertexMain   vertex        spirv_1_5 shadow.vert.spv)
compile_slang(shadow.slang fragmentMain fragment      spirv_1_5 shadow.frag.spv)
compile_slang(shadow.slang meshMain     mesh          spirv_1_6 shadow.mesh.spv)
compile_slang(ground.slang vertexMain   vertex        spirv_1_5 ground.vert.spv)
compile_slang(ground.slang fragmentMain fragment      spirv_1_5 ground.frag.spv)
compile_slang(grid.slang   vertexMain   vertex        spirv_1_5 grid.vert.spv)
compile_slang(grid.slang   fragmentMain fragment      spirv_1_5 grid.frag.spv)
compile_slang(colored_triangle.slang      vertexMain   vertex   spirv_1_5 colored_triangle.vert.spv)
compile_slang(colored_triangle.slang      fragmentMain fragment spirv_1_5 colored_triangle.frag.spv)
compile_slang(colored_triangle_mesh.slang vertexMain   vertex   spirv_1_5 colored_triangle_mesh.vert.spv)
compile_slang(colored_triangle_mesh.slang fragmentMain fragment spirv_1_5 tex_image.frag.spv)

# Standalone compute shaders (single 'main' entry, unchanged sources).
compile_slang(update_transform.comp.slang     main compute spirv_1_5 update_transform.comp.spv)
compile_slang(gradient_color.comp.slang        main compute spirv_1_5 gradient_color.comp.spv)
compile_slang(sky.comp.slang                   main compute spirv_1_5 sky.comp.spv)
compile_slang(build_tlas_instances.comp.slang  main compute spirv_1_5 build_tlas_instances.comp.spv)

# --- Remaining GLSL sources (background-compute experiments still in GLSL).
file(GLOB_RECURSE GLSL_SOURCE_FILES
    "${PROJECT_SOURCE_DIR}/shaders/*.frag"
    "${PROJECT_SOURCE_DIR}/shaders/*.vert"
    "${PROJECT_SOURCE_DIR}/shaders/*.comp"
    )

foreach(GLSL ${GLSL_SOURCE_FILES})
  get_filename_component(FILE_NAME ${GLSL} NAME)
  set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${FILE_NAME}.spv")
  message(STATUS "GLSL: ${FILE_NAME}")
  add_custom_command(
    OUTPUT ${SPIRV}
    COMMAND ${GLSL_VALIDATOR} -V ${GLSL} -o ${SPIRV}
    DEPENDS ${GLSL})
  list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach(GLSL)

add_custom_target(
    Shaders
    DEPENDS ${SPIRV_BINARY_FILES}
    )
