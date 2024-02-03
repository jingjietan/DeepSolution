#include "Scene.h"
#include "Core/Common.h"
#include "Core/Buffer.h"
#include "Core/Device.h"
#include "Core/Shader.h"
#include "Core/Image.h"
#include "Core/Transition.h"
#include "Core/Cube.h"
#include "Common/Bench.h"

#include <tiny_gltf.h>
#include <spdlog/spdlog.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <volk.h>
#include "Light.h"
#include <stb_image.h>
#include "Core/DescriptorWrite.h"

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


	struct SpecializationData
	{
		VkBool32 alphaMask;
		float alphaMaskCutoff;
	};

}

Scene::Scene(Device& device) : device_(device)
{
	maxFramesInFlight = device.getMaxFramesInFlight();

	// TODO: make these buffers resizable.
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
	indexBuffer = std::make_unique<Buffer>(device_, indexBufferSize,
		VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
		// VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | 
		VK_BUFFER_USAGE_TRANSFER_DST_BIT,
		VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

	const auto cubeSize = sizeof(BasicVertex) * cubeVertices.size();
	cubeBuffer_ = std::make_unique<Buffer>(device_, cubeSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	cubeBuffer_->upload(cubeVertices.data(), cubeSize);

	flattenCubemap_ = std::make_unique<FlattenCubemap>(device_, cubeBuffer_);
	irradianceCubemap_ = std::make_unique<IrradianceCubemap>(device_, cubeBuffer_);
	prefilterCubemap_ = std::make_unique<PrefilterCubemap>(device_, cubeBuffer_);

	// Compositing Pipeline
	/*
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

		auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);

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
	*/
}

void Scene::loadGLTF(const std::string& path, VkShaderModule vertexShader, VkShaderModule fragmentShader, VkDescriptorSet imageSet, VkPipelineLayout layout)
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
		const auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(image.width, image.height)))) + 1;
		const VkImageSubresourceRange range = { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT , .baseMipLevel = 0, .levelCount = mipLevels, .baseArrayLayer = 0, .layerCount = 1 };

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.arrayLayers = 1;
		imageInfo.extent = { uint32_t(image.width), uint32_t(image.height), 1 };
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.mipLevels = mipLevels;
		imageInfo.format = getVkFormat(image.component, image.bits, getHint(i));
		imageInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		auto ptr = std::make_unique<Image>(device_, imageInfo);
		ptr->attachImageView(range);

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.addressModeU = getVkAddressMode(sampler.wrapS, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		samplerInfo.addressModeV = getVkAddressMode(sampler.wrapT, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER);
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.minFilter = getVkFilter(sampler.minFilter, VK_FILTER_LINEAR);
		samplerInfo.magFilter = getVkFilter(sampler.magFilter, VK_FILTER_LINEAR);
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
		samplerInfo.maxLod = static_cast<float>(mipLevels);
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = device_.deviceProperties.limits.maxSamplerAnisotropy;
		
		ptr->attachSampler(samplerInfo);

		size_t imageSize = image.image.size() * sizeof(uint8_t);
		Buffer stagingBuffer(device_, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		stagingBuffer.upload(image.image.data(), imageSize);

		CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
			Transition::UndefinedToTransferDestination(ptr->get(), commandBuffer, range);

			ptr->upload(commandBuffer, stagingBuffer);

			Image::generateMipmaps(ptr->get(), image.width, image.height, mipLevels, commandBuffer);
		});

		VkDescriptorImageInfo descImageInfo{};
		descImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		descImageInfo.imageView = ptr->getView();
		descImageInfo.sampler = ptr->getSampler();
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
	writeSet.dstSet = imageSet;

	vkUpdateDescriptorSets(device_.device, 1, &writeSet, 0, nullptr);
