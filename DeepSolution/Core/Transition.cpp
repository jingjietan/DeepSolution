#include "Transition.h"

#include <volk.h>

void Transition::UndefinedToColorAttachment(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dependency{};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &dependency);
}

void Transition::UndefinedToDepthStencilAttachment(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT_KHR;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dependency{};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &dependency);
}

void Transition::UndefinedToTransferDestination(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;	
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = range;

	VkDependencyInfo dependency{};
	dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependency.imageMemoryBarrierCount = 1;
	dependency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &dependency);
}

void Transition::ColorAttachmentToTransferDestination(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::TransferDestinationToPresentable(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_NONE;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::TransferDestinationToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer, const VkImageSubresourceRange& range)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // at fragment
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange = range;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::ColorAttachmentToShaderReadOptimal(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::ColorAttachmentToTransferSource(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::ColorAttachmentToPresentable(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_NONE;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}

void Transition::ShaderReadOptimalToColorAttachment(VkImage image, VkCommandBuffer commandBuffer)
{
	VkImageMemoryBarrier2 imageBarrier{};
	imageBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	imageBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	imageBarrier.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageBarrier.srcQueueFamilyIndex = 0;
	imageBarrier.dstQueueFamilyIndex = 0;
	imageBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	imageBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageBarrier.image = image;
	imageBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageBarrier.subresourceRange.baseMipLevel = 0;
	imageBarrier.subresourceRange.baseArrayLayer = 0;
	imageBarrier.subresourceRange.levelCount = 1;
	imageBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo depedency{};
	depedency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depedency.imageMemoryBarrierCount = 1;
	depedency.pImageMemoryBarriers = &imageBarrier;

	vkCmdPipelineBarrier2KHR(commandBuffer, &depedency);
}
