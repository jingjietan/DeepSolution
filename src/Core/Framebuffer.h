#pragma once

#include <vector>
#include <memory>
#include <vulkan/vulkan.h>

class Device;

enum class FramebufferType
{
	Color,
	Depth
};

struct FramebufferOption
{
	FramebufferType type;
	VkAttachmentLoadOp loadOp;
	VkAttachmentStoreOp storeOp;
	VkClearValue clearValue;
};

class Framebuffer
{
public:
	Framebuffer(VkExtent2D extent);
	void addColorAttachment(VkImageView image);
	void addDepthAttachment(VkImageView image);
	void beginRendering(VkCommandBuffer commandBuffer, const std::vector<FramebufferOption>& options);
	void endRendering(VkCommandBuffer commandBuffer);
private:
	std::vector<VkImageView> colorAttachements_{};
	VkImageView depthAttachment_{};
	VkExtent2D framebufferExtent_{};
};