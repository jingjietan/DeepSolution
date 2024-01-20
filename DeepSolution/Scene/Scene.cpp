#include "Scene.h"
#include "../Core/Common.h"
#include "../Core/Buffer.h"
#include "../Core/Device.h"

#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <volk.h>
#include "../Core/Shader.h"


namespace {
	template<class T>
	struct BufferHelper
	{
		const T* ptr;
		size_t count;
		size_t stride;
		BufferHelper(const tinygltf::Model& model, const tinygltf::Primitive& primitive, const std::string& attribute)
		{
			if (auto it = primitive.attributes.find(attribute); it != primitive.attributes.end())
			{
				const auto& accessor = model.accessors[it->second];
				const auto& bufferView = model.bufferViews[accessor.bufferView];
				const auto& buffer = model.buffers[bufferView.buffer];

				ptr = reinterpret_cast<const T*>(&buffer.data[accessor.byteOffset + bufferView.byteOffset]);
				count = accessor.count;
				if (auto byteStride = accessor.ByteStride(bufferView); byteStride >= 0)
				{
					stride = static_cast<size_t>(byteStride) / sizeof(T);
				}
				else
				{
					stride = static_cast<size_t>(tinygltf::GetComponentSizeInBytes(accessor.componentType));
				}
			}
			else
			{
				ptr = nullptr;
				count = 0;
				stride = 0;
			}
		}
	};

	struct IndexHelper
	{
		const void* ptr;
		size_t count;
		int componentType;
		IndexHelper(const tinygltf::Model& model, const tinygltf::Primitive& primitive)
		{
			const auto& accessor = model.accessors[primitive.indices];
			const auto& bufferView = model.bufferViews[accessor.bufferView];
			const auto& buffer = model.buffers[bufferView.buffer];

			ptr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
			count = accessor.count;
			componentType = accessor.componentType;
		}
		void deposit(std::vector<uint32_t>& indices) const
		{
			switch (componentType) {
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
				const uint32_t* p = reinterpret_cast<const uint32_t*>(ptr);
				for (size_t i = 0; i < count; i++) {
					indices.push_back(p[i]);
				}
			} break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
				const uint16_t* p = reinterpret_cast<const uint16_t*>(ptr);
				for (size_t i = 0; i < count; i++) {
					indices.push_back(p[i]);
				}
			} break;
			case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
				const uint8_t* p = reinterpret_cast<const uint8_t*>(ptr);
				for (size_t i = 0; i < count; i++) {
					indices.push_back(p[i]);
				}
			} break;
			default:
				throw std::runtime_error("Invalid component type!");
			}
		}
	};

	struct StaticVertex
	{
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec4 tangent;
		glm::vec2 uv;

		static VkVertexInputBindingDescription BindingDescription(uint32_t bindingSlot = 0)
		{
			VkVertexInputBindingDescription binding{};
			binding.binding = bindingSlot;
			binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			binding.stride = sizeof(StaticVertex);
			return binding;
		}
		static std::array<VkVertexInputAttributeDescription, 4> AttributesDescription(uint32_t bindingSlot = 0)
		{
			std::array<VkVertexInputAttributeDescription, 4> attributes{};
			attributes[0].binding = bindingSlot;
			attributes[0].location = 0;
			attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[0].offset = offsetof(StaticVertex, position);
			attributes[1].binding = bindingSlot;
			attributes[1].location = 1;
			attributes[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[1].offset = offsetof(StaticVertex, normal);
			attributes[2].binding = bindingSlot;
			attributes[2].location = 2;
			attributes[2].format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attributes[2].offset = offsetof(StaticVertex, tangent);
			attributes[3].binding = bindingSlot;
			attributes[3].location = 3;
			attributes[3].format = VK_FORMAT_R32G32_SFLOAT;
			attributes[3].offset = offsetof(StaticVertex, uv);
			return attributes;
		}
	};
}

