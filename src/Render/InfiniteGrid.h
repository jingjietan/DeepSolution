#pragma once

#include <vulkan/vulkan.h>

class Device;

/**
 * @brief Credits to https://asliceofrendering.com/scene%20helper/2020/01/05/InfiniteGrid/.
*/
class InfiniteGrid
{
public:
	/**
	 * @brief Creates resources to draw an infinite grid. Reuses the global uniform layout.
	*/
	InfiniteGrid(Device& device, VkDescriptorSetLayout globalUniformLayout);

	/**
	 * @brief Bind the global uniform set before calling this.
	*/
	void draw(VkCommandBuffer commandBuffer, VkDescriptorSet globalSet, VkImageView color, VkImageView depth, VkExtent2D extent);

	~InfiniteGrid();
private:
	Device& device_;
	VkPipeline pipeline_;
	VkPipelineLayout pipelineLayout_;
};