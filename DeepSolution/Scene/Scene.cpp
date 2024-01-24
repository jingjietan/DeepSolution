#include "Scene.h"
#include "../Core/Common.h"
#include "../Core/Buffer.h"
#include "../Core/Device.h"
#include "../Core/Shader.h"
#include "../Core/Image.h"
#include "../Core/Transition.h"

#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <volk.h>


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

	struct QuadVertex {
		glm::vec3 position;
		glm::vec2 uv; 
		
		static VkVertexInputBindingDescription BindingDescription(uint32_t bindingSlot = 0)
		{
			VkVertexInputBindingDescription binding{};
			binding.binding = bindingSlot;
			binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			binding.stride = sizeof(QuadVertex);
			return binding;
		}

		static std::array<VkVertexInputAttributeDescription, 2> AttributesDescription(uint32_t bindingSlot = 0)
		{
			std::array<VkVertexInputAttributeDescription, 2> attributes{};
			attributes[0].binding = bindingSlot;
			attributes[0].location = 0;
			attributes[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			attributes[0].offset = offsetof(QuadVertex, position);
			attributes[1].binding = bindingSlot;
			attributes[1].location = 1;
			attributes[1].format = VK_FORMAT_R32G32_SFLOAT;
			attributes[1].offset = offsetof(QuadVertex, uv);
			return attributes;
		}
	};

	std::vector<QuadVertex> quadVertices = {
		QuadVertex {
			glm::vec3{1.0f, 1.0f, 1.0f}, glm::vec2{1.0f, 1.0f}
		},
		QuadVertex {
			glm::vec3{1.0f, -1.0f, 1.0f}, glm::vec2{1.0f, 0.0f}
		},
		QuadVertex {
			glm::vec3{-1.0f, -1.0f, 1.0f}, glm::vec2{0.0f, 0.0f}
		},
		QuadVertex {
			glm::vec3{-1.0f, 1.0f, 1.0f}, glm::vec2{0.f, 1.0f}
		}
	};
	std::vector<uint16_t> quadIndices = { 0, 1, 2, 0, 3, 2 };

	enum FormatUsageHint {
		NO_HINT = 0u, // Use unorm
		UNORM, // For data encoded in images
		SRGB // For textures
	};

	VkFormat getVkFormat(const int noOfComponents, const int bitsPerChannel, const FormatUsageHint hint = NO_HINT) {
		VkFormat format;
		if (noOfComponents == 3 && bitsPerChannel == 8) {
			format = hint != SRGB ? VK_FORMAT_R8G8B8_UNORM : VK_FORMAT_R8G8B8_SRGB;
		}
		else if (noOfComponents == 4 && bitsPerChannel == 8) {
			format = hint != SRGB ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB;
		}
		else {
			throw std::runtime_error("No format found for this combination!");
		}
		return format;
	}

	VkFilter getVkFilter(const int filterIndex, const VkFilter defaultFilter) {
		switch (filterIndex) {
		case TINYGLTF_TEXTURE_FILTER_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			return VK_FILTER_NEAREST;
		case TINYGLTF_TEXTURE_FILTER_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
			return VK_FILTER_LINEAR;
		default:
			return defaultFilter;
		}
	}

	VkSamplerMipmapMode getVkMipmapMode(const int filterIndex, const VkSamplerMipmapMode defaultMipmapMode) {
		switch (filterIndex) {
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR:
			return VK_SAMPLER_MIPMAP_MODE_LINEAR;
		case TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST:
		case TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST:
			return VK_SAMPLER_MIPMAP_MODE_NEAREST;
		default:
			return defaultMipmapMode;
		}
	}

	VkSamplerAddressMode getVkAddressMode(const int wrapParameter, const VkSamplerAddressMode defaultAddressMode) {
		switch (wrapParameter) {
		case TINYGLTF_TEXTURE_WRAP_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_REPEAT;
		case TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE:
			return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		case TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT:
			return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
		default:
			return defaultAddressMode;
		}
	}

	VkPrimitiveTopology getMode(const int mode)
	{
		switch (mode)
		{
		case TINYGLTF_MODE_LINE:
			return VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
		case TINYGLTF_MODE_LINE_STRIP:
			return VK_PRIMITIVE_TOPOLOGY_LINE_STRIP;
		case TINYGLTF_MODE_POINTS:
			return VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
		case TINYGLTF_MODE_TRIANGLES:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		case TINYGLTF_MODE_TRIANGLE_FAN:
			return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN;
		//case TINYGLTF_MODE_TRIANGLE_STRIP:
		//	return VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY;
		default:
			throw std::runtime_error("Not implemented!");
		}
	}
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

	transparentVertex_ = loadShader(device.device, "Shaders/TransparentPBR.vert.spv");
	transparentFragment_ = loadShader(device.device, "Shaders/TransparentPBR.frag.spv");

	quadVerticesBuffer = std::make_unique<Buffer>(device, SizeInBytes(quadVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	quadIndicesBuffer = std::make_unique<Buffer>(device, SizeInBytes(quadIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	quadVerticesBuffer->upload(quadVertices.data(), SizeInBytes(quadVertices));
	quadIndicesBuffer->upload(quadIndices.data(), SizeInBytes(quadIndices));

	// Compositing Pipeline
	compositingSetLayout = [](VkDevice device) {
		VkDescriptorSetLayout layout;
		std::array<VkDescriptorSetLayoutBinding, 2> compositingBindings{};
		compositingBindings[0].binding = 0;
		compositingBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		compositingBindings[0].descriptorCount = 1;
		compositingBindings[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		compositingBindings[1].binding = 1;
		compositingBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		compositingBindings[1].descriptorCount = 1;
		compositingBindings[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		VkDescriptorSetLayoutCreateInfo compositingSetCI{};
		compositingSetCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		compositingSetCI.bindingCount = static_cast<uint32_t>(compositingBindings.size());
		compositingSetCI.pBindings = compositingBindings.data();
		check(vkCreateDescriptorSetLayout(device, &compositingSetCI, nullptr, &layout));
		return layout;
	}(device.device);
	
	VkPipelineLayoutCreateInfo compositingLayoutCI{};
	compositingLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	compositingLayoutCI.setLayoutCount = 1;
	compositingLayoutCI.pSetLayouts = &compositingSetLayout;
	check(vkCreatePipelineLayout(device.device, &compositingLayoutCI, nullptr, &compositingPipelineLayout));

	auto compositingVertex = loadShader(device.device, "Shaders/Compositing.vert.spv");
	auto compositingFrag = loadShader(device.device, "Shaders/Compositing.frag.spv");

	compositingPipeline = [&]() {
		VkPipeline pipeline;
		std::array stages = {
			CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, compositingVertex),
			CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, compositingFrag)
		};

		auto vertexBinding = QuadVertex::BindingDescription();
		auto vertexAttributes = QuadVertex::AttributesDescription();
		auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

		auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			false,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

		auto multisampleState = CreateInfo::MultisampleState();

		auto depthStencilState = CreateInfo::DepthStencilState();
		depthStencilState.depthWriteEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState attachment{};
		attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		attachment.blendEnable = VK_TRUE;
		attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		attachment.dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		attachment.colorBlendOp = VK_BLEND_OP_ADD;
		attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);
		// colorBlendState.logicOpEnable = VK_TRUE;

		std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

		VkFormat swapchainFormat = device_.getSurfaceFormat();
		auto rendering = CreateInfo::Rendering(&swapchainFormat, 1, VK_FORMAT_UNDEFINED);

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
		pipelineCreateInfo.layout = compositingPipelineLayout;

		Connect(pipelineCreateInfo, rendering);

		check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
		return pipeline;
	}();

	vkDestroyShaderModule(device.device, compositingVertex, nullptr);
	vkDestroyShaderModule(device.device, compositingFrag, nullptr);

	compositingPool = [](VkDevice device) {
		VkDescriptorPool pool;
		VkDescriptorPoolSize poolSize{};
		poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSize.descriptorCount = 2;

		VkDescriptorPoolCreateInfo poolCI{};
		poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolCI.maxSets = 1;
		poolCI.poolSizeCount = 1;
		poolCI.pPoolSizes = &poolSize;
		check(vkCreateDescriptorPool(device, &poolCI, nullptr, &pool));
		return pool;
	}(device.device);

	compositingSet = [](VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout) {
		VkDescriptorSet set;
		VkDescriptorSetAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocateInfo.descriptorPool = pool;
		allocateInfo.descriptorSetCount = 1;
		allocateInfo.pSetLayouts = &layout;
		check(vkAllocateDescriptorSets(device, &allocateInfo, &set));
		return set;
	}(device.device, compositingPool, compositingSetLayout);

	// Global Descriptor Set (Per Frame, Global)
	VkDescriptorSetLayoutBinding globalSetBinding0{};
	globalSetBinding0.binding = 0;
	globalSetBinding0.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	globalSetBinding0.descriptorCount = 1;
	globalSetBinding0.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

	VkDescriptorSetLayoutCreateInfo globalSetLayoutCI{};
	globalSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	globalSetLayoutCI.bindingCount = 1;
	globalSetLayoutCI.pBindings = &globalSetBinding0;
	check(vkCreateDescriptorSetLayout(device.device, &globalSetLayoutCI, nullptr, &globalSetLayout));

	// Bindless Set (Global)
	VkDescriptorSetLayoutBinding bindlessSetBinding0{};
	bindlessSetBinding0.binding = 0;
	bindlessSetBinding0.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindlessSetBinding0.descriptorCount = 4;
	bindlessSetBinding0.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorSetLayoutBinding bindlessSetBinding1{};
	bindlessSetBinding1.binding = 1;
	bindlessSetBinding1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindlessSetBinding1.descriptorCount = 10000;
	bindlessSetBinding1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

	VkDescriptorBindingFlags bindlessFlags = 
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT;

	std::array bindingFlags{ (VkDescriptorBindingFlags)VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT, bindlessFlags, };

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindlessSetLayoutCIFlags{};
	bindlessSetLayoutCIFlags.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindlessSetLayoutCIFlags.bindingCount = bindingFlags.size();
	bindlessSetLayoutCIFlags.pBindingFlags = bindingFlags.data();

	std::array bindings{ bindlessSetBinding0, bindlessSetBinding1 };

	VkDescriptorSetLayoutCreateInfo bindlessSetLayoutCI{};
	bindlessSetLayoutCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	bindlessSetLayoutCI.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	bindlessSetLayoutCI.bindingCount = bindings.size();
	bindlessSetLayoutCI.pBindings = bindings.data();
	Connect(bindlessSetLayoutCI, bindlessSetLayoutCIFlags);

	check(vkCreateDescriptorSetLayout(device.device, &bindlessSetLayoutCI, nullptr, &bindlessSetLayout));
	setName(device_, bindlessSetLayout, "Bindless Set Layout");

	// Model Push Constant
	VkPushConstantRange pushRange{};
	pushRange.size = sizeof(PushConstant);
	pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

	std::array pipelineSetLayouts{ globalSetLayout, bindlessSetLayout };

	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.pSetLayouts = pipelineSetLayouts.data();
	pipelineLayoutCI.setLayoutCount = static_cast<uint32_t>(pipelineSetLayouts.size());
	pipelineLayoutCI.pPushConstantRanges = &pushRange;
	pipelineLayoutCI.pushConstantRangeCount = 1;
	check(vkCreatePipelineLayout(device_.device, &pipelineLayoutCI, nullptr, &pipelineLayout_));

	// Pool
	VkDescriptorPoolSize poolSize{};
	poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	poolSize.descriptorCount = maxFramesInFlight;
	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = 16;
	poolCI.poolSizeCount = 1;
	poolCI.pPoolSizes = &poolSize;
	check(vkCreateDescriptorPool(device.device, &poolCI, nullptr, &globalPool_));
	setName(device_, globalPool_, "Global Pool");

	VkDescriptorPoolSize bindlessImagePoolSize{};
	bindlessImagePoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bindlessImagePoolSize.descriptorCount = 512;
	
	VkDescriptorPoolCreateInfo bindlessPoolCI{};
	bindlessPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	bindlessPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	bindlessPoolCI.maxSets = 1;
	bindlessPoolCI.poolSizeCount = 1;
	bindlessPoolCI.pPoolSizes = &bindlessImagePoolSize;
	check(vkCreateDescriptorPool(device.device, &bindlessPoolCI, nullptr, &bindlessPool));
	setName(device_, bindlessPool, "Bindless Pool");

	// Sets
	std::vector<VkDescriptorSetLayout> globalLayouts{ maxFramesInFlight, globalSetLayout };
	globalSets_.resize(maxFramesInFlight);
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = globalPool_;
	allocateInfo.descriptorSetCount = maxFramesInFlight;
	allocateInfo.pSetLayouts = globalLayouts.data();
	check(vkAllocateDescriptorSets(device.device, &allocateInfo, globalSets_.data()));
	for (size_t i = 0; i < globalSets_.size(); i++)
	{
		setName(device_, globalSets_[i], fmt::format("Global Set {}", i));
	}

	uint32_t descriptorCount = 2048;
	VkDescriptorSetVariableDescriptorCountAllocateInfo varaibleInfo{};
	varaibleInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	varaibleInfo.descriptorSetCount = 1;
	varaibleInfo.pDescriptorCounts = &descriptorCount;

	VkDescriptorSetAllocateInfo bindlessAllocateInfo{};
	bindlessAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	bindlessAllocateInfo.descriptorPool = bindlessPool;
	bindlessAllocateInfo.descriptorSetCount = 1;
	bindlessAllocateInfo.pSetLayouts = &bindlessSetLayout;
	Connect(bindlessAllocateInfo, varaibleInfo);
	check(vkAllocateDescriptorSets(device.device, &bindlessAllocateInfo, &bindlessSet_));
	setName(device_, bindlessSet_, "Bindless Set");

	// Set is done! Time to write.
	initialiseDefaultTextures();

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

		
		bool ret = //loader.LoadBinaryFromFile(&model, &error, &warn, path);
			loader.LoadASCIIFromFile(&model, &error, &warn, path);

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

	Buffer stagingBuffer(device_, 16ull * 1024ull * 1024ull,
		VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
		VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	// Gather hints
	std::unordered_map<int, FormatUsageHint> hints{};
	auto insertHint = [&](int valid, FormatUsageHint hint) {
		if (valid == -1)
		{
			return;
		}
		hints[valid] = hint;
	};
	auto getHint = [&](int id) {
		if (auto it = hints.find(id); it != hints.end())
		{
			return it->second;
		}
		return FormatUsageHint::NO_HINT;
	};
	for (const auto& material : model.materials)
	{
		insertHint(material.pbrMetallicRoughness.baseColorTexture.index, FormatUsageHint::SRGB);
		insertHint(material.normalTexture.index, FormatUsageHint::UNORM);
		insertHint(material.pbrMetallicRoughness.metallicRoughnessTexture.index, FormatUsageHint::UNORM);
	}

	// Load textures
	std::vector<VkDescriptorImageInfo> descImageInfos{};
	const uint32_t startingElement = textures.size();

	for (size_t i = 0; i < model.textures.size(); i++)
	{
		const auto& texture = model.textures[i];
		static tinygltf::Sampler defSampler;
		const auto& sampler = texture.sampler != -1 ? model.samplers[texture.sampler] : defSampler;
		const auto& image = model.images[texture.source];

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.arrayLayers = 1;
		imageInfo.extent = { uint32_t(image.width), uint32_t(image.height), 1 };
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.mipLevels = 1;
		imageInfo.format = getVkFormat(image.component, image.bits, getHint(i));
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		auto ptr = std::make_unique<Image>(device_, imageInfo);
		ptr->AttachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.addressModeU = getVkAddressMode(sampler.wrapS, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		samplerInfo.addressModeV = getVkAddressMode(sampler.wrapT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.minFilter = getVkFilter(sampler.minFilter, VK_FILTER_LINEAR);
		samplerInfo.magFilter = getVkFilter(sampler.magFilter, VK_FILTER_LINEAR);
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		
		ptr->AttachSampler(samplerInfo);

		size_t imageSize = image.image.size() * sizeof(uint8_t);
		Buffer stagingBuffer(device_, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		stagingBuffer.upload(image.image.data(), imageSize);

		CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
			Transition::UndefinedToTransferDestination(ptr->Get(), commandBuffer);

			ptr->Upload(commandBuffer, stagingBuffer.operator const VkBuffer &());

			Transition::TransferDestinationToShaderReadOptimal(ptr->Get(), commandBuffer);
		});

		VkDescriptorImageInfo descImageInfo{};
		descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descImageInfo.imageView = ptr->GetView();
		descImageInfo.sampler = ptr->GetSampler();
		descImageInfos.push_back(descImageInfo);

		textures.push_back(std::move(ptr));
	}

	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.descriptorCount = descImageInfos.size();
	writeSet.dstArrayElement = startingElement;
	writeSet.dstBinding = 1;
	writeSet.pImageInfo = descImageInfos.data();
	writeSet.dstSet = bindlessSet_;

	vkUpdateDescriptorSets(device_.device, 1, &writeSet, 0, nullptr);

	const auto opaquePipeline = [&](bool doubleSided, int mode) {
		VkPipeline pipeline;
		std::array stages = {
			CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader_),
			CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader_)
		};

		auto vertexBinding = StaticVertex::BindingDescription();
		auto vertexAttributes = StaticVertex::AttributesDescription();
		auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

		auto inputAssemblyState = CreateInfo::InputAssemblyState(getMode(mode));

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			VK_FALSE,
			doubleSided ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

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
		return pipeline;
	};

	const auto transparentPipeline = [&](bool doubleSided, int mode) {
		VkPipeline pipeline;
		std::array stages = {
			CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, transparentVertex_),
			CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, transparentFragment_)
		};

		auto vertexBinding = StaticVertex::BindingDescription();
		auto vertexAttributes = StaticVertex::AttributesDescription();
		auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

		auto inputAssemblyState = CreateInfo::InputAssemblyState(getMode(mode));

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			VK_FALSE,
			doubleSided ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

		auto multisampleState = CreateInfo::MultisampleState();

		auto depthStencilState = CreateInfo::DepthStencilState();
		depthStencilState.depthWriteEnable = false;

		std::array<VkPipelineColorBlendAttachmentState, 2> attachments{};
		attachments[0].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		attachments[0].blendEnable = VK_TRUE;
		attachments[0].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachments[0].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
		attachments[0].colorBlendOp = VK_BLEND_OP_ADD;
		attachments[0].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		attachments[0].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		attachments[0].alphaBlendOp = VK_BLEND_OP_ADD;

		attachments[1].colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		attachments[1].blendEnable = VK_TRUE;
		attachments[1].srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachments[1].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
		attachments[1].colorBlendOp = VK_BLEND_OP_ADD;
		attachments[1].srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		attachments[1].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		attachments[1].alphaBlendOp = VK_BLEND_OP_ADD;
		auto colorBlendState = CreateInfo::ColorBlendState(attachments.data(), 2);

		std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

		VkFormat swapchainFormat[] = {
			VK_FORMAT_R16G16B16A16_SFLOAT,
			VK_FORMAT_R16_SFLOAT
		};
		auto rendering = CreateInfo::Rendering(swapchainFormat, 2, device_.getDepthFormat());

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
		return pipeline;
	};

	// Recursive load node fn
	const auto loadNode = [&](const auto& loadNodeFn, const int nodeId) -> std::unique_ptr<Node> {
		const auto& node = model.nodes[nodeId];

		auto reprNode = std::make_unique<Node>();
		reprNode->name = node.name;
	
		reprNode->matrix = node.matrix.size() == 16 ? glm::mat4(glm::make_mat4(node.matrix.data())) : glm::mat4(1.0f);
		reprNode->translation = node.translation.size() == 3 ? glm::vec3(glm::make_vec3(node.translation.data())) : glm::vec3(0.0f);
		reprNode->scale = node.scale.size() == 3 ? glm::vec3(glm::make_vec3(node.scale.data())) : glm::vec3(1.0f);
		reprNode->rotation = node.rotation.size() == 4 ? glm::quat(glm::make_quat(node.rotation.data())) : glm::identity<glm::quat>();
		
		if (node.mesh != -1)
		{
			auto gpuMesh = std::make_unique<Mesh>();

			const auto& mesh = model.meshes[node.mesh];
			for (const auto& primitive : mesh.primitives)
			{
				const auto& material = model.materials[primitive.material];

				std::vector<StaticVertex> vertices{};
				std::vector<uint32_t> indices{};

				BufferHelper<float> position{ model, primitive, "POSITION" };
				BufferHelper<float> normal{ model, primitive, "NORMAL" };
				BufferHelper<float> tangent{ model, primitive, "TANGENT" };
				BufferHelper<float> uv{ model, primitive, "TEXCOORD_0" };

				for (size_t i = 0; i < position.count; i++)
				{
					StaticVertex vertex{};
					vertex.position = position.ptr ? glm::make_vec3(&position.ptr[position.stride * i]) : glm::vec3();
					vertex.normal = normal.ptr ? glm::make_vec3(&normal.ptr[normal.stride * i]) : glm::vec3();
					vertex.tangent = tangent.ptr ? glm::make_vec4(&tangent.ptr[tangent.stride * i]) : glm::vec4(1.0f);
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

				const auto colorId = material.pbrMetallicRoughness.baseColorTexture.index;
				const auto normalId = material.normalTexture.index;
				const auto mroId = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
				
				const auto transparent = false; //material.alphaMode == "BLEND";
				
				// Pipeline
				VkPipeline pipeline = transparent ? 
					transparentPipeline(material.doubleSided, primitive.mode) : 
					opaquePipeline(material.doubleSided, primitive.mode);
			

				gpuMesh->submeshes.push_back(Submesh{
					.vertexAlloc = vertexAlloc,
					.indexAlloc = indicesAlloc,
					.indexCount = indexCount,
					.firstIndex = firstIndex,
					.vertexOffset = vertexOffset,
					
					.pipeline = pipeline,
					.colorId = colorId,
					.normalId = normalId,
					.mroId = mroId,
					.transparent = transparent
				});
				
			}
			reprNode->mesh = std::move(gpuMesh);
			
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

void Scene::draw(VkCommandBuffer commandBuffer, Camera& camera, VkImageView colorView, VkImageView depthView)
{
	if (!accum || !reveal)
	{
		recreateAccumReveal(camera.viewportWidth, camera.viewportHeight);
	}
	if (compositeWidth != camera.viewportWidth || compositeHeight != camera.viewportHeight)
	{
		recreateAccumReveal(camera.viewportWidth, camera.viewportHeight);
	}

	{ // Global UBO
		GlobalUniform uniform{};
		uniform.projection = camera.calculateProjection();
		uniform.view = camera.calculateView();
		uniform.viewPos = camera.position;
		globalUniformBuffers_[frameCount_]->upload(&uniform, sizeof(uniform));
	}

	// Group stuff
	struct RenderComponent
	{
		glm::mat4 model;
		std::vector<Submesh> meshes;
	};

	std::vector<RenderComponent> opaqueGroup;
	std::vector<RenderComponent> transparentGroup;

	const auto group = [&](const auto& groupFn, const std::unique_ptr<Node>& node, glm::mat4 parent) -> void {
		glm::mat4 model = parent * node->getMatrix();
		
		if (node->mesh)
		{
			RenderComponent opaqueComponent;
			RenderComponent transparentComponent;
			opaqueComponent.model = model;
			transparentComponent.model = model;

			for (const auto& submesh : node->mesh->submeshes)
			{
				if (submesh.transparent)
				{
					transparentComponent.meshes.push_back(submesh);
				}
				else
				{
					opaqueComponent.meshes.push_back(submesh);
				}
			}

			if (!opaqueComponent.meshes.empty())
			{
				opaqueGroup.push_back(opaqueComponent);
			}
			if (!transparentComponent.meshes.empty())
			{
				transparentGroup.push_back(transparentComponent);
			}
		}

		for (const auto& child : node->childrens)
		{
			groupFn(groupFn, child, model);
		}
	};

	for (const auto& node : nodes)
	{
		group(group, node, glm::mat4(1.0f));
	}


	// Begin Rendering (Opaque)
	{
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = colorView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.clearValue = { 0.f, 0.f, 0.f, 1.f };

		VkRenderingAttachmentInfo depthAttachment{};
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = depthView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		depthAttachment.clearValue = { 1.0f, 0 };

		VkRenderingInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderInfo.renderArea.offset = { 0, 0 };
		renderInfo.renderArea.extent = { uint32_t(camera.viewportWidth), uint32_t(camera.viewportHeight) };
		renderInfo.layerCount = 1;
		renderInfo.colorAttachmentCount = 1;
		renderInfo.pColorAttachments = &colorAttachment;
		renderInfo.pDepthAttachment = &depthAttachment;

		vkCmdBeginRendering(commandBuffer, &renderInfo);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = camera.viewportHeight;
		viewport.width = camera.viewportWidth;
		viewport.height = -camera.viewportHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { uint32_t(camera.viewportWidth), uint32_t(camera.viewportHeight) };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		size_t offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->operator const VkBuffer& (), &offset);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer->operator const VkBuffer& (), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSets_[frameCount_], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &bindlessSet_, 0, nullptr);

		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = "Opaque";
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
		
		for (const auto& opaque : opaqueGroup)
		{
			PushConstant push{};
			push.model = opaque.model;
			for (const auto& submesh : opaque.meshes)
			{
				push.colorId = submesh.colorId;
				push.normalId = submesh.normalId;
				push.mroId = submesh.mroId;
				vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, submesh.pipeline);
				vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, submesh.vertexOffset, 0);
			}
		}

		vkCmdEndDebugUtilsLabelEXT(commandBuffer);

		vkCmdEndRendering(commandBuffer);
	}

	{ // Render Pass (Transparent)
		std::array<VkRenderingAttachmentInfo, 2> attachments{};
		attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachments[0].imageView = accum->GetView();
		attachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].clearValue = { 0.f, 0.f, 0.f, 0.f };
		attachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachments[1].imageView = reveal->GetView();
		attachments[1].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[1].clearValue = { 1.f, 0.f, 0.f, 0.f };

		VkRenderingAttachmentInfo depthAttachment{};
		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = depthView;
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // dont clear this please...
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRenderingInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderInfo.renderArea.offset = { 0, 0 };
		renderInfo.renderArea.extent = { uint32_t(compositeWidth), uint32_t(compositeHeight) };
		renderInfo.layerCount = 1;
		renderInfo.colorAttachmentCount = static_cast<uint32_t>(attachments.size());
		renderInfo.pColorAttachments = attachments.data();
		renderInfo.pDepthAttachment = &depthAttachment;

		vkCmdBeginRendering(commandBuffer, &renderInfo);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = camera.viewportHeight;
		viewport.width = camera.viewportWidth;
		viewport.height = -camera.viewportHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { uint32_t(camera.viewportWidth), uint32_t(camera.viewportHeight) };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		size_t offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->operator const VkBuffer& (), &offset);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer->operator const VkBuffer& (), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSets_[frameCount_], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &bindlessSet_, 0, nullptr);
	

		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = "Transparent";
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);
		for (const auto& transparent : transparentGroup)
		{
			PushConstant push{};
			push.model = transparent.model;
			for (const auto& submesh : transparent.meshes)
			{
				push.colorId = submesh.colorId;
				vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, submesh.pipeline);
				vkCmdDrawIndexed(commandBuffer, submesh.indexCount, 1, submesh.firstIndex, submesh.vertexOffset, 0);
			}
		}
		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
		vkCmdEndRendering(commandBuffer);
	}

	Transition::ColorAttachmentToShaderReadOptimal(accum->Get(), commandBuffer);
	Transition::ColorAttachmentToShaderReadOptimal(reveal->Get(), commandBuffer);

	{ // Compositing
		VkRenderingAttachmentInfo colorAttachment{};
		colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		colorAttachment.imageView = colorView;
		colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; // dont clear this please.
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

		VkRenderingInfo renderInfo{};
		renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		renderInfo.renderArea.offset = { 0, 0 };
		renderInfo.renderArea.extent = { uint32_t(camera.viewportWidth), uint32_t(camera.viewportHeight) };
		renderInfo.layerCount = 1;
		renderInfo.colorAttachmentCount = 1;
		renderInfo.pColorAttachments = &colorAttachment;

		vkCmdBeginRendering(commandBuffer, &renderInfo);

		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = "Compositing";
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

		VkViewport viewport{};
		// viewport.x = 0.0f;
		viewport.width = camera.viewportWidth;
		viewport.height = camera.viewportHeight;
		//viewport.y = camera.viewportHeight;
		//viewport.width = camera.viewportWidth;
		//viewport.height = -camera.viewportHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { uint32_t(camera.viewportWidth), uint32_t(camera.viewportHeight) };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositingPipeline);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, compositingPipelineLayout, 0, 1, &compositingSet, 0, nullptr);

		size_t offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &quadVerticesBuffer->operator const VkBuffer& (), &offset);
		vkCmdBindIndexBuffer(commandBuffer, quadIndicesBuffer->operator const VkBuffer& (), 0, VK_INDEX_TYPE_UINT16);

		vkCmdDrawIndexed(commandBuffer, quadIndices.size(), 1, 0, 0, 0);
		
		vkCmdEndDebugUtilsLabelEXT(commandBuffer);

		vkCmdEndRendering(commandBuffer);
	}

	Transition::ShaderReadOptimalToColorAttachment(accum->Get(), commandBuffer);
	Transition::ShaderReadOptimalToColorAttachment(reveal->Get(), commandBuffer);


	frameCount_ = (frameCount_ + 1) % maxFramesInFlight;

	cleanupRecycling();
}

