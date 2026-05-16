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

# --- Slang sources: shaders/<basename>.<stage>.slang -> shaders/<basename>.<stage>.spv
# `common.slang` is imported by other shaders and is not compiled standalone.
file(GLOB_RECURSE SLANG_SOURCE_FILES "${PROJECT_SOURCE_DIR}/shaders/*.slang")
foreach(SLANG ${SLANG_SOURCE_FILES})
    get_filename_component(FILE_NAME ${SLANG} NAME)
    if(FILE_NAME STREQUAL "common.slang")
        continue()
    endif()
    string(REGEX MATCH "\\.(vert|frag|comp|mesh|task)\\.slang$" STAGE_MATCH ${FILE_NAME})
    if(NOT STAGE_MATCH)
        message(FATAL_ERROR "Slang file ${FILE_NAME} must end in .vert.slang, .frag.slang, .comp.slang, .mesh.slang, or .task.slang")
    endif()
    set(STAGE_FLAG ${CMAKE_MATCH_1})
    if(STAGE_FLAG STREQUAL "vert")
        set(SLANG_STAGE "vertex")
    elseif(STAGE_FLAG STREQUAL "frag")
        set(SLANG_STAGE "fragment")
    elseif(STAGE_FLAG STREQUAL "comp")
        set(SLANG_STAGE "compute")
    elseif(STAGE_FLAG STREQUAL "mesh")
        set(SLANG_STAGE "mesh")
    elseif(STAGE_FLAG STREQUAL "task")
        # slangc names the task stage "amplification" (DXR/DX12 terminology).
        set(SLANG_STAGE "amplification")
    endif()
    # Mesh / task stages need SPIR-V 1.4 minimum (SPV_EXT_mesh_shader); bump to 1.6 for safety.
    if(STAGE_FLAG STREQUAL "mesh" OR STAGE_FLAG STREQUAL "task")
        set(SLANG_PROFILE "spirv_1_6")
    else()
        set(SLANG_PROFILE "spirv_1_5")
    endif()
    string(REGEX REPLACE "\\.slang$" ".spv" SPV_NAME ${FILE_NAME})
    set(SPIRV "${PROJECT_SOURCE_DIR}/shaders/${SPV_NAME}")
    message(STATUS "Slang: ${FILE_NAME} -> ${SPV_NAME} (stage=${SLANG_STAGE}, profile=${SLANG_PROFILE})")
    add_custom_command(
        OUTPUT ${SPIRV}
        COMMAND ${SLANGC} ${SLANG}
            -profile ${SLANG_PROFILE}
            -target spirv
            -emit-spirv-directly
            -fvk-use-entrypoint-name
            -entry main
            -stage ${SLANG_STAGE}
            -I ${PROJECT_SOURCE_DIR}/shaders
            -o ${SPIRV}
        DEPENDS ${SLANG} ${PROJECT_SOURCE_DIR}/shaders/common.slang)
    list(APPEND SPIRV_BINARY_FILES ${SPIRV})
endforeach()

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
