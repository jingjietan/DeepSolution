#include "PrefilterCubemap.h"

#include "../Core/Common.h"
#include "../Core/Shader.h"
#include "../Core/Device.h"
#include "../Core/Image.h"
#include "../Core/Transition.h"
#include "../Core/Buffer.h"
#include "../Core/DescriptorWrite.h"
#include "../Core/Cube.h"
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>

PrefilterCubemap::PrefilterCubemap(Device& device, const std::shared_ptr<Buffer>& buffer) : device_(device), cubeBuffer(buffer)
{
	uniformBuffer = std::make_unique<Buffer>(device_, 7 * sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	ShaderReflect reflect;
	reflect.add("Shaders/Irradiance.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	reflect.add("Shaders/Prefilter.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	prefilterDescLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
	prefilterPool = reflect.retrieveDescriptorPool(device_.device);
	prefilterSet = reflect.retrieveSet(device_.device, prefilterPool, prefilterDescLayout)[0];
	prefilterLayout = reflect.retrievePipelineLayout(device_.device, { prefilterDescLayout });
	auto prefilterStages = reflect.retrieveShaderModule(device_.device);

	prefilterPipeline = [&]() {
		VkPipeline pipeline;
		const auto stages = reflect.getStages(prefilterStages);

		auto vertexBinding = BasicVertex::BindingDescription();
		auto vertexAttributes = BasicVertex::PositionAttributesDescription();
		auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

		auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			false,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

		auto multisampleState = CreateInfo::MultisampleState();

		auto depthStencilState = CreateInfo::DepthStencilState();

		auto attachment = CreateInfo::ColorBlendAttachment();
		auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);

		std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

		VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		auto rendering = CreateInfo::Rendering(&format, 1, VK_FORMAT_UNDEFINED);
		rendering.viewMask = 0b111111;

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
		pipelineCreateInfo.layout = prefilterLayout;

		Connect(pipelineCreateInfo, rendering);

		check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));

		return pipeline;
	}();

	DescriptorWrite writer;
	writer.add(prefilterSet, 0, 0, BufferType::Uniform, 1, uniformBuffer->operator const VkBuffer & (), 0, VK_WHOLE_SIZE);
	writer.write(device_.device);

	const auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.f);
	uniformBuffer->upload(&projection, sizeof(projection));
	std::array views{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
	uniformBuffer->upload(views.data(), views.size() * sizeof(glm::mat4), sizeof(glm::mat4));

	ShaderReflect::deleteModules(device_.device, prefilterStages);
}

std::unique_ptr<Image> PrefilterCubemap::convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int dim)
{
	VkExtent2D extent = { uint32_t(dim), uint32_t(dim) };
	const uint32_t maxMipLevels = 5;

	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, maxMipLevels, 0, 6 };
	VkImageCreateInfo imageCI = CreateInfo::Image2DCI(extent, maxMipLevels, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
	imageCI.arrayLayers = 6; //for cubemap
	imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	auto img = std::make_unique<Image>(device_, imageCI);

	Transition::UndefinedToColorAttachment(img->Get(), commandBuffer, range);

	DescriptorWrite writer;
	writer.add(prefilterSet, 1, 0, ImageType::CombinedSampler, 1, sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.write(device_.device);

	VkDebugUtilsLabelEXT label{};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = "Prefilter";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue = { 0.f, 0.f, 0.f, 1.f };

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.viewMask = 0b111111; // multiview 6 times.

	for (uint32_t mipLevel = 0; mipLevel < maxMipLevels; mipLevel++)
	{
		const int mippedDim = dim * std::pow(0.5, mipLevel);
		const auto imageView = [&]() {
			VkImageView iv;
			VkImageViewCreateInfo imageViewCI{};
			imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCI.image = img->Get();
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
			imageViewCI.format = img->GetFormat();
			imageViewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, mipLevel, 1, 0, 6 };
			check(vkCreateImageView(device_.device, &imageViewCI, nullptr, &iv));
			return iv;
		}();
		cleanupBin.push_back(imageView);

		colorAttachment.imageView = imageView;
		renderInfo.renderArea.extent.width = uint32_t(mippedDim);
		renderInfo.renderArea.extent.height = uint32_t(mippedDim);
		vkCmdBeginRendering(commandBuffer, &renderInfo);

		const VkExtent2D mippedExtent = { uint32_t(mippedDim), uint32_t(mippedDim) };
		VkRect2D scissor{};
		scissor.extent = mippedExtent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		VkViewport viewport = CreateInfo::Viewport(mippedExtent);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterPipeline);

		VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &cubeBuffer->operator const VkBuffer & (), &offset);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, prefilterLayout, 0, 1, &prefilterSet, 0, nullptr);

		float roughness = float(mipLevel) / float(maxMipLevels - 1);
		vkCmdPushConstants(commandBuffer, prefilterLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(float), &roughness);

		vkCmdDraw(commandBuffer, cubeVertices.size(), 1, 0, 0);

		vkCmdEndRendering(commandBuffer);
	}

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	// image + sampler
	img->AttachCubeMapImageView(range);
	VkSamplerCreateInfo samplerCI = CreateInfo::SamplerCI(maxMipLevels, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);
	img->AttachSampler(samplerCI);

	return img;
}

PrefilterCubemap::~PrefilterCubemap()
{
	vkDestroyDescriptorSetLayout(device_.device, prefilterDescLayout, nullptr);
	vkDestroyDescriptorPool(device_.device, prefilterPool, nullptr);
	vkDestroyPipelineLayout(device_.device, prefilterLayout, nullptr);
	vkDestroyPipeline(device_.device, prefilterPipeline, nullptr);

	for (const auto& imageView : cleanupBin)
	{
		vkDestroyImageView(device_.device, imageView, nullptr);
	}
}
