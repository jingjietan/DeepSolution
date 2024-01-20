add_library(imgui 
	imgui/imgui.cpp
	imgui/imgui_demo.cpp
	imgui/imgui_draw.cpp
	imgui/imgui_tables.cpp
	imgui/imgui_widgets.cpp
	imgui/backends/imgui_impl_vulkan.cpp
	imgui/backends/imgui_impl_glfw.cpp
)

target_include_directories(imgui PUBLIC imgui)
target_link_libraries(imgui glfw Vulkan::Headers)
target_compile_definitions(imgui PUBLIC IMGUI_IMPL_VULKAN_NO_PROTOTYPES)

include(FetchContent)
#FetchContent_Declare (
#	tracy
#	GIT_REPOSITORY https://github.com/wolfpld/tracy.git
#	GIT_TAG v0.10
#	GIT_SHALLOW TRUE
#	GIT_PROGRESS TRUE
#)
#FetchContent_MakeAvailable (tracy)