Scene::Scene(Device& device) : device_(device)
{
	maxFramesInFlight = device.getMaxFramesInFlight();

	constexpr size_t vertexBufferSize = 1024ull * 1024ull * 1024ull; // 1gb.
	constexpr size_t indexBufferSize = 256ull * 1024ull * 1024ull; // 256mb;

	VmaVirtualBlockCreateInfo blockCI{};
	blockCI.size = vertexBufferSize;

	vmaCreateVirtualBlock(&blockCI, &virtualVertex_);
	check(virtualVertex_);

	blockCI.size = indexBufferSize;
	vmaCreateVirtualBlock(&blockCI, &virtualIndices_);
	check(virtualIndices_);

	vertexBuffer = std::make_unique<Buffer>(device_, vertexBufferSize,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
		//VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);
	indexBuffer = std::make_unique<Buffer>(device_, vertexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		// VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

	vertexShader_ = loadShader(device.device, "Shaders/PBR.vert.spv");
	fragmentShader_ = loadShader(device.device, "Shaders/PBR.frag.spv");

	// Global Descriptor Set (Per Frame, Global)
	VkDescriptorSetLayoutBinding binding0{};
	binding0.binding = 0;
	binding0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	binding0.descriptorCount = 1;
	binding0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo globalSetLayoutCI{};
	globalSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalSetLayoutCI.bindingCount = 1;
	globalSetLayoutCI.pBindings = &binding0;
	check(vkCreateDescriptorSetLayout(device.device, &globalSetLayoutCI, nullptr, &globalSetLayout));

	VkPushConstantRange pushRange{};
	pushRange.size = sizeof(PushConstant);
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.pSetLayouts = &globalSetLayout;
	pipelineLayoutCI.setLayoutCount = 1;
	pipelineLayoutCI.pPushConstantRanges = &pushRange;
	pipelineLayoutCI.pushConstantRangeCount = 1;
	check(vkCreatePipelineLayout(device_.device, &pipelineLayoutCI, nullptr, &pipelineLayout_));

	// Sets
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = maxFramesInFlight;
	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 16;
	poolCI.poolSizeCount = 1;
	poolCI.pPoolSizes = &poolSize;
	check(vkCreateDescriptorPool(device.device, &poolCI, nullptr, &globalPool_));

	std::vector<VkDescriptorSetLayout> layouts{ maxFramesInFlight, globalSetLayout };
	globalSets_.resize(maxFramesInFlight);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = globalPool_;
	allocateInfo.descriptorSetCount = maxFramesInFlight;
	allocateInfo.pSetLayouts = layouts.data();
	check(vkAllocateDescriptorSets(device.device, &allocateInfo, globalSets_.data()));

	globalUniformBuffers_.resize(maxFramesInFlight);
	for (size_t i = 0; i < maxFramesInFlight; i++)
	{
		globalUniformBuffers_[i] = std::make_unique<Buffer>(device, sizeof(GlobalUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	}
	// Write
	for (size_t i = 0; i < maxFramesInFlight; i++)
	{
		VkDescriptorBufferInfo bufferInfo{};
		bufferInfo.buffer = globalUniformBuffers_[i]->operator const VkBuffer & ();
		bufferInfo.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet writeSet{};
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		writeSet.descriptorCount = 1;
		writeSet.dstArrayElement = 0;
		writeSet.dstBinding = 0;
		writeSet.dstSet = globalSets_[i];
		writeSet.pBufferInfo = &bufferInfo;
		vkUpdateDescriptorSets(device.device, 1, &writeSet, 0, nullptr);
	}
}

void Scene::loadGLTF(const std::string& path)
{
	tinygltf::Model model;
	{
		tinygltf::TinyGLTF loader;
		std::string warn;
		std::string error;

		bool ret = loader.LoadASCIIFromFile(&model, &error, &warn, path);

		if (!warn.empty())
		{
			SPDLOG_WARN("Warning while loading file {}: {}", path, warn);
		}
		if (!error.empty())
		{
			SPDLOG_ERROR("Error while loading file {}: {}", path, error);
		}
		check(ret);
	}

	Buffer stagingBuffer(device_, 8ull * 1024ull * 1024ull,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	// Recursive load node fn
	const auto loadNode = [&](const auto& loadNodeFn, const int nodeId) -> std::unique_ptr<Node> {
		const auto& node = model.nodes[nodeId];

		auto reprNode = std::make_unique<Node>();

		if (node.matrix.size() == 16)
		{
			reprNode->matrix = glm::make_mat4(node.matrix.data());
		}
		else
		{
			auto t = node.translation.size() == 3 ? glm::vec3(glm::make_vec3(node.translation.data())) : glm::vec3(0.0f);
			auto s = node.scale.size() == 3 ? glm::vec3(glm::make_vec3(node.scale.data())) : glm::vec3(1.0f);
			auto r = node.rotation.size() == 4 ? glm::quat(glm::make_quat(node.rotation.data())) : glm::identity<glm::quat>();
			reprNode->matrix = glm::translate(glm::mat4(1.0f), t) * glm::mat4(r) * glm::scale(glm::mat4(1.0f), s);
		}

		if (node.mesh != -1)
		{
			auto gpuMesh = std::make_unique<Mesh>();

			const auto& mesh = model.meshes[node.mesh];
			for (const auto& primitive : mesh.primitives)
			{
				std::vector<StaticVertex> vertices{};
				std::vector<uint32_t> indices{};

				BufferHelper<float> position{ model, primitive, "POSITION" };
				BufferHelper<float> normal{ model, primitive, "NORMAL" };
				BufferHelper<float> tangent{ model, primitive, "TANGENT" };
				BufferHelper<float> uv{ model, primitive, "UV0" };

				for (size_t i = 0; i < position.count; i++)
				{
					StaticVertex vertex{};
					vertex.position = position.ptr ? glm::make_vec3(&position.ptr[position.stride * i]) : glm::vec3();
					vertex.normal = normal.ptr ? glm::make_vec3(&normal.ptr[normal.stride * i]) : glm::vec3();
					vertex.tangent = tangent.ptr ? glm::make_vec4(&tangent.ptr[tangent.stride * i]) : glm::vec4();
					vertex.uv = uv.ptr ? glm::make_vec2(&uv.ptr[uv.stride * i]) : glm::vec2();
					vertices.push_back(vertex);
				}

				check(primitive.indices >= 0);
				IndexHelper index{ model, primitive };
				index.deposit(indices);

				const auto verticesSize = SizeInBytes(vertices);
				const auto indicesSize = SizeInBytes(indices);

				Allocation vertexAlloc = performAllocation(virtualVertex_, verticesSize);
				Allocation indicesAlloc = performAllocation(virtualIndices_, indicesSize);

				stagingBuffer.upload(vertices.data(), verticesSize);
				CreateInfo::performOneTimeAction(device_.device, device_.transferQueue.queue, device_.transferPool, [&](VkCommandBuffer commandBuffer) {
					stagingBuffer.copy(*vertexBuffer, commandBuffer, verticesSize, vertexAlloc.offset, 0);
				});

				stagingBuffer.upload(indices.data(), indicesSize);
				CreateInfo::performOneTimeAction(device_.device, device_.transferQueue.queue, device_.transferPool, [&](VkCommandBuffer commandBuffer) {
					stagingBuffer.copy(*indexBuffer, commandBuffer, indicesSize, indicesAlloc.offset, 0);
				});

				const auto firstIndex = static_cast<uint32_t>(indicesAlloc.offset / sizeof(uint32_t));
				const auto vertexOffset = static_cast<int32_t>(vertexAlloc.offset / sizeof(StaticVertex));

				const auto indexCount = static_cast<uint32_t>(indices.size());

				// Pipeline
				VkPipeline pipeline;
				{
					std::array stages = {
						CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader_),
						CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader_)
					};

					auto vertexBinding = StaticVertex::BindingDescription();
					auto vertexAttributes = StaticVertex::AttributesDescription();
					auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

					auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

					auto viewportState = CreateInfo::ViewportState();

					auto rasterizationState = CreateInfo::RasterizationState(VK_FALSE, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);

					auto multisampleState = CreateInfo::MultisampleState();

					auto depthStencilState = CreateInfo::DepthStencilState();

					auto colorAttachment = CreateInfo::ColorBlendAttachment();
					auto colorBlendState = CreateInfo::ColorBlendState(&colorAttachment, 1);

					std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
					auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

					VkFormat swapchainFormat = device_.getSurfaceFormat();
					auto rendering = CreateInfo::Rendering(&swapchainFormat, 1, device_.getDepthFormat());

					VkGraphicsPipelineCreateInfo pipelineCreateInfo{ VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };
					pipelineCreateInfo.stageCount = static_cast<uint32_t>(stages.size());
					pipelineCreateInfo.pStages = stages.data();
					pipelineCreateInfo.pVertexInputState = &vertexInputState;
					pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
					pipelineCreateInfo.pTessellationState = nullptr;
					pipelineCreateInfo.pViewportState = &viewportState;
					pipelineCreateInfo.pRasterizationState = &rasterizationState;
					pipelineCreateInfo.pMultisampleState = &multisampleState;
					pipelineCreateInfo.pDepthStencilState = &depthStencilState;
					pipelineCreateInfo.pColorBlendState = &colorBlendState;
					pipelineCreateInfo.pDynamicState = &dynamicState;
					pipelineCreateInfo.layout = pipelineLayout_;

					Connect(pipelineCreateInfo, rendering);

					check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
				}

				gpuMesh->submeshes.push_back(Submesh{
					.vertexAlloc = vertexAlloc,
					.indexAlloc = indicesAlloc,
					.indexCount = indexCount,
					.firstIndex = firstIndex,
					.vertexOffset = vertexOffset,
					.pipeline = pipeline
				});
				// gpuMesh.submeshes.emplace_back(vertexAlloc, indicesAlloc, firstIndex, vertexOffset, indexCount, pipeline);
			}
			reprNode->mesh = std::move(gpuMesh);
			//meshes.push_back(gpuMesh);
		}
			
		for (const auto& childId : node.children)
		{
			auto childNode = loadNodeFn(loadNodeFn, childId);
			reprNode->childrens.push_back(std::move(childNode));
		}

		return reprNode;
	};

	const auto& defaultScene = model.defaultScene != -1 ? model.scenes[model.defaultScene] : model.scenes.front();
	
	for (const auto nodeId : defaultScene.nodes)
	{
		nodes.push_back(loadNode(loadNode, nodeId));
	}

}

void Scene::draw(VkCommandBuffer commandBuffer, Camera& camera)
{
	size_t offset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->operator const VkBuffer &(), &offset);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer->operator const VkBuffer & (), 0, VK_INDEX_TYPE_UINT32);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSets_[frameCount_], 0, nullptr);
	
	{
		GlobalUniform uniform{};
		uniform.projection = camera.calculateProjection();
		uniform.view = camera.calculateView();
		globalUniformBuffers_[frameCount_]->upload(&uniform, sizeof(uniform));
	}
	
	const auto render = [&](const auto& render1, const std::unique_ptr<Node>& node, glm::mat4 parent) -> void {
		glm::mat4 model = parent * node->matrix;
		if (node->mesh)
		{
			PushConstant push{};
			push.model = model;
			vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(push), &push);

			for (const auto& submesh : node->mesh->submeshes)
			{
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, submesh.pipeline);
				vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, submesh.vertexOffset, 0);
			}
		}

		for (const auto& child : node->childrens)
		{
			render1(render1, child, model);
		}
	};

	for (const auto& node : nodes)
	{
		render(render, node, glm::mat4(1.0f));
	}

	frameCount_ = (frameCount_ + 1) % maxFramesInFlight;
}