Scene::~Scene()
{
	vkDestroyShaderModule(device_.device, vertexShader_, nullptr);
	vkDestroyShaderModule(device_.device, fragmentShader_, nullptr);
	vkDestroyShaderModule(device_.device, transparentVertex_, nullptr);
	vkDestroyShaderModule(device_.device, transparentFragment_, nullptr);

	const auto deleteNode = [&](const auto& deleteNodeFn, const std::unique_ptr<Node>& node) -> void {
		if (node->mesh)
		{
			for (const auto& submesh : node->mesh->submeshes)
			{
				// SPDLOG_INFO("Allocation at offset V: {} I: {} with something like vkCmdDrawIndexed(c, {}, 1, {}, {}, 0)", submesh.vertexAlloc.offset, submesh.indexAlloc.offset, submesh.indexCount, submesh.firstIndex, submesh.vertexOffset);
				vmaVirtualFree(virtualVertex_, submesh.vertexAlloc.allocation);
				vmaVirtualFree(virtualIndices_, submesh.indexAlloc.allocation);

				vkDestroyPipeline(device_.device, submesh.pipeline, nullptr);
			}
		}

		for (const auto& child : node->childrens)
		{
			deleteNodeFn(deleteNodeFn, child);
		}
	};

	for (const auto& node : nodes)
	{
		deleteNode(deleteNode, node);
	}

	
	textures.clear();
	globalUniformBuffers_.clear();

	vkDestroyDescriptorPool(device_.device, globalPool_, nullptr);
	vkDestroyDescriptorPool(device_.device, bindlessPool, nullptr);

	vkDestroyDescriptorSetLayout(device_.device, globalSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, bindlessSetLayout, nullptr);

	vkDestroyPipelineLayout(device_.device, pipelineLayout_, nullptr);

	vkDestroyPipeline(device_.device, compositingPipeline, nullptr);
	vkDestroyPipelineLayout(device_.device, compositingPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, compositingSetLayout, nullptr);
	vkDestroyDescriptorPool(device_.device, compositingPool, nullptr);

	vmaDestroyVirtualBlock(virtualVertex_);
	vmaDestroyVirtualBlock(virtualIndices_);

	vertexBuffer.reset();
	indexBuffer.reset();
}

