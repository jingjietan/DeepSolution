#include "Framebuffer.h"
#include "Common.h"

Framebuffer::Framebuffer(VkExtent2D extent): framebufferExtent_(extent)
{
}

void Framebuffer::addColorAttachment(VkImageView image)
{
	colorAttachements_.push_back(image);
}

void Framebuffer::addDepthAttachment(VkImageView image)
{
	check(depthAttachment_ == nullptr, "Overriding depth attachment!");
	depthAttachment_ = image;
}

void Framebuffer::beginRendering(VkCommandBuffer commandBuffer, const std::vector<FramebufferOption>& options)
{
	std::vector<VkRenderingAttachmentInfo> colorAttachments{};
	VkRenderingAttachmentInfo depthAttachment{};
	for (const auto attachment : colorAttachements_)
	{
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = attachment;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachments.push_back(colorAttachment);
	}

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.renderArea.extent = framebufferExtent_;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = static_cast<uint32_t>(colorAttachments.size());
	renderInfo.pColorAttachments = colorAttachments.data();

	if (depthAttachment_)
	{
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = depthAttachment_;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		renderInfo.pDepthAttachment = &depthAttachment;
	}

	uint32_t colorCursor = 0;
	for (const auto& option : options)
	{
		switch (option.type)
		{
		case FramebufferType::Color:
			colorAttachments[colorCursor].loadOp = option.loadOp;
			colorAttachments[colorCursor].storeOp = option.storeOp;
			colorAttachments[colorCursor].clearValue = option.clearValue;
			colorCursor++;
			break;
		case FramebufferType::Depth:
			depthAttachment.loadOp = option.loadOp;
			depthAttachment.storeOp = option.storeOp;
			depthAttachment.clearValue = option.clearValue;
			break;
		default:
			break;
		}
	}

	vkCmdBeginRendering(commandBuffer, &renderInfo);
}

void Framebuffer::endRendering(VkCommandBuffer commandBuffer)
{
	vkCmdEndRendering(commandBuffer);
}
