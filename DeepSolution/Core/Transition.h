#pragma once

#include <vulkan/vulkan.h>

namespace Transition
{
	void UndefinedToColorAttachment(VkImage image, VkCommandBuffer commandBuffer);
	void UndefinedToDepthStencilAttachment(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToPresentable(VkImage image, VkCommandBuffer commandBuffer);
}