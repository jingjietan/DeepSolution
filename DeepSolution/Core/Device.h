#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/**
 * @brief Class to reference back for query state, allocation and deallocation.
*/
class Device
{
public:
	Device() = default;
	
	Device(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator allocator);

	VkDevice device{};
	VkPhysicalDevice physicalDevice{};
	VmaAllocator allocator{};



	VkFormat getDepthFormat() const;

private:
	// Use this to cache search results.
	mutable VkFormat depthFormat_ = VK_FORMAT_UNDEFINED;
};