void Scene::recreateAccumReveal(int width, int height)
{	
	if (accum)
	{
		recycling.push_back({ std::move(accum), maxFramesInFlight });
	}
	if (reveal)
	{
		recycling.push_back({ std::move(reveal), maxFramesInFlight });
	}

	// Weighted Blend Order Requirements
	VkImageCreateInfo accumCI{};
	accumCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	accumCI.imageType = VK_IMAGE_TYPE_2D;
	accumCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	accumCI.arrayLayers = 1;
	accumCI.extent = { uint32_t(width), uint32_t(height), 1 };
	accumCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	accumCI.samples = VK_SAMPLE_COUNT_1_BIT;
	accumCI.mipLevels = 1;
	accumCI.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	accumCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	accum = std::make_unique<Image>(device_, accumCI);

	accum->AttachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	accum->AttachSampler(samplerInfo);

	VkImageCreateInfo revealCI{};
	revealCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	revealCI.imageType = VK_IMAGE_TYPE_2D;
	revealCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	revealCI.arrayLayers = 1;
	revealCI.extent = { uint32_t(width), uint32_t(height), 1 };
	revealCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	revealCI.samples = VK_SAMPLE_COUNT_1_BIT;
	revealCI.mipLevels = 1;
	revealCI.format = VK_FORMAT_R16_SFLOAT;
	revealCI.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	reveal = std::make_unique<Image>(device_, revealCI);

	reveal->AttachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	reveal->AttachSampler(samplerInfo);

	CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
		Transition::UndefinedToColorAttachment(accum->Get(), commandBuffer);
		Transition::UndefinedToColorAttachment(reveal->Get(), commandBuffer);
	});

	VkDescriptorImageInfo accumImageInfo{};
	accumImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	accumImageInfo.imageView = accum->GetView();
	accumImageInfo.sampler = accum->GetSampler();

	VkDescriptorImageInfo revealImageInfo{};
	revealImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	revealImageInfo.imageView = reveal->GetView();
	revealImageInfo.sampler = reveal->GetSampler();

	auto writeSet = [](VkDevice device, VkDescriptorImageInfo& imageInfo, uint32_t binding, VkDescriptorSet set)
	{
		VkWriteDescriptorSet writeSet{};
		writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeSet.descriptorCount = 1;
		writeSet.dstArrayElement = 0;
		writeSet.dstBinding = binding;
		writeSet.pImageInfo = &imageInfo;
		writeSet.dstSet = set;

		vkUpdateDescriptorSets(device, 1, &writeSet, 0, nullptr);
	};
	writeSet(device_.device, accumImageInfo, 0, compositingSet);
	writeSet(device_.device, revealImageInfo, 1, compositingSet);

	compositeWidth = width;
	compositeHeight = height;
}

