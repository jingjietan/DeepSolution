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
	PrefilterCubemap(Device& device, const std::shared_ptr<Buffer>& cubeBuffer);

	std::unique_ptr<Image> prefilter(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int dim);
	std::unique_ptr<Image> precomputerBRDF(VkCommandBuffer commandBuffer, int width, int height);

	~PrefilterCubemap();
private:
	Device& device_;
	// Prefilter
	VkDescriptorSetLayout prefilterDescLayout{};
	VkDescriptorPool prefilterPool{};
	VkDescriptorSet prefilterSet{};
	VkPipelineLayout prefilterLayout{};
	VkPipeline prefilterPipeline{};

	// BRDF
	VkPipelineLayout brdfLayout{};
	VkPipeline brdfPipeline{};

	std::shared_ptr<Buffer> cubeBuffer;
	std::unique_ptr<Buffer> uniformBuffer;

	std::vector<VkImageView> cleanupBin{}; // this is for cleanup
};