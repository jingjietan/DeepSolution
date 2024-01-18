#include "CommandPool.h"

#include <volk.h>
#include "Common.h"

CommandPool::CommandPool(VkDevice device, Queue queue, VkCommandPoolCreateFlags flags): device_(device), queue_(queue.queue)
{
	VkCommandPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCI.flags = flags;
	poolCI.queueFamilyIndex = queue.family;
	check(vkCreateCommandPool(device_, &poolCI, nullptr, &pool_));
}

VkCommandBuffer CommandPool::allocate() const
{
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = pool_;
	allocateInfo.commandBufferCount = 1;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	VkCommandBuffer commandBuffer;
	check(vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer), "Failed to allocate command buffer!");

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);
	return commandBuffer;
}

void CommandPool::submit(VkCommandBuffer commandBuffer) const
{
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	check(vkQueueSubmit(queue_, 1, &submitInfo, VK_NULL_HANDLE));
	vkQueueWaitIdle(queue_);

	vkFreeCommandBuffers(device_, pool_, 1, &commandBuffer);
}

void CommandPool::perform(const std::function<void(VkCommandBuffer)>& f) const
{
	VkCommandBuffer cb = allocate();
	f(cb);
	submit(cb);
}

void CommandPool::Destroy()
{
	vkDestroyCommandPool(device_, pool_, nullptr);
}
