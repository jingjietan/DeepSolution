#include "DescriptorWrite.h"
#include <stdexcept>
#include "Common.h"
#include <cassert>
#include <fmt/format.h>

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

void DescriptorCreator::add(const std::string& name, uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages, VkDescriptorBindingFlags flags)
{
	VkDescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = binding;
	layoutBinding.descriptorType = type;
	layoutBinding.descriptorCount = count;
	layoutBinding.stageFlags = stages;
	pipelineBindings[name].first.push_back(layoutBinding);
	pipelineBindings[name].second.push_back(flags);
}

VkDescriptorSetLayout DescriptorCreator::createLayout(const std::string& name, VkDevice device, VkDescriptorSetLayoutCreateFlags flags)
{
	const auto& bindings = pipelineBindings[name].first;
	const auto& bindingFlags = pipelineBindings[name].second;
	VkDescriptorSetLayout layout;

	VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutCIFlags{};
	setLayoutCIFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	setLayoutCIFlags.bindingCount = static_cast<uint32_t>(bindingFlags.size());
	setLayoutCIFlags.pBindingFlags = bindingFlags.data();

	VkDescriptorSetLayoutCreateInfo setLayoutCI{};
	setLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCI.pNext = &setLayoutCIFlags;
	setLayoutCI.flags = flags;
	setLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutCI.pBindings = bindings.data();
	check(vkCreateDescriptorSetLayout(device, &setLayoutCI, nullptr, &layout));
	createdLayouts[name] = layout;
	setName(device, layout, fmt::format("{} Layout", name));
	return layout;
}

VkDescriptorPool DescriptorCreator::createPool(const std::string& name, VkDevice device, uint32_t multiplesOf, VkDescriptorPoolCreateFlags flags)
{
	const auto& bindings = pipelineBindings[name].first;

	VkDescriptorPool descPool;
	std::map<VkDescriptorType, size_t> poolTypeToIndex;
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (const auto& binding : bindings)
	{
		if (auto it = poolTypeToIndex.find(binding.descriptorType); it != poolTypeToIndex.end())
		{
			poolSizes[it->second].descriptorCount += binding.descriptorCount * multiplesOf;
		}
		else
		{
			VkDescriptorPoolSize poolSize{};
			poolSize.type = binding.descriptorType;
			poolSize.descriptorCount = binding.descriptorCount * multiplesOf;
			poolSizes.push_back(poolSize);
			poolTypeToIndex[binding.descriptorType] = poolSizes.size() - 1;
		}
	}

	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.flags = flags;
	poolCI.maxSets = multiplesOf;
	poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCI.pPoolSizes = poolSizes.data();
	check(vkCreateDescriptorPool(device, &poolCI, nullptr, &descPool));
	createdPools[name] = descPool;
	setName(device, descPool, fmt::format("{} Pool", name));
	return descPool;
}

VkPipelineLayout DescriptorCreator::createPipelineLayout(VkDevice device, const VkDescriptorSetLayout* pLayouts, uint32_t layoutCount, VkPushConstantRange* range)
{
	VkPipelineLayout layout;
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.pSetLayouts = pLayouts;
	pipelineLayoutCI.setLayoutCount = layoutCount;
	if (range)
	{
		pipelineLayoutCI.pPushConstantRanges = range;
		pipelineLayoutCI.pushConstantRangeCount = 1;
	}
	check(vkCreatePipelineLayout(device, &pipelineLayoutCI, nullptr, &layout));
	setName(device, layout, "Pipeline Layout");
	return layout;
}

void DescriptorCreator::allocateSets(const std::string& name, VkDevice device, uint32_t count, VkDescriptorSet* pSets)
{
	const auto& pool = createdPools[name];
	const auto layout = createdLayouts[name];

	std::vector<VkDescriptorSetLayout> layouts{ count, layout };
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = pool;
	allocateInfo.descriptorSetCount = count;
	allocateInfo.pSetLayouts = layouts.data();
	check(vkAllocateDescriptorSets(device, &allocateInfo, pSets));
	for (size_t i = 0; i < count; i++)
	{
		setName(device, pSets[i], fmt::format("{} Set {}", name, i));
	}
}

void DescriptorCreator::allocateVariableSets(const std::string& name, VkDevice device, uint32_t count, VkDescriptorSet* pSets, uint32_t variableCount)
{
	const auto pool = createdPools[name];
	const auto layout = createdLayouts[name];
	const bool isVariable = pipelineBindings[name].second.back() & VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT;
	assert(isVariable && "Called allocate variable on non-variable set.");

	std::vector<uint32_t> variableCounts(count, variableCount);
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo{};
	variableInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableInfo.descriptorSetCount = count;
	variableInfo.pDescriptorCounts = variableCounts.data();

	std::vector<VkDescriptorSetLayout> layouts{ count, layout };
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.pNext = &variableInfo;
	allocateInfo.descriptorPool = pool;
	allocateInfo.descriptorSetCount = count;
	allocateInfo.pSetLayouts = layouts.data();
	check(vkAllocateDescriptorSets(device, &allocateInfo, pSets));
	for (size_t i = 0; i < count; i++)
	{
		setName(device, pSets[i], fmt::format("{} Variable Set {}", name, i));
	}
}
