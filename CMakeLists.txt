cmake_minimum_required (VERSION 3.25)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project ("DeepSolution")

find_package(Vulkan REQUIRED)
find_package(VulkanMemoryAllocator CONFIG REQUIRED)
find_package(volk CONFIG REQUIRED)
find_package(vk-bootstrap CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(TINYGLTF_INCLUDE_DIRS "tiny_gltf.h")

add_subdirectory(third-party)

add_executable (DeepSolution "DeepSolution.cpp" "DeepSolution/Core/Swapchain.h" "DeepSolution/Core/Swapchain.cpp" "DeepSolution/Core/Common.h" "DeepSolution/Core/Common.cpp"   "DeepSolution/Application.h" "DeepSolution/Application.cpp" "DeepSolution/Core/Transition.h" "DeepSolution/Core/Transition.cpp" "DeepSolution/Implementation/VMA.cpp" "DeepSolution/Core/Image.h" "DeepSolution/Core/Image.cpp"   "DeepSolution/Core/Device.h" "DeepSolution/Core/Device.cpp" "DeepSolution/ImGuiAdapter.h" "DeepSolution/ImGuiAdapter.cpp" "DeepSolution/ResourceManager.h" "DeepSolution/ResourceManager.cpp" "DeepSolution/Scene/Scene.h" "DeepSolution/Scene/Scene.cpp" "DeepSolution/Implementation/TinyGLTF.cpp" "DeepSolution/Core/Buffer.h" "DeepSolution/Core/Buffer.cpp" "DeepSolution/Core/Shader.h" "DeepSolution/Core/Shader.cpp" "DeepSolution/Camera.h" "DeepSolution/Camera.cpp" "DeepSolution/Timer.h" "DeepSolution/Timer.cpp" )

target_link_libraries(${PROJECT_NAME} PRIVATE Vulkan::Headers GPUOpen::VulkanMemoryAllocator)
target_link_libraries(${PROJECT_NAME} PRIVATE volk::volk)
target_link_libraries(${PROJECT_NAME} PRIVATE spdlog::spdlog)
target_link_libraries(${PROJECT_NAME} PRIVATE glfw)
target_link_libraries(${PROJECT_NAME} PRIVATE glm::glm)
target_link_libraries(${PROJECT_NAME} PRIVATE vk-bootstrap::vk-bootstrap)
target_link_libraries(${PROJECT_NAME} PRIVATE imgui)
# target_link_libraries(${PROJECT_NAME} PRIVATE Tracy::TracyClient)
target_include_directories(${PROJECT_NAME} PRIVATE ${TINYGLTF_INCLUDE_DIRS})
target_include_directories(${PROJECT_NAME} PRIVATE ${Stb_INCLUDE_DIR})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_20)
# target_compile_definitions(${PROJECT_NAME} PUBLIC TRACY_ENABLE TRACY_VK_USE_SYMBOL_TABLE)
target_compile_definitions(${PROJECT_NAME} PUBLIC VK_NO_PROTOTYPES VMA_STATIC_VULKAN_FUNCTIONS=0 VMA_DYNAMIC_VULKAN_FUNCTIONS=0 GLM_FORCE_DEPTH_ZERO_TO_ONE)

add_custom_target(symlink COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets)

add_custom_target(shaders)

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