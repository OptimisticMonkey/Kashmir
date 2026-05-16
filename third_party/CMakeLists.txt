find_package(Vulkan REQUIRED)

# SDL3 ships with the Vulkan SDK. Locate it via its installed CMake package.
if(NOT TARGET SDL3::SDL3)
  find_package(SDL3 CONFIG REQUIRED HINTS "$ENV{VULKAN_SDK}/cmake" "$ENV{VULKAN_SDK}")
endif()

add_library(vkbootstrap STATIC)
add_library(glm INTERFACE)
add_library(vma INTERFACE)

add_library(stb_image INTERFACE)

add_subdirectory(fastgltf)
add_subdirectory(fmt EXCLUDE_FROM_ALL)
# SDL2 is no longer built from source; SDL3 from the Vulkan SDK is used.
# add_subdirectory(SDL EXCLUDE_FROM_ALL)

target_sources(vkbootstrap PRIVATE 
  vkbootstrap/VkBootstrap.h
  vkbootstrap/VkBootstrap.cpp
  )

target_include_directories(vkbootstrap PUBLIC vkbootstrap)
target_link_libraries(vkbootstrap PUBLIC Vulkan::Vulkan $<$<BOOL:UNIX>:${CMAKE_DL_LIBS}>)
set_property(TARGET vkbootstrap PROPERTY CXX_STANDARD 20)

#both vma and glm and header only libs so we only need the include path
target_include_directories(vma INTERFACE vma)

target_include_directories(glm INTERFACE glm)

#add_library(sdl2 INTERFACE)

#target_include_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Include/SDL2 )
#target_link_directories(sdl2 INTERFACE $ENV{VULKAN_SDK}/Lib )

#target_link_libraries(sdl2 INTERFACE SDL2 SDL2main)

add_library(imgui STATIC)

target_include_directories(imgui PUBLIC imgui)

target_sources(imgui PRIVATE 
  imgui/imgui.h
  imgui/imgui.cpp

  imgui/imgui_demo.cpp
  imgui/imgui_draw.cpp
  imgui/imgui_widgets.cpp
  imgui/imgui_tables.cpp
  imgui/imgui_impl_vulkan.cpp
  imgui/imgui_impl_sdl3.cpp
  )

target_link_libraries(imgui PUBLIC Vulkan::Vulkan SDL3::SDL3)

target_include_directories(stb_image INTERFACE stb_image)

# meshoptimizer — built from third_party/meshoptimizer/src/*.cpp.
# Added as a git submodule (third_party/meshoptimizer). Used for meshlet
# generation on the mesh-shader rendering path.
add_library(meshoptimizer STATIC)
file(GLOB MESHOPT_SRC meshoptimizer/src/*.cpp)
target_sources(meshoptimizer PRIVATE ${MESHOPT_SRC})
target_include_directories(meshoptimizer PUBLIC meshoptimizer/src)
