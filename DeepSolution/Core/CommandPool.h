#pragma once

#include <functional>

#include <vulkan/vulkan.h>
#include "Queue.h"

class CommandPool
{
public:
	CommandPool() = default;
	CommandPool(VkDevice device, Queue queue, VkCommandPoolCreateFlags flags = 0);

	VkCommandBuffer allocate() const;
	void submit(VkCommandBuffer commandBuffer) const;
	void perform(const std::function<void(VkCommandBuffer)>& f) const;

	void Destroy();
private:
	VkDevice device_{};
	VkQueue queue_{};

	VkCommandPool pool_{};
};