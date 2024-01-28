#include "DescriptorWrite.h"
#include <stdexcept>

namespace {
	VkDescriptorType getType(BufferType type)
	{
		switch (type)
		{
		case BufferType::Uniform:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case BufferType::DynamicUniform:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		case BufferType::Storage:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		default:
			throw std::runtime_error("");
		}
	}

	VkDescriptorType getType(ImageType type)
	{
		switch (type)
		{
		case ImageType::CombinedSampler:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		default:
			throw std::runtime_error("");
		}
	}
}

void DescriptorWrite::add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, BufferType type, uint32_t count, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range)
{
	VkDescriptorBufferInfo bufferInfo{};
	bufferInfo.buffer = buffer;
	bufferInfo.offset = offset;
	bufferInfo.range = range;
	bufferInfos.push_back(bufferInfo);

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = set;
	writeSet.dstBinding = binding;
	writeSet.dstArrayElement = arrayElement;
	writeSet.descriptorType = getType(type);
	writeSet.descriptorCount = count;
	writeSet.pBufferInfo = &bufferInfos.back();
	writes.push_back(writeSet);
}

void DescriptorWrite::add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, ImageType type, uint32_t count, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout)
{
	VkDescriptorImageInfo imageInfo{};
	imageInfo.imageLayout = imageLayout;
	imageInfo.imageView = imageView;
	imageInfo.sampler = sampler;
	imageInfos.push_back(imageInfo);

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.dstSet = set;
	writeSet.dstBinding = binding;
	writeSet.dstArrayElement = arrayElement;
	writeSet.descriptorType = getType(type);
	writeSet.descriptorCount = count;
	writeSet.pImageInfo = &imageInfos.back();
	writes.push_back(writeSet);
}

void DescriptorWrite::write(VkDevice device)
{
	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
