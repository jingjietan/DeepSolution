#pragma once
#include <string>
#include <vk_mem_alloc.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "../Core/Image.h" // todo: cannot forward declare image?
#include "../Render/InfiniteGrid.h"
#include "../State.h"
#include "../Common/Handle.h"
#include "../Core/Common.h"
#include "../Utility/FlattenCubemap.h"
#include "../Render/Skybox.h"

class Device;
class Buffer;
class Image;

struct Submesh
{
	VmaVirtualAllocation vertexAlloc;
	VmaVirtualAllocation indexAlloc;
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;

	VkPipeline pipeline;
	int colorId;
	int normalId;
	int mroId;

	bool transparent;
};

struct Mesh
{
	std::vector<Submesh> submeshes;
};

struct Node
{
	std::vector<std::unique_ptr<Node>> childrens;
	glm::vec3 scale;
	glm::quat rotation;
	glm::vec3 translation;
	glm::mat4 matrix;
	std::string name;
	glm::mat4 getMatrix() const;
	std::unique_ptr<Mesh> mesh;
};

struct GlobalUniform
{
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 viewPos;
};

struct PushConstant
{
	glm::mat4 model;
	int colorId;
	int normalId;
	int mroId;
};

struct IndirectDrawParam
{
	glm::mat4 model;
	int colorId;
	int normalId;
	int mroId;
};

class Scene
{
public:
	Scene(Device& device);

	void loadGLTF(const std::string& path);

	void loadCubeMap(const std::string& path);

	// Handle loadTexture(const std::string& path);

	void draw(VkCommandBuffer commandBuffer, const State& state, VkImageView colorView, VkImageView depthView);

	~Scene();
private:
	Device& device_;
	VmaVirtualBlock virtualVertex_{}; // used for managing big buffers.
	VmaVirtualBlock virtualIndices_{};

	std::unique_ptr<Buffer> vertexBuffer{};
	std::unique_ptr<Buffer> indexBuffer{};
	std::unique_ptr<Buffer> indirectBuffer{};
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

	std::vector<std::unique_ptr<Buffer>> perMeshDrawDataBuffer;
	std::vector<std::unique_ptr<Buffer>> lightBuffer;
	struct alignas(16) LightUpload // base alignment
	{
		int32_t count;
	};

	VkDescriptorPool globalPool_{};
	VkDescriptorSetLayout globalSetLayout{};

	VkDescriptorPool bindlessPool{};
	VkDescriptorSetLayout bindlessSetLayout{};

	std::unique_ptr<Image> cubeMap_{};
	
	VkPipelineLayout pipelineLayout_{};

	std::vector<std::unique_ptr<Buffer>> globalUniformBuffers_;
	std::vector<VkDescriptorSet> globalSets_;
	VkDescriptorSet bindlessSet_{};

	static VmaVirtualAllocation performAllocation(VmaVirtualBlock block, VkDeviceSize size, VkDeviceSize& offset);

	uint32_t frameCount_{};
	uint32_t maxFramesInFlight;
	//
	std::shared_ptr<Buffer> cubeBuffer_;
	std::unique_ptr<InfiniteGrid> infiniteGrid_;
	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<FlattenCubemap> flattenCubemap_;
};