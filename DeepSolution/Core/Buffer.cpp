#include "Buffer.h"
#include "../Core/Device.h"

#include <volk.h>
#include "Common.h"

#include <stdexcept>

Buffer::Buffer(Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags flags, VmaMemoryUsage memory, const std::vector<uint32_t>& indices):
	device_(device), size_(size), persistent(flags & VMA_ALLOCATION_CREATE_MAPPED_BIT), 
	mappable(flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT || flags & VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT)
{
	VkBufferCreateInfo bufferCI{};
	bufferCI.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCI.usage = usage;
	bufferCI.size = size;
	if (indices.size() > 1)
	{
		bufferCI.sharingMode = VK_SHARING_MODE_CONCURRENT;
		bufferCI.queueFamilyIndexCount = static_cast<uint32_t>(indices.size());
		bufferCI.pQueueFamilyIndices = indices.data();
	}
	VmaAllocationCreateInfo allocationCI{};
	allocationCI.usage = memory;
	allocationCI.flags = flags;

	VmaAllocationInfo allocationInfo{};
	check(vmaCreateBuffer(device_.allocator, &bufferCI, &allocationCI, &buffer_, &allocation_, &allocationInfo));
	data_ = allocationInfo.pMappedData;
}

void Buffer::upload(void const* data, size_t size)
{
	check(size <= size_, "Upload called with size larger than capacity");
	void* mapped = map();
	memcpy(mapped, data, size);
	unmap();
}

void Buffer::copy(const Buffer& destBuffer, VkCommandBuffer commandBuffer, size_t size, size_t destOffset, size_t srcOffset) const
{
	check(destBuffer.size_ >= size + destOffset, "Destination buffer has insufficient space!");
	VkBufferCopy2 bufferRegion{};
	bufferRegion.sType = VK_STRUCTURE_TYPE_BUFFER_COPY_2;
	bufferRegion.size = size;
	bufferRegion.dstOffset = destOffset;
	bufferRegion.srcOffset = srcOffset;

	VkCopyBufferInfo2 copyBufferInfo{};
	copyBufferInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_INFO_2;
	copyBufferInfo.dstBuffer = destBuffer.buffer_;
	copyBufferInfo.srcBuffer = buffer_;
	copyBufferInfo.pRegions = &bufferRegion;
	copyBufferInfo.regionCount = 1;
	vkCmdCopyBuffer2(commandBuffer, &copyBufferInfo);
}

Buffer::~Buffer()
{
	vmaDestroyBuffer(device_.allocator, buffer_, allocation_);
}

void* Buffer::map()
{
	check(mappable, "Called map on not mappable buffer!");
	if (!data_)
	{
		vmaMapMemory(device_.allocator, allocation_, &data_);
	}
	return data_;
}

void Buffer::unmap()
{
	check(mappable, "Called unmap on non-mappable buffer!");
	if (!persistent)
	{
		vmaUnmapMemory(device_.allocator, allocation_);
	}
}
