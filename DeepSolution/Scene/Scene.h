#pragma once
#include <string>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../Camera.h"
#include "../Core/Image.h" // todo: cannot forward declare image?

class Device;
class Buffer;
class Image;

struct Allocation
{
	VmaVirtualAllocation allocation;
	VkDeviceSize offset;
};

struct Submesh
{
	Allocation vertexAlloc;
	Allocation indexAlloc;
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;

	VkPipeline pipeline;
	int colorId;

	bool transparent;
};

struct Mesh
{
	std::vector<Submesh> submeshes;
};

struct Node
{
	std::vector<std::unique_ptr<Node>> childrens;
	glm::mat4 matrix;
	std::unique_ptr<Mesh> mesh;
};

struct GlobalUniform
{
	glm::mat4 view;
	glm::mat4 projection;
};

struct PushConstant
{
	glm::mat4 model;
	int colorId;
};

class Scene
{
public:
	Scene(Device& device);

	void loadGLTF(const std::string& path);

	void draw(VkCommandBuffer commandBuffer, Camera& camera, VkImageView colorView, VkImageView depthView);

	~Scene();
private:
	Device& device_;
	VmaVirtualBlock virtualVertex_{}; // used for managing big buffers.
	VmaVirtualBlock virtualIndices_{};

	std::unique_ptr<Buffer> vertexBuffer{};
	std::unique_ptr<Buffer> indexBuffer{};
	std::vector<std::unique_ptr<Image>> textures{};

	std::vector<std::unique_ptr<Node>> nodes{};

	// Is this necessary?
	void recreateAccumReveal(int width, int height);
	std::unique_ptr<Image> accum;
	std::unique_ptr<Image> reveal;
	int compositeWidth;
	int compositeHeight;
	std::vector<std::pair<std::unique_ptr<Image>, int>> recycling;
	void cleanupRecycling();

	VkShaderModule vertexShader_{}; // For testing
	VkShaderModule fragmentShader_{};
	VkShaderModule transparentVertex_{};
	VkShaderModule transparentFragment_{};

	std::unique_ptr<Buffer> quadVerticesBuffer;
	std::unique_ptr<Buffer> quadIndicesBuffer;
	VkDescriptorPool compositingPool{};
	VkDescriptorSetLayout compositingSetLayout{};
	VkDescriptorSet compositingSet{};
	VkPipelineLayout compositingPipelineLayout{};
	VkPipeline compositingPipeline{};

	std::unique_ptr<Image> defaultTextures[4];
	void initialiseDefaultTextures();

	VkDescriptorPool globalPool_{};
	VkDescriptorSetLayout globalSetLayout{};

	VkDescriptorPool bindlessPool{};
	VkDescriptorSetLayout bindlessSetLayout{};

	VkPipelineLayout pipelineLayout_{};

	std::vector<std::unique_ptr<Buffer>> globalUniformBuffers_;
	std::vector<VkDescriptorSet> globalSets_;
	VkDescriptorSet bindlessSet_{};

	static Allocation performAllocation(VmaVirtualBlock block, VkDeviceSize size);

	uint32_t frameCount_{};
	uint32_t maxFramesInFlight;
};