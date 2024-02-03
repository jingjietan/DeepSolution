#include "Skybox.h"
#include "../Core/Buffer.h"
#include "../Core/Device.h"

#include <glm/glm.hpp>
#include "../Core/Shader.h"
#include "../Core/Cube.h"
#include "../Core/Common.h"
#include "../Core/DescriptorWrite.h"
#include <glm/ext/matrix_clip_space.hpp>

Skybox::Skybox(Device& device, const std::shared_ptr<Buffer>& cubeBuffer) : device_(device), cubeBuffer(cubeBuffer)
{
	uniformBuffer = std::make_unique<Buffer>(device_, 2 * sizeof(glm::mat4), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	ShaderReflect reflect;
	reflect.add("Shaders/Skybox.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	reflect.add("Shaders/Skybox.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	skyboxSetLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
	skyboxPool = reflect.retrieveDescriptorPool(device_.device);
	skyboxSet = reflect.retrieveSet(device_.device, skyboxPool, skyboxSetLayout)[0];
	skyboxLayout = reflect.retrievePipelineLayout(device_.device, { skyboxSetLayout });
	auto skyboxStages = reflect.retrieveShaderModule(device_.device);

	skyboxPipeline = [&]() {
		VkPipeline pipeline;
		const auto stages = ShaderReflect::getStages(skyboxStages);

		auto vertexBinding = BasicVertex::BindingDescription();
		auto vertexAttributes = BasicVertex::PositionAttributesDescription();
		auto vertexInputState = CreateInfo::VertexInputState(&vertexBinding, 1, vertexAttributes.data(), vertexAttributes.size());

		auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			false,
			VK_CULL_MODE_FRONT_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

		auto multisampleState = CreateInfo::MultisampleState();

		auto depthStencilState = CreateInfo::DepthStencilState();
		//depthStencilState.depthTestEnable = VK_FALSE;
		//depthStencilState.depthWriteEnable = VK_FALSE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

		auto attachment = CreateInfo::NoBlend();
		auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);

		std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

		VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		auto rendering = CreateInfo::Rendering(&format, 1, device_.getDepthFormat());

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
		pipelineCreateInfo.layout = skyboxLayout;

		Connect(pipelineCreateInfo, rendering);

		check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));

		return pipeline;
	}();

	DescriptorWrite writer;
	writer.add(skyboxSet, 0, 0, BufferType::Uniform, 1, *uniformBuffer, 0, VK_WHOLE_SIZE);
	writer.write(device_.device);

	ShaderReflect::deleteModules(device_.device, skyboxStages);
}

void Skybox::set(VkImageView imageView, VkSampler sampler) const
{
	DescriptorWrite writer;
	writer.add(skyboxSet, 1, 0, ImageType::CombinedSampler, 1, sampler, imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	writer.write(device_.device);
}

void Skybox::render(VkCommandBuffer commandBuffer, const glm::mat4& projection, const glm::mat4& view, VkImageView colorView, VkImageView depthView, VkExtent2D extent)
{
	{
		struct UBO {
			glm::mat4 projection;
			glm::mat4 view;
		} ubo{};
		ubo.projection = projection;
		ubo.view = view;
		uniformBuffer->upload(&ubo, sizeof(ubo));
	}

	VkDebugUtilsLabelEXT label{};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = "Skybox";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = colorView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = depthView;
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.renderArea.extent = extent;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.pDepthAttachment = &depthAttachment;

	vkCmdBeginRendering(commandBuffer, &renderInfo);

	VkViewport viewport = CreateInfo::Viewport(extent);
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxPipeline);

	VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, *cubeBuffer, &offset);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, skyboxLayout, 0, 1, &skyboxSet, 0, nullptr);

	vkCmdDraw(commandBuffer, cubeVertices.size(), 1, 0, 0);

	vkCmdEndRendering(commandBuffer);

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

Skybox::~Skybox()
{
	vkDestroyPipeline(device_.device, skyboxPipeline, nullptr);
	vkDestroyPipelineLayout(device_.device, skyboxLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, skyboxSetLayout, nullptr);
	vkDestroyDescriptorPool(device_.device, skyboxPool, nullptr);
}
