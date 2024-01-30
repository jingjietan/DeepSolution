#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>

class Device;
class Image;
class Buffer;
class IrradianceCubemap
{
public:
	IrradianceCubemap(Device& device, const std::shared_ptr<Buffer>& buffer);

	std::unique_ptr<Image> convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int dim);

	~IrradianceCubemap();
private:
	Device& device_;
	VkDescriptorSetLayout irradianceDescLayout{};
	VkDescriptorPool irradiancePool{};
	VkDescriptorSet irradianceSet{};
	VkPipelineLayout irradianceLayout{};
	VkPipeline irradiancePipeline{};

	std::shared_ptr<Buffer> cubeBuffer;
	std::unique_ptr<Buffer> uniformBuffer;
};