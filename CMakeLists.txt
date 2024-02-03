cmake_minimum_required (VERSION 3.25)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project ("DeepSolution")

# Project

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")
find_path(BSHOSHANY_THREAD_POOL_INCLUDE_DIRS "BS_thread_pool.hpp")
  
find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
 
find_package(tsl-robin-map CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_subdirectory(third-party)

add_executable (DeepSolution 
    "src/DeepSolution.cpp" 
    "src/Core/Swapchain.h"
    "src/Core/Swapchain.cpp" 
    "src/Core/Common.h" 
    "src/Core/Common.cpp"   
    "src/Application.h" 
    "src/Application.cpp" 
    "src/Core/Transition.h" 
    "src/Core/Transition.cpp" 
    "src/Implementation/VMA.cpp" 
    "src/Core/Image.h" 
    "src/Core/Image.cpp"   
    "src/Core/Device.h"
    "src/Core/Device.cpp"
    "src/ImGuiAdapter.h" 
    "src/ImGuiAdapter.cpp" 
    "src/Scene.h" 
    "src/Scene.cpp" 
    "src/Implementation/TinyGLTF.cpp" 
    "src/Core/Buffer.h" 
    "src/Core/Buffer.cpp"
    "src/Core/Shader.h"
    "src/Core/Shader.cpp" 
    "src/Camera/Camera.h" 
    "src/Camera/Camera.cpp"
    "src/Timer.h" 
    "src/Timer.cpp" 
    "src/Render/InfiniteGrid.h"
    "src/Render/InfiniteGrid.cpp"
    "src/State.h" 
    "src/Camera/ArcballCamera.h"
    "src/Camera/FreeCamera.h" 
    "src/Camera/FreeCamera.cpp"
    "src/Camera/ArcballCamera.cpp"
    "src/Common/Handle.h" 
    "src/Utility/FlattenCubemap.h"
    "src/Utility/FlattenCubemap.cpp"
    "src/Core/DescriptorWrite.h"
    "src/Core/DescriptorWrite.cpp"
    "src/Render/Skybox.h" 
    "src/Render/Skybox.cpp"
    "src/Core/Cube.h"
    "src/Utility/IrradianceCubemap.h"
    "src/Utility/IrradianceCubemap.cpp" 
    "src/Utility/PrefilterCubemap.h"
    "src/Utility/PrefilterCubemap.cpp"
    "src/Common/Bench.h"
    "src/Renderer.h"
    "src/Renderer.cpp"
    "src/Light.h"
 "src/Core/Framebuffer.h" "src/Core/Framebuffer.cpp" "src/Render/Bloom.h" "src/Render/Bloom.cpp")

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-cross-core spirv-cross-cpp spirv-cross-glsl spirv-cross-reflect)
target_include_directories(${PROJECT_NAME} PRIVATE ${BSHOSHANY_THREAD_POOL_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} PRIVATE tsl::robin_map)

# target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_ENABLE TRACY_VK_USE_SYMBOL_TABLE)

target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=0 GLM_FORCE_DEPTH_ZERO_TO_ONE)

# Docs
find_package(Doxygen OPTIONAL_COMPONENTS dot)
if (DOXYGEN_FOUND)
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE "${CMAKE_SOURCE_DIR}/README.md")
    doxygen_add_docs(doc
        ${CMAKE_SOURCE_DIR}/src
    )
    add_dependencies(${PROJECT_NAME} doc)
else()
    message("Doxygen not installed. Skipping make doc.")
endif()

# Test
if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(test)
endif()

# Assets + Shaders
add_custom_target(symlink COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)
add_custom_target(shaders)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/shaders/)
file(GLOB SHADER_FILES shaders/*.vert shaders/*.frag)
foreach(FILE ${SHADER_FILES})
  get_filename_component(FILENAME ${FILE} NAME)
  add_custom_command(TARGET shaders
                     COMMAND ${Vulkan_GLSLC_EXECUTABLE} ${FILE} -o ${CMAKE_BINARY_DIR}/Shaders/${FILENAME}.spv
                     MAIN_DEPENDENCY ${FILE}
                     COMMENT "HLSL ${FILE}"
                     WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                     VERBATIM)
endforeach(FILE)

add_dependencies(${PROJECT_NAME} shaders)
add_dependencies(${PROJECT_NAME} symlink)