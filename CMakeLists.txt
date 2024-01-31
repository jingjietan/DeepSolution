cmake_minimum_required (VERSION 3.25)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project ("DeepSolution")

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

find_package(spirv_cross_core CONFIG REQUIRED)
find_package(spirv_cross_glsl CONFIG REQUIRED)
find_package(spirv_cross_cpp CONFIG REQUIRED)
find_package(spirv_cross_reflect CONFIG REQUIRED)
 
find_package(tsl-robin-map CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

add_subdirectory(third-party)

add_executable (DeepSolution "DeepSolution.cpp" "DeepSolution/Core/Swapchain.h" "DeepSolution/Core/Swapchain.cpp" "DeepSolution/Core/Common.h" "DeepSolution/Core/Common.cpp"   "DeepSolution/Application.h" "DeepSolution/Application.cpp" "DeepSolution/Core/Transition.h" "DeepSolution/Core/Transition.cpp" "DeepSolution/Implementation/VMA.cpp" "DeepSolution/Core/Image.h" "DeepSolution/Core/Image.cpp"   "DeepSolution/Core/Device.h" "DeepSolution/Core/Device.cpp" "DeepSolution/ImGuiAdapter.h" "DeepSolution/ImGuiAdapter.cpp"   "DeepSolution/Scene/Scene.h" "DeepSolution/Scene/Scene.cpp" "DeepSolution/Implementation/TinyGLTF.cpp" "DeepSolution/Core/Buffer.h" "DeepSolution/Core/Buffer.cpp" "DeepSolution/Core/Shader.h" "DeepSolution/Core/Shader.cpp" "DeepSolution/Camera/Camera.h" "DeepSolution/Camera/Camera.cpp" "DeepSolution/Timer.h" "DeepSolution/Timer.cpp"  "DeepSolution/Render/InfiniteGrid.h" "DeepSolution/Render/InfiniteGrid.cpp" "DeepSolution/Light.h" "DeepSolution/State.h" "DeepSolution/Camera/ArcballCamera.h" "DeepSolution/Camera/FreeCamera.h" "DeepSolution/Camera/FreeCamera.cpp" "DeepSolution/Camera/ArcballCamera.cpp" "DeepSolution/Common/Handle.h"  "DeepSolution/Utility/FlattenCubemap.h" "DeepSolution/Utility/FlattenCubemap.cpp" "DeepSolution/Core/DescriptorWrite.h" "DeepSolution/Core/DescriptorWrite.cpp" "DeepSolution/Render/Skybox.h" "DeepSolution/Render/Skybox.cpp" "DeepSolution/Core/Cube.h" "DeepSolution/Utility/IrradianceCubemap.h" "DeepSolution/Utility/IrradianceCubemap.cpp" "DeepSolution/Utility/PrefilterCubemap.h" "DeepSolution/Utility/PrefilterCubemap.cpp" )

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
target_link_libraries(${PROJECT_NAME} PRIVATE spirv-cross-core spirv-cross-cpp spirv-cross-glsl spirv-cross-reflect)

target_link_libraries(${PROJECT_NAME} PRIVATE tsl::robin_map)

# target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_ENABLE TRACY_VK_USE_SYMBOL_TABLE)

if (CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    add_subdirectory(test)
endif()

# For GLTF Compatibility
target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=0 GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_custom_target(symlink COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)

add_custom_target(shaders)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/Shaders/)
file(GLOB SHADER_FILES Shaders/*.vert Shaders/*.frag)
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