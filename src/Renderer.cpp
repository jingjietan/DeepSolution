#include "Renderer.h"
#include "Core/Device.h"
#include "Core/Shader.h"
#include "Core/Common.h"
#include "Core/Cube.h"
#include "Core/Buffer.h"
#include "Core/Image.h"
#include "Scene.h"

#include <array>
#include <volk.h>
#include <fmt/format.h>
#include "Core/DescriptorWrite.h"
#include "Core/Transition.h"
#include "Core/Framebuffer.h"

Renderer::Renderer(Device& device, Scene& scene): device_(device), scene_(scene), maxFramesInFlight(device_.getMaxFramesInFlight())
{
	vertexShader_ = loadShader(device_.device, "Shaders/PBR.vert.spv");
	fragmentShader_ = loadShader(device_.device, "Shaders/PBR.frag.spv");

	constexpr size_t meshDrawDataBufferSize = 2048ull * sizeof(IndirectDrawParam);
	constexpr size_t lightBufferSize = 128ull * sizeof(Light) + sizeof(LightUpload); 
	constexpr size_t indirectBufferSize = 2048ull * sizeof(VkDrawIndexedIndirectCommand);

	indirectBuffer = std::make_unique<Buffer>(device_, indirectBufferSize,
		VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

	// Global Descriptor Set (Per Frame, Global)
	DescriptorCreator creator{};
	creator.add("Global Set", 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT);
	creator.add("Global Set", 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
	creator.add("Global Set", 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	globalSetLayout = creator.createLayout("Global Set", device_.device);

	// Bindless Set (Global)
	creator.add("Bindless Set", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4, VK_SHADER_STAGE_FRAGMENT_BIT, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT);
	creator.add("Bindless Set", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10000, VK_SHADER_STAGE_FRAGMENT_BIT, 
		VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT_EXT |
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT_EXT |
		VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT_EXT);
	bindlessSetLayout = creator.createLayout("Bindless Set", device_.device, VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT);

	// IBR set
	creator.add("IBR Set", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	creator.add("IBR Set", 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	creator.add("IBR Set", 2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	ibrSetLayout = creator.createLayout("IBR Set", device_.device);
	
	// Layout
	VkPushConstantRange range{};
	range.size = sizeof(uint32_t);
	range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	pipelineLayout_ = creator.createPipelineLayout(device_.device, std::array{ globalSetLayout, bindlessSetLayout, ibrSetLayout }, &range);

	// Rendering techniques
	

	// Pool
	globalPool_ = creator.createPool("Global Set", device_.device, maxFramesInFlight);
	bindlessPool = creator.createPool("Bindless Set", device_.device, 1, VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT);
	ibrPool = creator.createPool("IBR Set", device_.device, 1);

	// Sets
	globalSets_.resize(maxFramesInFlight);
	creator.allocateSets("Global Set", device_.device, maxFramesInFlight, globalSets_.data());
	creator.allocateVariableSets("Bindless Set", device_.device, 1, &bindlessSet_, 2048);
	creator.allocateSets("IBR Set", device_.device, 1, &ibrSet);

	// Buffers
	globalUniformBuffers_.resize(maxFramesInFlight);
	perMeshDrawDataBuffer.resize(maxFramesInFlight);
	lightBuffer.resize(maxFramesInFlight);

	for (size_t i = 0; i < maxFramesInFlight; i++)
	{
		globalUniformBuffers_[i] = std::make_unique<Buffer>(device, sizeof(GlobalUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		perMeshDrawDataBuffer[i] = std::make_unique<Buffer>(device_, meshDrawDataBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
		lightBuffer[i] = std::make_unique<Buffer>(device_, lightBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
	}

	// Write
	DescriptorWrite writer;
	for (size_t i = 0; i < maxFramesInFlight; i++)
	{
		writer.add(globalSets_[i], 0, 0, BufferType::Uniform, 1, *globalUniformBuffers_[i], 0, VK_WHOLE_SIZE);
		writer.add(globalSets_[i], 1, 0, BufferType::Storage, 1, *perMeshDrawDataBuffer[i], 0, VK_WHOLE_SIZE);
		writer.add(globalSets_[i], 2, 0, BufferType::Storage, 1, *lightBuffer[i], 0, VK_WHOLE_SIZE);
	}
	writer.write(device_.device);

	// HDR
	DescriptorCreator hdrCreator;
	hdrCreator.add("HDR To Image", 0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
	hdrPipeline_.setLayouts[0] = hdrCreator.createLayout("HDR To Image", device_.device);
	hdrPipeline_.pool = hdrCreator.createPool("HDR To Image", device_.device);
	hdrCreator.allocateSets("HDR To Image", device_.device, 1, &hdrSet);

	hdrPipeline_.layout = hdrCreator.createPipelineLayout(device_.device, hdrPipeline_.setLayouts);
	hdrPipeline_.pipeline = [&]() {
		VkShaderModule vertexShader = loadShader(device_.device, "Shaders/BRDF.vert.spv");
		VkShaderModule fragmentShader = loadShader(device_.device, "Shaders/Blit.frag.spv");

		VkPipeline pipeline;
		std::array stages = {
			CreateInfo::ShaderStage(VK_SHADER_STAGE_VERTEX_BIT, vertexShader),
			CreateInfo::ShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
		};
		auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);

		auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

		auto viewportState = CreateInfo::ViewportState();

		auto rasterizationState = CreateInfo::RasterizationState(
			VK_FALSE,
			VK_CULL_MODE_BACK_BIT,
			VK_FRONT_FACE_COUNTER_CLOCKWISE
		);

		auto multisampleState = CreateInfo::MultisampleState();

		auto depthStencilState = CreateInfo::NoDepthState();

		auto colorAttachment = CreateInfo::NoBlend();
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
		pipelineCreateInfo.layout = hdrPipeline_.layout;

		Connect(pipelineCreateInfo, rendering);

		check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));

		vkDestroyShaderModule(device_.device, vertexShader, nullptr);
		vkDestroyShaderModule(device_.device, fragmentShader, nullptr);
		return pipeline;
	}();

}

void Renderer::draw(VkCommandBuffer commandBuffer, VkImageView colorView, VkImageView depthView, const Scene::Drawbles& renderItems, const State& state)
{
	// HDR Pipeline
	const VkExtent2D extent = { uint32_t(state.camera_->viewportWidth), uint32_t(state.camera_->viewportHeight) };
	if (!hdrImage_ || extent != hdrImage_->getExtent())
	{
		hdrBin_.push_back(std::make_pair(std::move(hdrImage_), maxFramesInFlight));
		VkImageCreateInfo imageCI = CreateInfo::Image2DCI(extent, 1, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);
		hdrImage_ = std::make_unique<Image>(device_, imageCI);
		hdrImage_->attachImageView(hdrImage_->getFullRange());
		VkSamplerCreateInfo samplerCI = CreateInfo::SamplerCI(1, 
			VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 
			VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);

		hdrImage_->attachSampler(samplerCI);
		hdrImage_->UndefinedToColorAttachment(commandBuffer);

		DescriptorWrite writer;
		writer.add(hdrSet, 0, 0, ImageType::CombinedSampler, 1, hdrImage_->getSampler(), hdrImage_->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		writer.write(device_.device);
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

	if (!skybox_)
	{
		skybox_ = std::make_unique<Skybox>(device_, scene_.getCubeBuffer());
		skybox_->set(scene_.getCubeMap().getView(), scene_.getCubeMap().getSampler());
	}
	

	const auto& groups = std::get<Scene::PipelineGroups>(renderItems);
	const auto& indirectParams = std::get<Scene::DrawParams>(renderItems);
	const auto& commands = std::get<Scene::DrawCommands>(renderItems);

	indirectBuffer->upload(commands.data(), commands.size() * sizeof(VkDrawIndexedIndirectCommand));
	perMeshDrawDataBuffer[frameCount_]->upload(indirectParams.data(), indirectParams.size() * sizeof(IndirectDrawParam));

	// Begin Rendering (Opaque)
	{
		VkDebugUtilsLabelEXT label{};
		label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
		label.pLabelName = "Opaque";
		vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

		Framebuffer framebuffer(extent);
		framebuffer.addColorAttachment(hdrImage_->getView());
		framebuffer.addDepthAttachment(depthView);
		framebuffer.beginRendering(commandBuffer, {
			{FramebufferType::Color, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 0.f, 0.f, 0.f, 1.f }},
			{FramebufferType::Depth, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, { 1.0f, 0 }}
		});

		VkViewport viewport = CreateInfo::Viewport(extent);
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent = extent;
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		size_t offset = 0;
		VkBuffer vertexBuffer = scene_.getVertexBuffer();
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, &offset);
		vkCmdBindIndexBuffer(commandBuffer, scene_.getIndexBuffer(), 0, VK_INDEX_TYPE_UINT32);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 0, 1, &globalSets_[frameCount_], 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 1, 1, &bindlessSet_, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout_, 2, 1, &ibrSet, 0, nullptr);

		for (const auto& group : groups)
		{
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, group.first);
			vkCmdPushConstants(commandBuffer, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &group.second.offset);
			vkCmdDrawIndexedIndirect(commandBuffer, *indirectBuffer, sizeof(VkDrawIndexedIndirectCommand) * group.second.offset, group.second.count, sizeof(VkDrawIndexedIndirectCommand));
		}

		framebuffer.endRendering(commandBuffer);

		vkCmdEndDebugUtilsLabelEXT(commandBuffer);
	}

	hdrImage_->ColorAttachmentToShaderReadOptimal(commandBuffer);

	{ // Draw to swapchain image
		Framebuffer framebuffer(extent);
		framebuffer.addColorAttachment(colorView);
		framebuffer.beginRendering(commandBuffer, {
			{FramebufferType::Color, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, {}} //don't care as we are drawing over all pixels
		});

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdrPipeline_.layout, 0, 1, &hdrSet, 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, hdrPipeline_.pipeline);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);
		framebuffer.endRendering(commandBuffer);
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
	*/

	// infiniteGrid_->draw(commandBuffer, globalSets_[frameCount_], colorView, depthView, extent);

	skybox_->render(commandBuffer, state.camera_->calculateProjection(), state.camera_->calculateView(), colorView, depthView, extent);

	hdrImage_->ShaderReadOptimalToColorAttachment(commandBuffer);

	//Transition::ShaderReadOptimalToColorAttachment(accum->get(), commandBuffer);
	//Transition::ShaderReadOptimalToColorAttachment(reveal->get(), commandBuffer);

	cleanupFrameDependentItems();

	frameCount_ = (frameCount_ + 1) % maxFramesInFlight;
}

VkShaderModule Renderer::getVertexModule() const
{
	return vertexShader_;
}

VkShaderModule Renderer::getFragmentModule() const
{
	return fragmentShader_;
}

VkDescriptorSet Renderer::getImageSet() const
{
	return bindlessSet_;
}

VkDescriptorSet Renderer::getIBRSet() const
{
	return ibrSet;
}

VkPipelineLayout Renderer::getPipelineLayout() const
{
	return pipelineLayout_;
}

void Renderer::cleanupFrameDependentItems()
{
	for (auto& frame : hdrBin_)
	{
		frame.second -= 1;
	}
	std::erase_if(hdrBin_, [](const std::pair<std::unique_ptr<Image>, int>& imagePair) {
		return imagePair.second <= 0;
	});
}

Renderer::~Renderer()
{
	hdrPipeline_.clear(device_.device);

	vkDestroyShaderModule(device_.device, vertexShader_, nullptr);
	vkDestroyShaderModule(device_.device, fragmentShader_, nullptr);

	vkDestroyPipelineLayout(device_.device, pipelineLayout_, nullptr);

	vkDestroyDescriptorPool(device_.device, globalPool_, nullptr);
	vkDestroyDescriptorPool(device_.device, bindlessPool, nullptr); 
	vkDestroyDescriptorPool(device_.device, ibrPool, nullptr);

	vkDestroyDescriptorSetLayout(device_.device, globalSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, bindlessSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, ibrSetLayout, nullptr);
}
