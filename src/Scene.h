#pragma once
#include <string>
#include <vector>
#include <memory>
#include <map>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <vk_mem_alloc.h>

#include "State.h"
#include "Core/Common.h"
#include "Core/Image.h"
#include "Common/Handle.h"
#include "Render/Skybox.h"
#include "Render/InfiniteGrid.h"
#include "Utility/FlattenCubemap.h"
#include "Utility/IrradianceCubemap.h"
#include "Utility/PrefilterCubemap.h"

class Device;
class Buffer;

struct MaterialCharacteristic
{
	// shader name todo:
	bool doubleSided;
	int mode;
	bool alphaMask;
	float alphaMaskCutoff;

	constexpr auto tied() const { return std::tie(doubleSided, mode, alphaMask, alphaMaskCutoff); }
	constexpr bool operator==(MaterialCharacteristic const& rhs) const { return tied() == rhs.tied(); }
};

namespace std
{
	template<> struct hash< MaterialCharacteristic>
	{
		size_t operator()(const MaterialCharacteristic& c) const
		{
			size_t result = 0;
			hash_combine(result, c.doubleSided);
			hash_combine(result, c.mode);
			hash_combine(result, c.alphaMask);
			hash_combine(result, c.alphaMaskCutoff);
			return result;
		}
	};
}

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
	int emissiveId;

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

struct IndirectDrawParam
{
	glm::mat4 model;
	int colorId;
	int normalId;
	int mroId;
	int emissiveId;
};

class Scene
{
public:
	Scene(Device& device);

	void loadGLTF(const std::string& path, VkShaderModule vertexShader, VkShaderModule fragmentShader, VkDescriptorSet imageSet, VkPipelineLayout layout);

	void loadCubeMap(const std::string& path, VkDescriptorSet ibrSet);

	struct DrawCall
	{
		uint32_t offset;
		uint32_t count;
	};

	using PipelineGroups = std::map<VkPipeline, DrawCall>;
	using DrawParams = std::vector<IndirectDrawParam>;
	using DrawCommands = std::vector<VkDrawIndexedIndirectCommand>;
	using Drawbles = std::tuple <PipelineGroups, DrawParams, DrawCommands>;
	Drawbles getDrawables() const;

	VkBuffer getVertexBuffer() const;
	VkBuffer getIndexBuffer() const;

	VkPipeline getOrCreatePipeline(const MaterialCharacteristic& character, VkShaderModule vertexShader, VkShaderModule fragmentShader, VkDescriptorSet imageSet, VkPipelineLayout layout);

	Image& getCubeMap() const;
	Image& getPrefilter() const;
	Image& getIrradiance() const;
	Image& getBRDFMap() const;
	std::shared_ptr<Buffer> getCubeBuffer() const;

	~Scene();
private:
	Device& device_;
	VmaVirtualBlock virtualVertex_{}; // used for managing big buffers.
	VmaVirtualBlock virtualIndices_{};

	std::unique_ptr<Buffer> vertexBuffer{};
	std::unique_ptr<Buffer> indexBuffer{};
	
	std::vector<std::unique_ptr<Image>> textures{};
	std::unordered_map<MaterialCharacteristic, VkPipeline> pipelines{};

	std::vector<std::unique_ptr<Node>> nodes{};

	std::shared_ptr<Buffer> cubeBuffer_;
	std::unique_ptr<Image> cubeMap_;
	std::unique_ptr<Image> irradianceMap_;
	std::unique_ptr<Image> prefilterMap_;
	std::unique_ptr<Image> brdfMap_;

	std::unique_ptr<FlattenCubemap> flattenCubemap_;
	std::unique_ptr<IrradianceCubemap> irradianceCubemap_;
	std::unique_ptr<PrefilterCubemap> prefilterCubemap_;

	// Is this necessary?
	//void recreateAccumReveal(int width, int height);
	//std::unique_ptr<Image> accum;
	//std::unique_ptr<Image> reveal;
	//int compositeWidth;
	//int compositeHeight;
	//std::vector<std::pair<std::unique_ptr<Image>, int>> recycling;
	//void cleanupRecycling();

	//VkDescriptorPool compositingPool{};
	//VkDescriptorSetLayout compositingSetLayout{};
	//VkDescriptorSet compositingSet{};
	//VkPipelineLayout compositingPipelineLayout{};
	//VkPipeline compositingPipeline{};

	static VmaVirtualAllocation performAllocation(VmaVirtualBlock block, VkDeviceSize size, VkDeviceSize& offset);

	uint32_t maxFramesInFlight;
};