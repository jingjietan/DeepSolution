#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>

class Device;
class Buffer
{
public:
	/**
	 * @brief Buffer Constructor
	 * @param flags Use VMA_ALLOCATION_CREATE_MAPPED_BIT for persistent, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for mappable. Use VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT for large memory.
	*/
	Buffer(Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags = 0, VmaMemoryUsage memoryUsage = VMA_MEMORY_USAGE_AUTO, const std::vector<uint32_t>& indices = {});

	void upload(void const* data, size_t size);

	void copy(const Buffer& destBuffer, VkCommandBuffer commandBuffer, size_t size, size_t destOffset, size_t srcOffset) const;

	VkDeviceAddress getAddress() const;

	operator const VkBuffer &() const
	{
		return buffer_;
	}

	~Buffer();
private:
	Device& device_;
	VkBuffer buffer_{};
	VmaAllocation allocation_{};

	VkDeviceSize size_{};
	void* data_{};
	const bool persistent;
	const bool mappable;

	void* map();
	void unmap();
};