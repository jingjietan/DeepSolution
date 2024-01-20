#pragma once
#include <string>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include "../Camera.h"

class Device;
class Buffer;

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
};
/*
struct Transform
{
	std::unique_ptr<Transform> parent;
	std::vector<Transform> children;
	glm::mat4 matrix;
};
*/
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
};

class Scene
{
public:
	Scene(Device& device);

	void loadGLTF(const std::string& path);

	void draw(VkCommandBuffer commandBuffer, Camera& camera);

	~Scene();
private:
	Device& device_;
	VmaVirtualBlock virtualVertex_{}; // used for managing big buffers.
	VmaVirtualBlock virtualIndices_{};

	std::unique_ptr<Buffer> vertexBuffer{};
	std::unique_ptr<Buffer> indexBuffer{};

	std::vector<std::unique_ptr<Node>> nodes{};

	VkShaderModule vertexShader_{}; // For testing
	VkShaderModule fragmentShader_{};

	VkDescriptorPool globalPool_{};
	VkDescriptorSetLayout globalSetLayout{};
	VkPipelineLayout pipelineLayout_{};

	std::vector<std::unique_ptr<Buffer>> globalUniformBuffers_;
	std::vector<VkDescriptorSet> globalSets_;

	static Allocation performAllocation(VmaVirtualBlock block, VkDeviceSize size);

	uint32_t frameCount_{};
	uint32_t maxFramesInFlight;
};