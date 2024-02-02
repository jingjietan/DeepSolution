#include "InfiniteGrid.h"
#include "../Core/Device.h"
#include "../Core/Common.h"
#include "../Core/Shader.h"

#include <volk.h>
#include <array>

InfiniteGrid::InfiniteGrid(Device& device, VkDescriptorSetLayout globalUniformLayout): device_(device)
{
	VkPipelineLayoutCreateInfo pipelineLayoutCI{};
	pipelineLayoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCI.pSetLayouts = &globalUniformLayout;
	pipelineLayoutCI.setLayoutCount = 1;
	check(vkCreatePipelineLayout(device_.device, &pipelineLayoutCI, nullptr, &pipelineLayout_));

	std::array stages = {
		CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, loadShader(device_.device, "Shaders/InfiniteGrid.vert.spv")),
		CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, loadShader(device_.device, "Shaders/InfiniteGrid.frag.spv"))
	};

	auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);
	auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	auto viewportState = CreateInfo::ViewportState();
	auto rasterizationState = CreateInfo::RasterizationState(
		false,
		VK_CULL_MODE_NONE,
		VK_FRONT_FACE_CLOCKWISE
	); 
	auto multisampleState = CreateInfo::MultisampleState();
	auto depthStencilState = CreateInfo::DepthStencilState();

	auto colorAttachment = CreateInfo::ColorBlendAttachment();
	auto colorBlendState = CreateInfo::ColorBlendState(&colorAttachment, 1);

	std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

	VkFormat swapchainFormat = device_.getSurfaceFormat();
	auto rendering = CreateInfo::Rendering(&swapchainFormat, 1, device_.getDepthFormat());

	VkGraphicsPipelineCreateInfo pipelineCI{};
	pipelineCI.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCI.stageCount = static_cast<uint32_t>(stages.size());
	pipelineCI.pStages = stages.data();
	pipelineCI.pVertexInputState = &vertexInputState;
	pipelineCI.pInputAssemblyState = &inputAssemblyState;
	pipelineCI.pTessellationState = nullptr;
	pipelineCI.pViewportState = &viewportState;
	pipelineCI.pRasterizationState = &rasterizationState;
	pipelineCI.pMultisampleState = &multisampleState;
	pipelineCI.pDepthStencilState = &depthStencilState;
	pipelineCI.pColorBlendState = &colorBlendState;
	pipelineCI.pDynamicState = &dynamicState;
	pipelineCI.layout = pipelineLayout_;
	Connect(pipelineCI, rendering);
	
	check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCI, nullptr, &pipeline_));

	vkDestroyShaderModule(device_.device, stages[0].module, nullptr);
	vkDestroyShaderModule(device_.device, stages[1].module, nullptr);
}

void InfiniteGrid::draw(VkCommandBuffer commandBuffer, VkDescriptorSet globalSet, VkImageView color, VkImageView depth, VkExtent2D extent)
{
	VkDebugUtilsLabelEXT label{};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = "Infinite Grid";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = color;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; 
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = depth;
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

	VkViewport viewport{};
	viewport.width = static_cast<float>(extent.width);
	viewport.height = -static_cast<float>(extent.height);
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = extent;
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
	
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSet, 0, nullptr);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

	vkCmdDraw(commandBuffer, 6, 1, 0, 0);

	vkCmdEndRendering(commandBuffer);

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);
}

InfiniteGrid::~InfiniteGrid()
{
	vkDestroyPipeline(device_.device, pipeline_, nullptr);
	vkDestroyPipelineLayout(device_.device, pipelineLayout_, nullptr);
}
