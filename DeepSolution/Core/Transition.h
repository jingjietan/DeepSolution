#pragma once

#include <vulkan/vulkan.h>

namespace Transition
{
	void UndefinedToColorAttachment(VkImage image, VkCommandBuffer commandBuffer);
	void UndefinedToDepthStencilAttachment(VkImage image, VkCommandBuffer commandBuffer);
	void UndefinedToTransferDestination(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToTransferDestination(VkImage image, VkCommandBuffer commandBuffer);
	void TransferDestinationToPresentable(VkImage image, VkCommandBuffer commandBuffer);
	void TransferDestinationToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToTransferSource(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToPresentable(VkImage image, VkCommandBuffer commandBuffer);
	void ShaderReadOptimalToColorAttachment(VkImage image, VkCommandBuffer commandBuffer);
}