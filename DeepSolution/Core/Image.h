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
	void AttachImageView(const VkImageSubresourceRange& range);
	void AttachCubeMapImageView(const VkImageSubresourceRange& range);
	void AttachSampler(const VkSamplerCreateInfo& samplerCI);
	void Upload(VkCommandBuffer commandBuffer, VkBuffer buffer) const;

	VkImage Get() const;
	VkImageView GetView() const;
	VkSampler GetSampler() const;

	static uint32_t calculateMaxMiplevels(int width, int height);
	/**
	 * @brief Requires image to be in transfer destination.
	*/
	static void generateMipmaps(VkImage image, int width, int height, uint32_t mipLevels, VkCommandBuffer commandBuffer);

	/**
	 * @brief Requires image to be in transfer destination
	*/
	static void generateCubemapMipmaps(VkImage image, int width, uint32_t mipLevels, VkCommandBuffer commandBuffer);

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