#include "ResourceManager.h"
#include "Core/Device.h"
#include "Core/Common.h"
#include "Core/Swapchain.h"

#include <array>
#include <volk.h>

void ResourceManager::Connect(Device& device)
{
	device_ = &device;

	std::array<VkDescriptorSetLayoutBinding, 1> bindings{};
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindings[0].descriptorCount = device.indexingProperties.maxDescriptorSetUpdateAfterBindSampledImages;
	bindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	constexpr VkDescriptorBindingFlags flags = 
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;

	VkDescriptorSetLayoutBindingFlagsCreateInfo setLayoutCIFlags{};
	setLayoutCIFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	setLayoutCIFlags.bindingCount = 1;
	setLayoutCIFlags.pBindingFlags = &flags;
	
	VkDescriptorSetLayoutCreateInfo setLayoutCI{};
	setLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	setLayoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	setLayoutCI.bindingCount = static_cast<uint32_t>(bindings.size());
	setLayoutCI.pBindings = bindings.data();
	::Connect(setLayoutCI, setLayoutCIFlags);

	check(vkCreateDescriptorSetLayout(device_->device, &setLayoutCI, nullptr, &layout_));

	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	poolSize.descriptorCount = 1000;
	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolCI.poolSizeCount = 1;
	poolCI.pPoolSizes = &poolSize;
	poolCI.maxSets = 2;

	check(vkCreateDescriptorPool(device_->device, &poolCI, nullptr, &pool_));

	VkDescriptorSetVariableDescriptorCountAllocateInfo variableInfo{};
	variableInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableInfo.descriptorSetCount = 1;
	/*
	std::vector<VkDescriptorSetLayout> layouts(device_, layout_);

	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = pool_;
	allocateInfo.descriptorSetCount = Swapchain::MAX_FRAMES_IN_FLIGHT;
	allocateInfo.pSetLayouts = layouts.data();
	::Connect(allocateInfo, variableInfo);

	bindlessSets_.resize(Swapchain::MAX_FRAMES_IN_FLIGHT);
	check(vkAllocateDescriptorSets(device_->device, &allocateInfo, bindlessSets_.data()));
	*/
}

void ResourceManager::Destroy()
{
	vkDestroyDescriptorSetLayout(device_->device, layout_, nullptr);
	vkDestroyDescriptorPool(device_->device, pool_, nullptr);
}