/*
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
*/
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

				VkDeviceSize vertexSizeOffset;
				VkDeviceSize indicesSizeOffset;
				const auto vertexAlloc = performAllocation(virtualVertex_, verticesSize, vertexSizeOffset);
				const auto indicesAlloc = performAllocation(virtualIndices_, indicesSize, indicesSizeOffset);

				stagingBuffer.upload(vertices.data(), verticesSize);
				CreateInfo::performOneTimeAction(device_.device, device_.transferQueue.queue, device_.transferPool, [&](VkCommandBuffer commandBuffer) {
					stagingBuffer.copy(*vertexBuffer, commandBuffer, verticesSize, vertexSizeOffset, 0);
				});

				stagingBuffer.upload(indices.data(), indicesSize);
				CreateInfo::performOneTimeAction(device_.device, device_.transferQueue.queue, device_.transferPool, [&](VkCommandBuffer commandBuffer) {
					stagingBuffer.copy(*indexBuffer, commandBuffer, indicesSize, indicesSizeOffset, 0);
				});

				const auto firstIndex = static_cast<uint32_t>(indicesSizeOffset / sizeof(uint32_t));
				const auto vertexOffset = static_cast<int32_t>(vertexSizeOffset / sizeof(StaticVertex));

				const auto indexCount = static_cast<uint32_t>(indices.size());

				const auto colorId = material.pbrMetallicRoughness.baseColorTexture.index;
				const auto normalId = material.normalTexture.index;
				const auto mroId = material.pbrMetallicRoughness.metallicRoughnessTexture.index;
				const auto emissiveId = material.emissiveTexture.index;
				
				const auto transparent = false; //material.alphaMode == "BLEND";

				MaterialCharacteristic matCh{};
				matCh.doubleSided = material.doubleSided;
				matCh.mode = primitive.mode;
				matCh.alphaMask = material.alphaMode == "MASK";
				matCh.alphaMaskCutoff = material.alphaCutoff;
				
				// Pipeline
				VkPipeline pipeline = getOrCreatePipeline(matCh, vertexShader, fragmentShader, imageSet, layout);
			
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
					.emissiveId = emissiveId,
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

