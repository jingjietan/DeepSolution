#pragma once

#include <imgui.h>
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>

class Device;
class Queue;
class ImGuiAdapter
{
public:
	ImGuiAdapter(GLFWwindow* window, Device& device);

	void BeginFrame();

	void EndFrame();

	void Draw(VkImageView imageView, VkExtent2D extent, VkCommandBuffer commandBuffer);

	~ImGuiAdapter();
private:
	Device& device_;
	
	VkDescriptorPool descriptorPool_{};
};