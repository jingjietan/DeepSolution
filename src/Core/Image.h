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

	void attachImageView(VkImageAspectFlags flags);
	void attachImageView(const VkImageSubresourceRange& range);
	void attachCubeMapImageView(const VkImageSubresourceRange& range);
	void attachSampler(const VkSamplerCreateInfo& samplerCI);
	void upload(VkCommandBuffer commandBuffer, VkBuffer buffer) const;
	void upload(VkCommandBuffer commandBuffer, VkBuffer buffer, const VkImageSubresourceLayers& target) const;

	VkImage get() const;
	VkImageView getView() const;
	VkSampler getSampler() const;
	VkFormat getFormat() const;
	VkExtent2D getExtent() const;
	uint32_t getMipLevels() const;
	uint32_t getArrayLevels() const;
	VkImageSubresourceRange getFullRange(VkImageAspectFlags flag = VK_IMAGE_ASPECT_COLOR_BIT) const;

	static uint32_t calculateMaxMiplevels(int width, int height);
	/**
	 * @brief Requires image to be in transfer destination.
	*/
	static void generateMipmaps(VkImage image, int width, int height, uint32_t mipLevels, VkCommandBuffer commandBuffer);

	/**
	 * @brief Requires image to be in transfer destination
	*/
	static void generateCubemapMipmaps(VkImage image, int width, uint32_t mipLevels, VkCommandBuffer commandBuffer);

	void generateMaxMipmaps(VkCommandBuffer commandBuffer);
	void generateMaxCubeMipmaps(VkCommandBuffer commandBuffer);

	// Transition function
	void UndefinedToColorAttachment(VkCommandBuffer commandBuffer) const;
	//void UndefinedToDepthStencilAttachment(VkCommandBuffer commandBuffer);
	void UndefinedToTransferDestination(VkCommandBuffer commandBuffer) const;
	void ColorAttachmentToTransferDestination(VkCommandBuffer commandBuffer) const;
	//void TransferDestinationToPresentable(VkCommandBuffer commandBuffer);
	//void TransferDestinationToShaderReadOptimal(VkCommandBuffer commandBuffer);
	void ColorAttachmentToShaderReadOptimal(VkCommandBuffer commandBuffer) const;
	//void ColorAttachmentToTransferSource(VkCommandBuffer commandBuffer);
	//void ColorAttachmentToPresentable(VkCommandBuffer commandBuffer);
	void ShaderReadOptimalToColorAttachment(VkCommandBuffer commandBuffer) const;

	~Image();
private:
	VkImage image_{};
	VkImageView imageView_{};
	VmaAllocation allocation_{};
	VkSampler sampler_{};

	VkFormat format_{};
	uint32_t width, height;
	uint32_t mipLevels;
	uint32_t arrayLevels;

	// For cleanup
	Device& device_;
};