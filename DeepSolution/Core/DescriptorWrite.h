#pragma once

#include <volk.h>
#include <vector>
#include <list>

enum class BufferType
{
	Uniform,
	DynamicUniform,
	Storage
};

enum class ImageType
{
	CombinedSampler
};

class DescriptorWrite
{
public:
	void add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, BufferType type, uint32_t count, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, ImageType type, uint32_t count, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
	void write(VkDevice device);
private:
	std::list<VkDescriptorBufferInfo> bufferInfos; // to prevent invalidation :)
	std::list<VkDescriptorImageInfo> imageInfos;
	std::vector<VkWriteDescriptorSet> writes;
};