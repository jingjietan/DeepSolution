#include "Common.h"

#include "Device.h"
#include <spdlog/spdlog.h>

void check(bool condition, const std::string& message, const std::source_location& location)
{
	if (!condition)
	{
		SPDLOG_ERROR("Error at {} {}. {}", location.file_name(), location.line(), message);
		std::terminate();
	}
}

#include <volk.h>

VkFence CreateInfo::createFence(VkDevice device, VkFenceCreateFlags flags)
{
	VkFence fence;
	VkFenceCreateInfo fenceCI{};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = flags;
	check(vkCreateFence(device, &fenceCI, nullptr, &fence));
	return fence;
}

VkSemaphore CreateInfo::createBinarySemaphore(VkDevice device)
{
	VkSemaphore semaphore;
	VkSemaphoreCreateInfo semaphoreCI{};
	semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	check(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
	return semaphore;
}

VkCommandBuffer CreateInfo::allocateCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	VkCommandBuffer commandBuffer;
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandBufferCount = 1;
	allocateInfo.commandPool = commandPool;
	check(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));
	return commandBuffer;
}

VkRenderPass CreateInfo::createRenderPass(VkDevice device, const VkAttachmentDescription2* pAttachments, uint32_t attachmentCount, const VkSubpassDescription2* pSubpass, uint32_t subpassCount, const VkSubpassDependency2* pDependency, uint32_t dependencyCount)
{
	VkRenderPass renderPass;
	VkRenderPassCreateInfo2 renderPassCI{};
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	renderPassCI.pAttachments = pAttachments;
	renderPassCI.attachmentCount = attachmentCount;
	renderPassCI.pSubpasses = pSubpass;
	renderPassCI.subpassCount = subpassCount;
	renderPassCI.pDependencies = pDependency;
	renderPassCI.dependencyCount = dependencyCount;
	check(vkCreateRenderPass2(device, &renderPassCI, nullptr, &renderPass));
	return renderPass;
}

void Detail::setName(Device* device, VkObjectType type, void* ptr, const std::string& name)
{
#ifndef NDEBUG
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(ptr);
	nameInfo.pObjectName = name.c_str();
	check(vkSetDebugUtilsObjectNameEXT(device->device, &nameInfo));
#endif // !NDEBUG
}
