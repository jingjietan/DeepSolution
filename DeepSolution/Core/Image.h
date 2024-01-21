#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

class Device;
/**
 * @brief Bundle of image, image view, allocation, and sampler.
*/
class Image
{
public:
	Image(Device& device, const VkImageCreateInfo& imageInfo);

	void AttachImageView(VkImageAspectFlags flags);
	void AttachSampler(const VkSamplerCreateInfo& samplerCI);
	void Upload(VkCommandBuffer commandBuffer, VkBuffer buffer) const;

	VkImage Get() const;
	VkImageView GetView() const;
	VkSampler GetSampler() const;

	~Image();
private:
	VkImage image_{};
	VkImageView imageView_{};
	VmaAllocation allocation_{};
	VkSampler sampler_{};

	VkFormat format_{};
	int width, height;

	// For cleanup
	Device& device_;
};