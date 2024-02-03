#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include "Scene.h"

class Device;
class Image;
class Buffer;
class InfiniteGrid;
class Skybox;
class FlattenCubemap;
class IrradianceCubemap;
class PrefilterCubemap;

class Renderer
{
public:
	Renderer(Device& device, Scene& scene);

	void draw(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const Scene::Drawbles& renderItems, const State& state);

	// TODO: remove this
	VkShaderModule getVertexModule() const;
	VkShaderModule getFragmentModule() const;
	VkDescriptorSet getImageSet() const;
	VkDescriptorSet getIBRSet() const;
	VkPipelineLayout getPipelineLayout() const;

	void cleanupFrameDependentItems();

	~Renderer();
private:
	Device& device_;
	Scene& scene_;

	VkShaderModule vertexShader_{}; // For testing
	VkShaderModule fragmentShader_{};

	VkDescriptorSetLayout ibrSetLayout{};
	VkDescriptorPool ibrPool{};
	VkDescriptorSet ibrSet{};

	std::vector<std::unique_ptr<Buffer>> perMeshDrawDataBuffer;
	std::vector<std::unique_ptr<Buffer>> lightBuffer;
	std::unique_ptr<Buffer> indirectBuffer{};

	VkDescriptorPool globalPool_{};
	VkDescriptorSetLayout globalSetLayout{};

	VkDescriptorPool bindlessPool{};
	VkDescriptorSetLayout bindlessSetLayout{};

	VkPipelineLayout pipelineLayout_{};

	std::unique_ptr<Image> hdrImage_;
	VkDescriptorSet hdrSet;

	PipelineInfo<1> hdrPipeline_;
	std::vector <std::pair<std::unique_ptr<Image>, int>> hdrBin_;

	std::vector<std::unique_ptr<Buffer>> globalUniformBuffers_;
	std::vector<VkDescriptorSet> globalSets_;
	VkDescriptorSet bindlessSet_{};

	std::unique_ptr<InfiniteGrid> infiniteGrid_;
	std::unique_ptr<Skybox> skybox_;

	uint32_t frameCount_ = 0;
	const uint32_t maxFramesInFlight;
};