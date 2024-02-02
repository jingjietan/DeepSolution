#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>

class Device;
class Buffer;

class Skybox
{
public:
	Skybox(Device& device, const std::shared_ptr<Buffer>& cubeBuffer);

	void set(VkImageView imageView, VkSampler sampler) const;
	void render(VkCommandBuffer commandBuffer, const glm::mat4& projection, const glm::mat4& view, VkImageView colorView, VkImageView depthView, VkExtent2D extent);

	~Skybox();
private:
	Device& device_;

	std::shared_ptr<Buffer> cubeBuffer;
	std::unique_ptr<Buffer> uniformBuffer;

	VkDescriptorSet skyboxSet;
	VkDescriptorSetLayout skyboxSetLayout;
	VkDescriptorPool skyboxPool;
	VkPipelineLayout skyboxLayout;
	VkPipeline skyboxPipeline;
};