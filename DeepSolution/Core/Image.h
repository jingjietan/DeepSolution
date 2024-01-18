#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/**
 * @brief Bundle of image, image view, allocation, and sampler.
*/
class Image
{
public:
	Image() = default;
	Image(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& imageInfo);

	void AttachImageView(VkImageAspectFlags flags);
	void AttachSampler(const VkSamplerCreateInfo& samplerCI);

	VkImage Get() const;

	void Release();
private:
	VkImage image_{};
	VkImageView imageView_{};
	VmaAllocation allocation_{};
	VkSampler sampler_{};

	VkFormat format_{};

	// For cleanup
	VkDevice device_{};
	VmaAllocator allocator_{};
};