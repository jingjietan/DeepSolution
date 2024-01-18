#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include "Core/Swapchain.h"
#include "Core/Device.h"

class Application
{
public:
	Application() = default;
	Application(int width, int height);

	void run();

	~Application();
private:
	GLFWwindow* window_;
	
	VkInstance instance_;
	VkDebugUtilsMessengerEXT messenger_;
	VkSurfaceKHR surface_;
	
	Device device_;

	Queue graphics_;
	Queue transfer_;

	Swapchain swapchain_;

	void Draw();
};