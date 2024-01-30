#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>

class Device;
class Image;
class Buffer;
class PrefilterCubemap
{
public:
	PrefilterCubemap(Device& device, const std::shared_ptr<Buffer>& buffer);

	std::unique_ptr<Image> convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int dim);

	~PrefilterCubemap();
private:
	Device& device_;
	VkDescriptorSetLayout prefilterDescLayout{};
	VkDescriptorPool prefilterPool{};
	VkDescriptorSet prefilterSet{};
	VkPipelineLayout prefilterLayout{};
	VkPipeline prefilterPipeline{};

	std::shared_ptr<Buffer> cubeBuffer;
	std::unique_ptr<Buffer> uniformBuffer;

	std::vector<VkImageView> cleanupBin{}; // this is for cleanup
};