Scene::~Scene()
{
	vkDestroyShaderModule(device_.device, vertexShader_, nullptr);
	vkDestroyShaderModule(device_.device, fragmentShader_, nullptr);

	const auto deleteNode = [&](const auto& deleteNode1, const std::unique_ptr<Node>& node) -> void {
		if (node->mesh)
		{
			for (const auto& submesh : node->mesh->submeshes)
			{
				SPDLOG_INFO("Allocation at offset V: {} I: {} with something like vkCmdDrawIndexed(c, {}, 1, {}, {}, 0)", submesh.vertexAlloc.offset, submesh.indexAlloc.offset, submesh.indexCount, submesh.firstIndex, submesh.vertexOffset);
				vmaVirtualFree(virtualVertex_, submesh.vertexAlloc.allocation);
				vmaVirtualFree(virtualIndices_, submesh.indexAlloc.allocation);

				vkDestroyPipeline(device_.device, submesh.pipeline, nullptr);
			}
		}

		for (const auto& child : node->childrens)
		{
			deleteNode1(deleteNode1, child);
		}
	};

	for (const auto& node : nodes)
	{
		deleteNode(deleteNode, node);
	}

	

	globalUniformBuffers_.clear();
	vkDestroyDescriptorPool(device_.device, globalPool_, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, globalSetLayout, nullptr);
	vkDestroyPipelineLayout(device_.device, pipelineLayout_, nullptr);

	vmaDestroyVirtualBlock(virtualVertex_);
	vmaDestroyVirtualBlock(virtualIndices_);

	vertexBuffer.reset();
	indexBuffer.reset();
}

Allocation Scene::performAllocation(VmaVirtualBlock block, VkDeviceSize size)
{
	Allocation allocation{};
	VmaVirtualAllocationCreateInfo allocationCI{};
	allocationCI.size = size;
	auto res = vmaVirtualAllocate(block, &allocationCI, &allocation.allocation, &allocation.offset);
	if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		SPDLOG_WARN("Allocation failed!");
	}
	return allocation;
}
