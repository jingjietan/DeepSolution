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
	FlattenCubemap(Device& device);

	std::unique_ptr<Image> convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int width, int height);

	~FlattenCubemap();
private:
	Device& device_;
	VkDescriptorSetLayout conversionDescLayout{};
	VkDescriptorPool conversionPool{};
	VkDescriptorSet conversionSet{};
	VkPipelineLayout conversionLayout{};
	VkPipeline conversionPipeline{};

	std::unique_ptr<Buffer> quadBuffer;
	std::unique_ptr<Buffer> uniformBuffer;
};