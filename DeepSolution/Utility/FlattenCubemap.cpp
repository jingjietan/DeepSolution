#include "FlattenCubemap.h"

#include "../Core/Common.h"
#include "../Core/Shader.h"
#include "../Core/Device.h"
#include "../Core/Image.h"
#include "../Core/Transition.h"
#include "../Core/Buffer.h"

#include <array>
#include <glm/glm.hpp>
#include "../Core/DescriptorWrite.h"
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>

namespace {
	struct BasicVertex {
		glm::vec3 position;
		glm::vec3 normal;
		glm::vec2 uv;

		VkVertexInputBindingDescription static BindingDescription()
		{
			VkVertexInputBindingDescription bd{};
			bd.binding = 0;
			bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
			bd.stride = sizeof(BasicVertex);
			return bd;
		}
		std::array<VkVertexInputAttributeDescription, 3> static AttributesDescription()
		{
			std::array<VkVertexInputAttributeDescription, 3> ats{};
			ats[0].binding = 0;
			ats[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			ats[0].location = 0;
			ats[0].offset = offsetof(BasicVertex, position);
			ats[1].binding = 0;
			ats[1].format = VK_FORMAT_R32G32B32_SFLOAT;
			ats[1].location = 1;
			ats[1].offset = offsetof(BasicVertex, normal);
			ats[2].binding = 0;
			ats[2].format = VK_FORMAT_R32G32_SFLOAT;
			ats[2].location = 2;
			return ats;
		}
		std::array<VkVertexInputAttributeDescription, 1> static PositionAttributesDescription()
		{
			std::array<VkVertexInputAttributeDescription, 1> ats{};
			ats[0].binding = 0;
			ats[0].format = VK_FORMAT_R32G32B32_SFLOAT;
			ats[0].location = 0;
			ats[0].offset = offsetof(BasicVertex, position);
			return ats;
		}
	};

	std::vector<BasicVertex> cubeVertices = {
			{{ -1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
			{{1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}}, // bottom-right         
			{{1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
			{{ -1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{ -1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}}, // top-left
			// front face
			{{-1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}}, // bottom-left
			{{1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}}, // bottom-right
			{{1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}}, // top-right
			{{1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}}, // top-right
			{{-1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}}, // top-left
			{{-1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}}, // bottom-left
			// left face
			
			{{-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-right
			{{-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // top-left
			{{-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-left
			{{-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-left
			{{-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // bottom-right
			{{-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-right
								 								  
			// // right face
			{{1.0f,  1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-left
			{{1.0f, -1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-right
			{{1.0f,  1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // top-right         
			{{1.0f, -1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-right
			{{1.0f,  1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-left
			{{1.0f, -1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // bottom-left     
			// bottom face								  
			{{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-right
			{{1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 1.0f}}, // top-left
			{{1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-left
			{{1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-left
			{{-1.0f, -1.0f,  1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 0.0f}}, // bottom-right
			{{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-right
			// top face												  
			{{-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-left
			{{1.0f,  1.0f , 1.0f }, { 0.0f,  1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-right
			{{ 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 1.0f, 1.0f}}, // top-right     
			{{1.0f,  1.0f,  1.0f }, { 0.0f,  1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-right
			{{-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-left
			{{-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 0.0f}}  // bottom-left   
	};
}

FlattenCubemap::FlattenCubemap(Device& device): device_(device)
{
	const auto quadSize = sizeof(BasicVertex) * cubeVertices.size();
	quadBuffer = std::make_unique<Buffer>(device_, quadSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	quadBuffer->upload(cubeVertices.data(), quadSize);

	uniformBuffer = std::make_unique<Buffer>(device_, 7 * sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	
	ShaderReflect reflect;
	reflect.add("Shaders/FlattenCubemap.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	reflect.add("Shaders/FlattenCubemap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	conversionDescLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
	conversionPool = reflect.retrieveDescriptorPool(device_.device);
	conversionSet = reflect.retrieveSet(device_.device, conversionPool, conversionDescLayout)[0];
	conversionLayout = reflect.retrievePipelineLayout(device_.device, { conversionDescLayout });
	auto conversionStages = reflect.retrieveShaderModule(device_.device);

	conversionPipeline = [&]() {
		VkPipeline pipeline;
		const auto stages = ShaderReflect::getStages(conversionStages);

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
		pipelineCreateInfo.layout = conversionLayout;

		Connect(pipelineCreateInfo, rendering);

		check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));

		return pipeline;
	}();

	DescriptorWrite writer;
	writer.add(conversionSet, 0, 0, BufferType::Uniform, 1, uniformBuffer->operator const VkBuffer & (), 0, VK_WHOLE_SIZE);
	writer.write(device_.device);

	const auto projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.f);
	uniformBuffer->upload(&projection, sizeof(projection));
	std::array views{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
	uniformBuffer->upload(views.data(), views.size() * sizeof(glm::mat4), sizeof(glm::mat4));

	ShaderReflect::deleteModules(device_.device, conversionStages);
}

std::unique_ptr<Image> FlattenCubemap::convert(VkCommandBuffer commandBuffer, VkImageView imageView, VkSampler sampler, int width, int height)
{
	VkExtent2D extent = { uint32_t(width), uint32_t(height) };
	VkImageSubresourceRange range = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 6 };
	VkImageCreateInfo imageCI = CreateInfo::Image2DCI(extent, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
	imageCI.arrayLayers = 6; //for cubemap
	imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
	auto img = std::make_unique<Image>(device_, imageCI);
	img->AttachCubeMapImageView(range);

	// transition...
	Transition::UndefinedToColorAttachment(img->Get(), commandBuffer, range);

	// write to descriptor
	DescriptorWrite writer;
	writer.add(conversionSet, 1, 0, ImageType::CombinedSampler, 1, sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.write(device_.device);

	VkDebugUtilsLabelEXT label{};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = "Flatten";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = img->GetView();
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue = { 0.f, 0.f, 0.f, 1.f };

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.renderArea.extent = extent;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.viewMask = 0b111111; // multiview 6 times.

	vkCmdBeginRendering(commandBuffer, &renderInfo);

	VkViewport viewport = CreateInfo::Viewport(extent);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, conversionPipeline);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &quadBuffer->operator const VkBuffer & (), &offset);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, conversionLayout, 0, 1, &conversionSet, 0, nullptr);

	vkCmdDraw(commandBuffer, cubeVertices.size(), 1, 0, 0);

	vkCmdEndRendering(commandBuffer);

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	Transition::ColorAttachmentToShaderReadOptimal(img->Get(), commandBuffer);

	// image + sampler
	VkSamplerCreateInfo samplerCI = CreateInfo::SamplerCI(1, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);
	img->AttachSampler(samplerCI);

	return img;
}

FlattenCubemap::~FlattenCubemap()
{
	vkDestroyPipelineLayout(device_.device, conversionLayout, nullptr);
	vkDestroyPipeline(device_.device, conversionPipeline, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, conversionDescLayout, nullptr);
	vkDestroyDescriptorPool(device_.device, conversionPool, nullptr);

}
