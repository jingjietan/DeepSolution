#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

#include <vector>
#include <array>

class Device;

/**
 * @brief Swapchain abstraction.
*/
class Swapchain
{
public:
	/**
	 * @brief Setup swapchain and its dependencies.
	*/
	Swapchain(Device& device, VkFormat depthFormat = VK_FORMAT_UNDEFINED);

	/**
	 * @brief Acquire an image and updates the internal image index.
	 * @return A command buffer that has begin recording.
	*/
	VkCommandBuffer acquire(int width, int height);

	/**
	 * @brief Submits the frame command buffer and presents the corresponding image index
	*/
	void present();

	VkImage getCurrentImage() const;
	VkImageView getCurrentImageView() const;
	VkImageView getDepthImageView() const;
	VkExtent2D getExtent() const;
	VkFormat getFormat() const;

	~Swapchain();
private:
	VkSwapchainKHR swapchain_{};
	std::vector<VkImage> images_;
	std::vector<VkImageView> imageViews_;
	VkFormat imageFormat;
	VkColorSpaceKHR colorSpace;
	VkPresentModeKHR presentMode;

	// If requesting for depth image, set this to desired depth format
	VkFormat depthFormat = VK_FORMAT_UNDEFINED;
	VkImage depthImage_{};
	VkImageView depthImageView_{};
	VmaAllocation depthAllocation_{};

	uint32_t imageIndex_ = 0;

	void Refresh();

	VkFormat surfaceFormat_{};
	VkExtent2D surfaceExtent_{};

	struct FrameResource {
		VkCommandBuffer commandBuffer;
		VkSemaphore acquire;
		VkSemaphore present;
		VkFence fence;
	};
	uint32_t framesInFlight_; // Ask from device.
	std::vector<FrameResource> frameResources_;
	uint32_t currentFrame_ = 0;
	VkCommandPool commandPool_{};
	VkQueue graphics_{};

	// Stored for easier recreation of swapchain.
	Device& device_;
};