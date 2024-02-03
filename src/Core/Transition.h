#pragma once

#include <vulkan/vulkan.h>

namespace Transition
{
	void UndefinedToColorAttachment(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
	void UndefinedToDepthStencilAttachment(VkImage image, VkCommandBuffer commandBuffer);
	void UndefinedToTransferDestination(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
	void ColorAttachmentToTransferDestination(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
	void TransferDestinationToPresentable(VkImage image, VkCommandBuffer commandBuffer);
	void TransferDestinationToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
	void ColorAttachmentToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
	void ColorAttachmentToTransferSource(VkImage image, VkCommandBuffer commandBuffer);
	void ColorAttachmentToPresentable(VkImage image, VkCommandBuffer commandBuffer);
	void ShaderReadOptimalToColorAttachment(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range);
}