#include "Bloom.h"
#include "../Core/Image.h"
#include "../Core/Device.h"
#include "../Core/Transition.h"
#include "../Core/Shader.h"
#include <array>
#include "../Core/DescriptorWrite.h"
#include "../Core/Framebuffer.h"

Bloom::Bloom(Device& device): device_(device)
{
	{
		ShaderReflect reflect;
		reflect.add("Shaders/BRDF.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		reflect.add("Shaders/BloomDownsample.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		bloomDownsamplePipeline_.descLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
		bloomDownsamplePipeline_.pool = reflect.retrieveDescriptorPool(device_.device, maxDownsamples * device_.getMaxFramesInFlight());
		bloomDownsamplePipeline_.layout = reflect.retrievePipelineLayout(device_.device, { bloomDownsamplePipeline_.descLayout });
		auto bloomStages = reflect.retrieveShaderModule(device_.device);
		bloomDownsamplePipeline_.pipeline = [&]() {
			VkPipeline pipeline;

			const auto stages = ShaderReflect::getStages(bloomStages);

			auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);

			auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			auto viewportState = CreateInfo::ViewportState();

			auto rasterizationState = CreateInfo::RasterizationState(
				false,
				VK_CULL_MODE_BACK_BIT,
				VK_FRONT_FACE_COUNTER_CLOCKWISE
			);

			auto multisampleState = CreateInfo::MultisampleState();

			auto depthStencilState = CreateInfo::NoDepthState();

			auto attachment = CreateInfo::NoBlend();
			auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);

			std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

			VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
			auto rendering = CreateInfo::Rendering(&format, 1, VK_FORMAT_UNDEFINED);

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
			pipelineCreateInfo.layout = bloomDownsamplePipeline_.layout;

			Connect(pipelineCreateInfo, rendering);

			check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
			return pipeline;
		}();

		ShaderReflect::deleteModules(device_.device, bloomStages);

		bloomDownsamplePipeline_.sets = reflect.retrieveSet(device_.device, bloomDownsamplePipeline_.pool, bloomDownsamplePipeline_.descLayout, maxDownsamples);
	}
	{
		ShaderReflect reflect;
		reflect.add("Shaders/BRDF.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		reflect.add("Shaders/BloomUpsample.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		bloomUpsamplePipeline_.descLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
		bloomUpsamplePipeline_.pool = reflect.retrieveDescriptorPool(device_.device, maxDownsamples * device_.getMaxFramesInFlight());
		bloomUpsamplePipeline_.layout = reflect.retrievePipelineLayout(device_.device, { bloomUpsamplePipeline_.descLayout });
		auto bloomStages = reflect.retrieveShaderModule(device_.device);
		bloomUpsamplePipeline_.pipeline = [&]() {
			VkPipeline pipeline;

			const auto stages = ShaderReflect::getStages(bloomStages);

			auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);

			auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			auto viewportState = CreateInfo::ViewportState();

			auto rasterizationState = CreateInfo::RasterizationState(
				false,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE
			);

			auto multisampleState = CreateInfo::MultisampleState();

			auto depthStencilState = CreateInfo::NoDepthState();

			auto attachment = CreateInfo::ColorBlendAttachment();
			attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
			attachment.colorBlendOp = VK_BLEND_OP_ADD;
			attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
			attachment.alphaBlendOp = VK_BLEND_OP_ADD;

			auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);

			std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

			VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
			auto rendering = CreateInfo::Rendering(&format, 1, VK_FORMAT_UNDEFINED);

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
			pipelineCreateInfo.layout = bloomUpsamplePipeline_.layout;

			Connect(pipelineCreateInfo, rendering);

			check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
			return pipeline;
			}();

		ShaderReflect::deleteModules(device_.device, bloomStages);

		bloomUpsamplePipeline_.sets = reflect.retrieveSet(device_.device, bloomUpsamplePipeline_.pool, bloomUpsamplePipeline_.descLayout, maxDownsamples);
	}
	{
		ShaderReflect reflect;
		reflect.add("Shaders/BRDF.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		reflect.add("Shaders/BloomComposite.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

		bloomCompositePipeline_.descLayout = reflect.retrieveDescriptorSetLayout(device_.device)[0];
		bloomCompositePipeline_.pool = reflect.retrieveDescriptorPool(device_.device, maxDownsamples * device_.getMaxFramesInFlight());
		bloomCompositePipeline_.layout = reflect.retrievePipelineLayout(device_.device, { bloomCompositePipeline_.descLayout });
		auto bloomStages = reflect.retrieveShaderModule(device_.device);
		bloomCompositePipeline_.pipeline = [&]() {
			VkPipeline pipeline;

			const auto stages = ShaderReflect::getStages(bloomStages);

			auto vertexInputState = CreateInfo::VertexInputState(nullptr, 0, nullptr, 0);

			auto inputAssemblyState = CreateInfo::InputAssemblyState(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			auto viewportState = CreateInfo::ViewportState();

			auto rasterizationState = CreateInfo::RasterizationState(
				false,
				VK_CULL_MODE_NONE,
				VK_FRONT_FACE_COUNTER_CLOCKWISE
			);

			auto multisampleState = CreateInfo::MultisampleState();

			auto depthStencilState = CreateInfo::NoDepthState();

			auto attachment = CreateInfo::NoBlend();

			auto colorBlendState = CreateInfo::ColorBlendState(&attachment, 1);

			std::array dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
			auto dynamicState = CreateInfo::DynamicState(dynamicStates.data(), dynamicStates.size());

			VkFormat format = device_.getSurfaceFormat();
			auto rendering = CreateInfo::Rendering(&format, 1, VK_FORMAT_UNDEFINED);

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
			pipelineCreateInfo.layout = bloomCompositePipeline_.layout;

			Connect(pipelineCreateInfo, rendering);

			check(vkCreateGraphicsPipelines(device_.device, nullptr, 1, &pipelineCreateInfo, nullptr, &pipeline));
			return pipeline;
		}();

		ShaderReflect::deleteModules(device_.device, bloomStages);

		bloomCompositePipeline_.set = reflect.retrieveSet(device_.device, bloomCompositePipeline_.pool, bloomCompositePipeline_.descLayout)[0];

	}
	
	bloomImageViews_.resize(maxDownsamples);

	VkSamplerCreateInfo samplerCI = CreateInfo::SamplerCI(maxDownsamples, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);
	check(vkCreateSampler(device_.device, &samplerCI, nullptr, &bloomSampler_));
}

void Bloom::doBloom(VkCommandBuffer commandBuffer, VkExtent2D extent, VkImage image, VkImageView colorView, VkImageView swapchainView)
{
	// We start with one miplevel.
	const auto bloomExtent = VkExtent2D{ extent.width / 2, extent.height / 2 };

	if (!bloomImage_ || bloomImage_->getExtent() != bloomExtent)
	{
		for (const auto& imageView : bloomImageViews_)
		{
			bloomImageViewsRecycle_.push_back(std::make_pair(imageView, device_.getMaxFramesInFlight()));
		}

		VkImageCreateInfo imageCI = CreateInfo::Image2DCI(bloomExtent, maxDownsamples, VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
		bloomImage_ = std::make_unique<Image>(device_, imageCI);
		bloomImage_->attachImageView({ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
		VkSamplerCreateInfo samplerCI = CreateInfo::SamplerCI(maxDownsamples, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR, device_.deviceProperties.limits.maxSamplerAnisotropy);
		bloomImage_->attachSampler(samplerCI);

		setName(device_.device, bloomImage_->get(), "Bloom Image");
		for (uint32_t i = 0; i < maxDownsamples; i++)
		{
			VkImageView imageView;
			VkImageViewCreateInfo imageViewCI{};
			imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCI.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			imageViewCI.image = bloomImage_->get();
			imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCI.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 };
			check(vkCreateImageView(device_.device, &imageViewCI, nullptr, &imageView));
			bloomImageViews_[i] = imageView;
		}

		DescriptorWrite writer;
		for (uint32_t i = 0; i < bloomDownsamplePipeline_.sets.size(); i++)
		{
			writer.add(bloomDownsamplePipeline_.sets[i], 0, 0, ImageType::CombinedSampler, 1,
				bloomSampler_, i == 0 ? colorView : bloomImageViews_[i - 1], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);	
		}
		for (uint32_t i = 0; i < maxDownsamples; i++)
		{
			writer.add(bloomUpsamplePipeline_.sets[i], 0, 0, ImageType::CombinedSampler, 1,
				bloomSampler_, bloomImageViews_[maxDownsamples - 1 - i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		}
		writer.add(bloomCompositePipeline_.set, 0, 0, ImageType::CombinedSampler, 1,
			bloomSampler_, colorView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		writer.add(bloomCompositePipeline_.set, 1, 0, ImageType::CombinedSampler, 1,
			bloomImage_->getSampler(), bloomImage_->getView(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		writer.write(device_.device);
		bloomImage_->UndefinedToColorAttachment(commandBuffer);
	}

	const auto extentGivenMiplevel = [](VkExtent2D extent, uint32_t mipLevel) {
		for (uint32_t i = mipLevel; i > 0; i--)
		{
			extent.width /= 2;
			extent.height /= 2;
		}
		return extent;
	};

	// Downsampling
	VkDebugUtilsLabelEXT label{};
	label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	label.pLabelName = "Downsampling";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	Transition::ColorAttachmentToShaderReadOptimal(image, commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

	for (size_t i = 0; i < maxDownsamples; i++)
	{
		// transition i to sampled
		if (i != 0)
			Transition::ColorAttachmentToShaderReadOptimal(bloomImage_->get(), commandBuffer, {VK_IMAGE_ASPECT_COLOR_BIT, uint32_t(i - 1), 1, 0, 1});
		// transition i + 1 to color attachment (already done)
		
		const auto dim = extentGivenMiplevel(bloomExtent, i);
		Framebuffer framebuffer(dim);
		framebuffer.addColorAttachment(bloomImageViews_[i]);
		framebuffer.beginRendering(commandBuffer, {
			FramebufferOption{FramebufferType::Color, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, {}}
		});

		auto viewport = CreateInfo::Viewport(dim);
		VkRect2D scissor{};
		scissor.extent = dim;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdPushConstants(commandBuffer, bloomDownsamplePipeline_.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(dim), &dim);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomDownsamplePipeline_.layout, 0, 1, &bloomDownsamplePipeline_.sets[i], 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomDownsamplePipeline_.pipeline);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		framebuffer.endRendering(commandBuffer);
	}
	// all mips will be in sampled (except last)
	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	// Blur Upsampling
	label.pLabelName = "Upsampling";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	for (uint32_t i = 0; i < maxDownsamples - 1; i++)
	{
		int mipLevelToReadFrom = int(maxDownsamples) - 1 - i;
		int mipLevelToWriteTo = int(maxDownsamples) - 2 - i;

		Transition::ColorAttachmentToShaderReadOptimal(bloomImage_->get(), commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, uint32_t(mipLevelToReadFrom), 1, 0, 1 });
		Transition::ShaderReadOptimalToColorAttachment(bloomImage_->get(), commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, uint32_t(mipLevelToWriteTo), 1, 0, 1 });
		
		const auto dim = mipLevelToWriteTo == -1 ? extent : extentGivenMiplevel(bloomExtent, mipLevelToWriteTo);
		Framebuffer framebuffer(dim);
		framebuffer.addColorAttachment(mipLevelToWriteTo == -1 ? colorView : bloomImageViews_[mipLevelToWriteTo]);
		framebuffer.beginRendering(commandBuffer, {
			FramebufferOption{FramebufferType::Color, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, {}}
		});

		auto viewport = CreateInfo::Viewport(dim);
		VkRect2D scissor{};
		scissor.extent = dim;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		float radius = 0.005f;
		vkCmdPushConstants(commandBuffer, bloomUpsamplePipeline_.layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(radius), &radius);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUpsamplePipeline_.layout, 0, 1, &bloomUpsamplePipeline_.sets[i], 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomUpsamplePipeline_.pipeline);
		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		framebuffer.endRendering(commandBuffer);
	}
	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	label.pLabelName = "Compositing";
	vkCmdBeginDebugUtilsLabelEXT(commandBuffer, &label);

	Transition::ColorAttachmentToShaderReadOptimal(bloomImage_->get(), commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	Framebuffer framebuffer(extent);
	framebuffer.addColorAttachment(swapchainView);
	framebuffer.beginRendering(commandBuffer, {
		FramebufferOption{FramebufferType::Color, VK_ATTACHMENT_LOAD_OP_DONT_CARE, VK_ATTACHMENT_STORE_OP_STORE, {}}
	});

	auto viewport = CreateInfo::Viewport(extent);
	VkRect2D scissor{};
	scissor.extent = extent;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomCompositePipeline_.layout, 0, 1, &bloomCompositePipeline_.set, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, bloomCompositePipeline_.pipeline);
	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	framebuffer.endRendering(commandBuffer);

	vkCmdEndDebugUtilsLabelEXT(commandBuffer);

	Transition::ShaderReadOptimalToColorAttachment(image, commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	Transition::ShaderReadOptimalToColorAttachment(bloomImage_->get(), commandBuffer, { VK_IMAGE_ASPECT_COLOR_BIT, 0, maxDownsamples, 0, 1 });
	// all mips will be color attachment

	cleanup();
}

Bloom::~Bloom()
{
	for (const auto& imageView : bloomImageViews_)
	{
		vkDestroyImageView(device_.device, imageView, nullptr);
	}
	vkDestroySampler(device_.device, bloomSampler_, nullptr);
	vkDestroyDescriptorPool(device_.device, bloomDownsamplePipeline_.pool, nullptr);
	vkDestroyDescriptorPool(device_.device, bloomUpsamplePipeline_.pool, nullptr);
	vkDestroyDescriptorPool(device_.device, bloomCompositePipeline_.pool, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, bloomDownsamplePipeline_.descLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, bloomUpsamplePipeline_.descLayout, nullptr);
	vkDestroyDescriptorSetLayout(device_.device, bloomCompositePipeline_.descLayout, nullptr);
	vkDestroyPipeline(device_.device, bloomDownsamplePipeline_.pipeline, nullptr);
	vkDestroyPipeline(device_.device, bloomUpsamplePipeline_.pipeline, nullptr);
	vkDestroyPipeline(device_.device, bloomCompositePipeline_.pipeline, nullptr);
	vkDestroyPipelineLayout(device_.device, bloomDownsamplePipeline_.layout, nullptr);
	vkDestroyPipelineLayout(device_.device, bloomUpsamplePipeline_.layout, nullptr);
	vkDestroyPipelineLayout(device_.device, bloomCompositePipeline_.layout, nullptr);
}

void Bloom::cleanup()
{
	for (auto& p : bloomImageViewsRecycle_)
	{
		p.second -= 1;
		if (p.second <= 0)
		{
			vkDestroyImageView(device_.device, p.first, nullptr);
		}
	}
	std::erase_if(bloomImageViewsRecycle_, [](auto& p) {
		return p.second <= 0;
	});
}
