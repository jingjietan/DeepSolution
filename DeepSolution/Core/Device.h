#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/**
 * @brief Class to reference back for query state, allocation and deallocation.
*/
class Device
{
public:
	struct Queue
	{
		VkQueue queue;
		uint32_t family;
	};

	Device() = default;

	void init(void* window);
	void deinit();
	
	VkInstance instance{};
	VkDebugUtilsMessengerEXT messenger{};
	VkSurfaceKHR surface{};
	VkDevice device{};
	VkPhysicalDevice physicalDevice{};
	VmaAllocator allocator{};

	VkCommandPool graphicsPool{};
	Queue graphicsQueue{};

	Queue transferQueue{};

	VkPhysicalDeviceDescriptorIndexingProperties indexingProperties{};

	uint32_t getMaxFramesInFlight() const;
	VkFormat getDepthFormat() const;

private:
	// Use this to cache search results.
	mutable VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
};