void Scene::loadCubeMap(const std::string& path, VkDescriptorSet ibrSet)
{
	auto s = Bench::record();
	
	int x, y, nr;
	auto data = stbi_loadf(path.c_str(), &x, &y, &nr, 4);
	check(data);

	const auto format = VK_FORMAT_R32G32B32A32_SFLOAT;
	const auto mipLevels = Image::calculateMaxMiplevels(x, y);

	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.arrayLayers = 1;
	imageCI.extent = { uint32_t(x), uint32_t(y), 1 };
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.mipLevels = mipLevels;
	imageCI.format = format;
	imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

	VkSamplerCreateInfo samplerInfo = 
		CreateInfo::SamplerCI(mipLevels, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);
	
	size_t imageSize = static_cast<size_t>(x) * y * 4 * sizeof(uint32_t);
	Buffer stagingBuffer(device_, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	stagingBuffer.upload(data, imageSize);
	stbi_image_free(data);

	auto img = std::make_unique<Image>(device_, imageCI);
	img->attachImageView(img->getFullRange());
	img->attachSampler(samplerInfo);

	std::vector<VkImageView> temp;
	
	CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
		img->UndefinedToTransferDestination(commandBuffer);
		img->upload(commandBuffer, stagingBuffer);
		img->generateMaxMipmaps(commandBuffer);

		cubeMap_ = flattenCubemap_->convert(commandBuffer, img.get(), 1024);
		cubeMap_->ColorAttachmentToTransferDestination(commandBuffer);
		cubeMap_->generateMaxCubeMipmaps(commandBuffer);

		irradianceMap_ = irradianceCubemap_->convert(commandBuffer, cubeMap_.get(), 32); // 32 by 32 irradiance
		prefilterMap_ = prefilterCubemap_->precomputeFilter(commandBuffer, cubeMap_.get(), 128, temp); // 128 by 128 prefilter
		brdfMap_ = prefilterCubemap_->precomputerBRDF(commandBuffer, 512, 512);

		irradianceMap_->ColorAttachmentToShaderReadOptimal(commandBuffer);
		prefilterMap_->ColorAttachmentToShaderReadOptimal(commandBuffer);
		brdfMap_->ColorAttachmentToShaderReadOptimal(commandBuffer);
	});

	auto e = Bench::record();
	SPDLOG_INFO("Cubemap took: {}ms.", Bench::diff<float>(s, e));

	// temp is ready to clear
	for (const auto& t : temp)
	{
		vkDestroyImageView(device_.device, t, nullptr);
	}

	setName(device_.device, cubeMap_->get(), path);

	DescriptorWrite writer;
	writer.add(ibrSet, 0, 0, ImageType::CombinedSampler, 1, irradianceMap_->getSampler(), irradianceMap_->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.add(ibrSet, 1, 0, ImageType::CombinedSampler, 1, prefilterMap_->getSampler(), prefilterMap_->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.add(ibrSet, 2, 0, ImageType::CombinedSampler, 1, brdfMap_->getSampler(), brdfMap_->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.write(device_.device);
}

Scene::Drawbles Scene::getDrawables() const
{
	// Group stuff
	
	struct RenderObject
	{
		glm::mat4 model;
		const Submesh* submesh;
	};
	std::unordered_map<VkPipeline, std::vector<RenderObject>> grouping;

	const auto group = [&](const auto& groupFn, const std::unique_ptr<Node>& node, glm::mat4 parent) -> void {
		glm::mat4 model = parent * node->getMatrix();
		if (node->mesh)
		{
			for (const auto& submesh : node->mesh->submeshes)
			{
				auto& group = grouping[submesh.pipeline];
				group.push_back(RenderObject{ .model = model, .submesh = &submesh });
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

	std::vector<IndirectDrawParam> indirectParams;
	std::vector<VkDrawIndexedIndirectCommand> commands;
	PipelineGroups opaqueGroup;

	size_t objectCounter = 0;
	for (const auto& group : grouping)
	{
		const size_t groupOffset = objectCounter;
		size_t groupCounter = 0;
		for (const auto& object : group.second)
		{
			IndirectDrawParam param{};
			param.model = object.model;
			param.colorId = object.submesh->colorId;
			param.normalId = object.submesh->normalId;
			param.mroId = object.submesh->mroId;
			param.emissiveId = object.submesh->emissiveId;
			indirectParams.push_back(param);

			VkDrawIndexedIndirectCommand command{};
			command.firstIndex = object.submesh->firstIndex;
			command.firstInstance = 0;
			command.indexCount = object.submesh->indexCount;
			command.instanceCount = 1;
			command.vertexOffset = object.submesh->vertexOffset;
			commands.push_back(command);

			groupCounter++;
		}

		opaqueGroup[group.first] = { .offset = static_cast<uint32_t>(groupOffset), .count = static_cast<uint32_t>(groupCounter) };
		objectCounter += groupCounter;
	}

	return std::make_tuple(opaqueGroup, indirectParams, commands);
}

VkBuffer Scene::getVertexBuffer() const
{
	return *vertexBuffer;
}

VkBuffer Scene::getIndexBuffer() const
{
	return *indexBuffer;
}

/*

void Scene::draw(VkCommandBuffer commandBuffer, const State& state, VkImageView colorView, VkImageView depthView)
{
	if (!accum || !reveal)
	{
		recreateAccumReveal(state.camera_->viewportWidth, state.camera_->viewportHeight);
	}
	if (compositeWidth != state.camera_->viewportWidth || compositeHeight != state.camera_->viewportHeight)
	{
		recreateAccumReveal(state.camera_->viewportWidth, state.camera_->viewportHeight);
	}

	{ // Global UBO
		GlobalUniform uniform{};
		uniform.projection = state.camera_->calculateProjection();
		uniform.view = state.camera_->calculateView();
		uniform.viewPos = state.camera_->getPosition();
		globalUniformBuffers_[frameCount_]->upload(&uniform, sizeof(uniform));
	}

	{ // Light
		LightUpload upload{};
		upload.count = static_cast<int32_t>(state.lights_.size());
		lightBuffer[frameCount_]->upload(&upload, sizeof(upload));
		lightBuffer[frameCount_]->upload(state.lights_.data(), state.lights_.size() * sizeof(Light), sizeof(upload));
	}

	// Group stuff
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

	std::vector<IndirectDrawParam> indirectParams;
	std::vector<VkDrawIndexedIndirectCommand> commands;
	for (const auto& item : opaqueGroup)
	{
		for (const auto& mesh : item.meshes)
		{
			IndirectDrawParam param{};
			param.model = item.model;
			param.colorId = mesh.colorId;
			param.normalId = mesh.normalId;
			param.mroId = mesh.mroId;
			param.emissiveId = mesh.emissiveId;
			indirectParams.push_back(param);

			VkDrawIndexedIndirectCommand command{};
			command.firstIndex = mesh.firstIndex;
			command.firstInstance = 0;
			command.indexCount = mesh.indexCount;
			command.instanceCount = 1;
			command.vertexOffset = mesh.vertexOffset;
			commands.push_back(command);
		}
	}

	//indirectBuffer->upload(commands.data(), commands.size() * sizeof(VkDrawIndexedIndirectCommand));
	//perMeshDrawDataBuffer[frameCount_]->upload(indirectParams.data(), indirectParams.size() * sizeof(IndirectDrawParam));

	// Begin Rendering (Opaque)
	{
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = "Opaque";
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

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
		renderInfo.renderArea.extent = { uint32_t(state.camera_->viewportWidth), uint32_t(state.camera_->viewportHeight) };
		renderInfo.layerCount = 1;
		renderInfo.colorAttachmentCount = 1;
		renderInfo.pColorAttachments = &colorAttachment;
		renderInfo.pDepthAttachment = &depthAttachment;

		vkCmdBeginRendering(commandBuffer, &renderInfo);

		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = state.camera_->viewportHeight;
		viewport.width = state.camera_->viewportWidth;
		viewport.height = -state.camera_->viewportHeight;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = { uint32_t(state.camera_->viewportWidth), uint32_t(state.camera_->viewportHeight) };
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		size_t offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer->operator const VkBuffer& (), &offset);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer->operator const VkBuffer& (), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSets_[frameCount_], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &bindlessSet_, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 2, 1, &ibrSet, 0, nullptr);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, opaqueGroup[0].meshes[0].pipeline);
		vkCmdDrawIndexedIndirect(commandBuffer, indirectBuffer->operator const VkBuffer& (), 0, commands.size(), sizeof(VkDrawIndexedIndirectCommand));

		vkCmdEndRendering(commandBuffer);

		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	/* Transparent is WIP

	{ // Render Pass (Transparent)
		std::array<VkRenderingAttachmentInfo, 2> attachments{};
		attachments[0].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachments[0].imageView = accum->getView();
		attachments[0].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachments[0].clearValue = { 0.f, 0.f, 0.f, 0.f };
		attachments[1].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		attachments[1].imageView = reveal->getView();
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

	Transition::ColorAttachmentToShaderReadOptimal(accum->get(), commandBuffer);
	Transition::ColorAttachmentToShaderReadOptimal(reveal->get(), commandBuffer);

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
		viewport.width = camera.viewportWidth;
		viewport.height = camera.viewportHeight;
		// viewport.x = 0.0f;
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

	const VkExtent2D extent = { uint32_t(state.camera_->viewportWidth), uint32_t(state.camera_->viewportHeight) };
	// infiniteGrid_->draw(commandBuffer, globalSets_[frameCount_], colorView, depthView, extent);

	skybox_->render(commandBuffer, state.camera_->calculateProjection(), state.camera_->calculateView(), colorView, depthView, extent);

	Transition::ShaderReadOptimalToColorAttachment(accum->get(), commandBuffer);
	Transition::ShaderReadOptimalToColorAttachment(reveal->get(), commandBuffer);
	

	frameCount_ = (frameCount_ + 1) % maxFramesInFlight;

	cleanupRecycling();
}
*/

VkPipeline Scene::getOrCreatePipeline(const MaterialCharacteristic& character, VkShaderModule vertexShader, VkShaderModule fragmentShader, VkDescriptorSet imageSet, VkPipelineLayout layout)
{
	if (auto it = pipelines.find(character); it != pipelines.end())
	{
		return it->second;
	}
	else
	{
		const auto opaquePipeline = [&](bool doubleSided, int mode, bool alphaMask, float alphaMaskCutoff) {
			VkPipeline pipeline;
			std::array stages = {
				CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
				CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
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

			VkFormat swapchainFormat = VK_FORMAT_R32G32B32A32_SFLOAT; //hdr
			auto rendering = CreateInfo::Rendering(&swapchainFormat, 1, device_.getDepthFormat());

			SpecializationData data{};
			data.alphaMask = alphaMask;
			data.alphaMaskCutoff = alphaMaskCutoff;

			std::array<VkSpecializationMapEntry, 2> mapEntries{};
			mapEntries[0].constantID = 0;
			mapEntries[0].offset = offsetof(SpecializationData, alphaMask);
			mapEntries[0].size = sizeof(SpecializationData::alphaMask);
			mapEntries[1].constantID = 1;
			mapEntries[1].offset = offsetof(SpecializationData, alphaMaskCutoff);
			mapEntries[1].size = sizeof(SpecializationData::alphaMaskCutoff);
			VkSpecializationInfo specInfo{};
			specInfo.dataSize = sizeof(SpecializationData);
			specInfo.mapEntryCount = static_cast<uint32_t>(mapEntries.size());
			specInfo.pMapEntries = mapEntries.data();
			specInfo.pData = &data;
			stages[1].pSpecializationInfo = &specInfo;

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
			pipelineCreateInfo.layout = layout;

			Connect(pipelineCreateInfo, rendering);

			check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
			return pipeline;
		};
		pipelines[character] = opaquePipeline(character.doubleSided, character.mode, character.alphaMask, character.alphaMaskCutoff);
		return pipelines[character];
	}
}

Image& Scene::getCubeMap() const
{
	return *cubeMap_;
}

Image& Scene::getPrefilter() const
{
	return *prefilterMap_;
}

Image& Scene::getIrradiance() const
{
	return *irradianceMap_;
}

Image& Scene::getBRDFMap() const
{
	return *brdfMap_;
}

std::shared_ptr<Buffer> Scene::getCubeBuffer() const
{
	return cubeBuffer_;
}

Scene::~Scene()
{
	const auto deleteNode = [&](const auto& deleteNodeFn, const std::unique_ptr<Node>& node) -> void {
		if (node->mesh)
		{
			for (const auto& submesh : node->mesh->submeshes)
			{
				vmaVirtualFree(virtualVertex_, submesh.vertexAlloc);
				vmaVirtualFree(virtualIndices_, submesh.indexAlloc);
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

	for (const auto& pipeline : pipelines)
	{
		vkDestroyPipeline(device_.device, pipeline.second, nullptr);
	}
	
	textures.clear();

	vmaDestroyVirtualBlock(virtualVertex_);
	vmaDestroyVirtualBlock(virtualIndices_);

	vertexBuffer.reset();
	indexBuffer.reset();
}

/*
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

	accum->attachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	accum->attachSampler(samplerInfo);

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

	reveal->attachImageView(VK_IMAGE_ASPECT_COLOR_BIT);

	reveal->attachSampler(samplerInfo);

	CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
		Transition::UndefinedToColorAttachment(accum->get(), commandBuffer, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
		Transition::UndefinedToColorAttachment(reveal->get(), commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	});

	VkDescriptorImageInfo accumImageInfo{};
	accumImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	accumImageInfo.imageView = accum->getView();
	accumImageInfo.sampler = accum->getSampler();

	VkDescriptorImageInfo revealImageInfo{};
	revealImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	revealImageInfo.imageView = reveal->getView();
	revealImageInfo.sampler = reveal->getSampler();

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
*/

VmaVirtualAllocation Scene::performAllocation(VmaVirtualBlock block, VkDeviceSize size, VkDeviceSize& offset)
{
	VmaVirtualAllocation allocation{};
	VmaVirtualAllocationCreateInfo allocationCI{};
	allocationCI.size = size;
	auto res = vmaVirtualAllocate(block, &allocationCI, &allocation, &offset);
	if (res == VK_ERROR_OUT_OF_DEVICE_MEMORY)
	{
		SPDLOG_WARN("Allocation failed!");
	}
	return allocation;
}

glm::mat4 Node::getMatrix() const
{
	return glm::translate(glm::mat4(1.0f), translation) * glm::mat4(rotation) * glm::scale(glm::mat4(1.0f), scale) * matrix;
}
