#pragma once

#include <vulkan/vulkan_core.h>
#include "../Core/Common.h"
#include <memory>
#include <utility>

class Device;
class Image;
class Bloom
{
public:
	Bloom(Device& device);

	void doBloom(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage image, VkImageView colorView, VkImageView swapchainView);

	~Bloom();
private:
	const uint32_t maxDownsamples = 5;

	Device& device_;
	struct BloomDownsamplePipeline {
		VkDescriptorSetLayout descLayout;
		VkDescriptorPool pool;
		VkPipelineLayout layout;
		std::vector<VkDescriptorSet> sets;
		VkPipeline pipeline;
	} bloomDownsamplePipeline_;

	struct BloomUpsamplePipeline {
		VkDescriptorSetLayout descLayout;
		VkDescriptorPool pool;
		VkPipelineLayout layout;
		std::vector<VkDescriptorSet> sets;
		VkPipeline pipeline;
	} bloomUpsamplePipeline_;

	struct BloomCompositePipeline {
		VkDescriptorSetLayout descLayout;
		VkDescriptorPool pool;
		VkPipelineLayout layout;
		VkDescriptorSet set;
		VkPipeline pipeline;
	} bloomCompositePipeline_;

	std::unique_ptr<Image> bloomImage_{};
	VkSampler bloomSampler_{};

	// Index i represents image view of miplevel i.
	std::vector<VkImageView> bloomImageViews_;

	std::vector<std::pair<VkImageView, int>> bloomImageViewsRecycle_;

	void cleanup();
};