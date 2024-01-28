#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>

class Device;
class Image;
class Buffer;
class FlattenCubemap
{
public:
	FlattenCubemap(Device& device, const std::shared_ptr<Buffer>& cubeBuffer);

	/**
	 * @brief Convert flat HDR image to cubemap. Width and Height must be the same. Only 32bit RGBA images are supported for now. Use graphics queue for this.
	*/
	std::unique_ptr<Image> convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int width, int height);

	~FlattenCubemap();
private:
	Device& device_;
	VkDescriptorSetLayout conversionDescLayout{};
	VkDescriptorPool conversionPool{};
	VkDescriptorSet conversionSet{};
	VkPipelineLayout conversionLayout{};
	VkPipeline conversionPipeline{};

	std::shared_ptr<Buffer> cubeBuffer;
	std::unique_ptr<Buffer> uniformBuffer;
};