void Scene::cleanupRecycling()
{
	for (auto& recycle : recycling)
	{
		recycle.second -= 1;
	}
	std::erase_if(recycling, [](auto& pair) {
		return pair.second <= 0;
	});
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

void Scene::initialiseDefaultTextures()
{
	std::array<uint8_t, 4> colorData_{ 0xff,0xff,0xff,0xff };
	auto commandBuffer = CreateInfo::allocateCommandBuffer(device_.device, device_.graphicsPool);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	Buffer stagingBuffer(device_, 4 * sizeof(uint8_t), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	stagingBuffer.upload(colorData_.data(), 4 * sizeof(uint8_t));
	{
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCI.arrayLayers = 1;
		imageCI.extent = { 1, 1, 1 };
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.mipLevels = 1;
		imageCI.format = VK_FORMAT_R8G8B8A8_SRGB;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		defaultTextures[0] = std::make_unique<Image>(device_, imageCI);

		defaultTextures[0]->AttachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		defaultTextures[0]->AttachSampler(samplerCI);

		Transition::UndefinedToTransferDestination(defaultTextures[0]->Get(), commandBuffer);

		defaultTextures[0]->Upload(commandBuffer, stagingBuffer);
		
		Transition::TransferDestinationToShaderReadOptimal(defaultTextures[0]->Get(), commandBuffer);
	}

	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(device_.graphicsQueue.queue, 1, &submitInfo, nullptr);
	vkQueueWaitIdle(device_.graphicsQueue.queue);

	// Write
	VkDescriptorImageInfo descImageInfo{};
	descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	descImageInfo.imageView = defaultTextures[0]->GetView();
	descImageInfo.sampler = defaultTextures[0]->GetSampler();
	
	VkWriteDescriptorSet writeSet{};
	writeSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeSet.descriptorCount = 1;
	writeSet.dstArrayElement = 0;
	writeSet.dstBinding = 0;
	writeSet.pImageInfo = &descImageInfo;
	writeSet.dstSet = bindlessSet_;

	vkUpdateDescriptorSets(device_.device, 1, &writeSet, 0, nullptr);
}

glm::mat4 Node::getMatrix() const
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}
