#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <array>

#include "Queue.h"
#include "CommandPool.h"

class Device;

/**
 * @brief Swapchain abstraction, used together with dynamic rendering extension.
*/
class Swapchain
{
public:
	/**
	 * @brief Uninitialised state
	*/
	Swapchain() = default;
	
	/**
	 * @brief Setup swapchain and its dependencies.
	*/
	void Connect(Device& device, VkSurfaceKHR surface, Queue graphicsQueue);

	void SetDepthFormat(VkFormat format);
	
	/**
	 * @brief Acquire an image and updates the internal image index.
	 * @return A command buffer that has begin recording.
	*/
	VkCommandBuffer Acquire(VkClearValue color, VkClearValue depthStencil);

	/**
	 * @brief Submits the frame command buffer and presents the corresponding image index
	*/
	void Present();

	VkImage GetCurrentImage() const;
	VkImageView GetCurrentImageView() const;

	void Release();
private:
	VkSwapchainKHR swapchain_{};
	std::vector<VkImage> images_;
	std::vector<VkImageView> imageViews_;

	// If requesting for depth image, set this to desired depth format
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	VkImage depthImage_{};
	VkImageView depthImageView_{};
	VmaAllocation depthAllocation_{};

	// For non swapchain layout transitions.
	CommandPool pool{};

	uint32_t imageIndex_ = 0;

	void Refresh();

	VkFormat surfaceFormat_{};
	VkExtent2D surfaceExtent_{};

	const static int MAX_FRAMES_IN_FLIGHT = 2;
	struct FrameResource {
		VkCommandBuffer commandBuffer;
		VkSemaphore acquire;
		VkSemaphore present;
		VkFence fence;
	};
	std::vector<FrameResource> frameResources_;
	uint32_t currentFrame_ = 0;
	VkCommandPool commandPool_{};
	VkQueue graphics_{};

	// Stored for easier recreation of swapchain.
	Device* device_{};
	VkSurfaceKHR surface_{};
};