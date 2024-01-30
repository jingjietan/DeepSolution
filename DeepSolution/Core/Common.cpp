#include "Common.h"

#include "Device.h"
#include <spdlog/spdlog.h>

void check(bool condition, const std::string& message, const std::source_location& location)
{
	if (!condition)
	{
		SPDLOG_ERROR("Error at {} {}. {}", location.file_name(), location.line(), message);
		std::terminate();
	}
}

#include <volk.h>

VkFence CreateInfo::createFence(VkDevice device, VkFenceCreateFlags flags)
{
	VkFence fence;
	VkFenceCreateInfo fenceCI{};
	fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCI.flags = flags;
	check(vkCreateFence(device, &fenceCI, nullptr, &fence));
	return fence;
}

VkSemaphore CreateInfo::createBinarySemaphore(VkDevice device)
{
	VkSemaphore semaphore;
	VkSemaphoreCreateInfo semaphoreCI{};
	semaphoreCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	check(vkCreateSemaphore(device, &semaphoreCI, nullptr, &semaphore));
	return semaphore;
}

VkCommandBuffer CreateInfo::allocateCommandBuffer(VkDevice device, VkCommandPool commandPool)
{
	VkCommandBuffer commandBuffer;
	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandBufferCount = 1;
	allocateInfo.commandPool = commandPool;
	check(vkAllocateCommandBuffers(device, &allocateInfo, &commandBuffer));
	return commandBuffer;
}

VkCommandPool CreateInfo::createCommandPool(VkDevice device, uint32_t queueFamily, uint32_t flags)
{
	VkCommandPool commandPool;
	VkCommandPoolCreateInfo commandPoolCI{};
	commandPoolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCI.queueFamilyIndex = queueFamily;
	commandPoolCI.flags = flags;
	check(vkCreateCommandPool(device, &commandPoolCI, nullptr, &commandPool));
	return commandPool;
}

void CreateInfo::performOneTimeAction(VkDevice device, VkQueue queue, VkCommandPool commandPool, const std::function<void(VkCommandBuffer)>& action)
{
	VkCommandBuffer commandBuffer = allocateCommandBuffer(device, commandPool);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	action(commandBuffer);

	vkEndCommandBuffer(commandBuffer);
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	check(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	check(vkQueueWaitIdle(queue)); // device lost error is so weird.
}

VkRenderPass CreateInfo::createRenderPass(VkDevice device, const VkAttachmentDescription2* pAttachments, uint32_t attachmentCount, const VkSubpassDescription2* pSubpass, uint32_t subpassCount, const VkSubpassDependency2* pDependency, uint32_t dependencyCount)
{
	VkRenderPass renderPass;
	VkRenderPassCreateInfo2 renderPassCI{};
	renderPassCI.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
	renderPassCI.pAttachments = pAttachments;
	renderPassCI.attachmentCount = attachmentCount;
	renderPassCI.pSubpasses = pSubpass;
	renderPassCI.subpassCount = subpassCount;
	renderPassCI.pDependencies = pDependency;
	renderPassCI.dependencyCount = dependencyCount;
	check(vkCreateRenderPass2(device, &renderPassCI, nullptr, &renderPass));
	return renderPass;
}

VkViewport CreateInfo::Viewport(VkExtent2D extent)
{
	VkViewport viewport{};
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(extent.height);
	viewport.width = static_cast<float>(extent.width);
	viewport.height = -static_cast<float>(extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	return viewport;
}

VkPipelineShaderStageCreateInfo CreateInfo::ShaderStage(VkShaderStageFlagBits stage, VkShaderModule module)
{
	VkPipelineShaderStageCreateInfo shaderStage{};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = module;
	shaderStage.pName = "main";
	return shaderStage;
}

VkPipelineVertexInputStateCreateInfo CreateInfo::VertexInputState(VkVertexInputBindingDescription* pBinding, size_t bindingCount, VkVertexInputAttributeDescription* pAttribute, size_t attributeCount)
{
	VkPipelineVertexInputStateCreateInfo vertexInputState{};
	vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputState.pVertexBindingDescriptions = pBinding;
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingCount);
	vertexInputState.pVertexAttributeDescriptions = pAttribute;
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeCount);
	return vertexInputState;
}

VkPipelineInputAssemblyStateCreateInfo CreateInfo::InputAssemblyState(VkPrimitiveTopology topology)
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = topology;
	inputAssemblyState.primitiveRestartEnable = VK_FALSE;
	return inputAssemblyState;
}

VkPipelineViewportStateCreateInfo CreateInfo::ViewportState()
{
	VkPipelineViewportStateCreateInfo viewportState{};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.scissorCount = 1;
	viewportState.viewportCount = 1;
	return viewportState;
}

VkPipelineRasterizationStateCreateInfo CreateInfo::RasterizationState(VkBool32 depthClamp, VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	VkPipelineRasterizationStateCreateInfo rasterizationState{};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.depthClampEnable = depthClamp;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.lineWidth = 1.0f;
	rasterizationState.cullMode = cullMode;
	rasterizationState.frontFace = frontFace;
	rasterizationState.depthBiasEnable = VK_FALSE;
	return rasterizationState;
}

VkPipelineMultisampleStateCreateInfo CreateInfo::MultisampleState()
{
	VkPipelineMultisampleStateCreateInfo  multisampleState{};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	return multisampleState;
}

VkPipelineDepthStencilStateCreateInfo CreateInfo::DepthStencilState()
{
	VkPipelineDepthStencilStateCreateInfo depthStencilState{};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
	depthStencilState.minDepthBounds = 0.f;
	depthStencilState.maxDepthBounds = 1.f;
	return depthStencilState;
}

VkPipelineColorBlendAttachmentState CreateInfo::NoBlend()
{
	VkPipelineColorBlendAttachmentState attachment{};
	attachment.blendEnable = VK_FALSE;
	attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	return attachment;
}

VkPipelineColorBlendAttachmentState CreateInfo::ColorBlendAttachment()
{
	VkPipelineColorBlendAttachmentState attachment{}; // Normal Color Blending
	attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	attachment.blendEnable = VK_TRUE;
	attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.colorBlendOp = VK_BLEND_OP_ADD;
	attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	attachment.alphaBlendOp = VK_BLEND_OP_ADD;
	return attachment;
}

VkPipelineColorBlendStateCreateInfo CreateInfo::ColorBlendState(VkPipelineColorBlendAttachmentState* pAttachment, size_t attachmentCount)
{
	VkPipelineColorBlendStateCreateInfo colorBlendState{};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.pAttachments = pAttachment;
	colorBlendState.attachmentCount = static_cast<uint32_t>(attachmentCount);
	return colorBlendState;
}

VkPipelineDynamicStateCreateInfo CreateInfo::DynamicState(VkDynamicState* pDynamicState, size_t dynamicStateCount)
{
	VkPipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = pDynamicState;
	dynamicState.dynamicStateCount = dynamicStateCount;
	return dynamicState;
}

VkPipelineRenderingCreateInfo CreateInfo::Rendering(VkFormat* colorFormats, size_t colorFormatCount, VkFormat depthFormat)
{
	VkPipelineRenderingCreateInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	renderingInfo.colorAttachmentCount = static_cast<uint32_t>(colorFormatCount);
	renderingInfo.pColorAttachmentFormats = colorFormats;
	renderingInfo.depthAttachmentFormat = depthFormat;
	return renderingInfo;
}

VkImageCreateInfo CreateInfo::Image2DCI(VkExtent2D extent, uint32_t mipLevels, VkFormat format, VkImageUsageFlags usage)
{
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCI.arrayLayers = 1;
	imageCI.extent = { extent.width, extent.height, 1 };
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.mipLevels = mipLevels;
	imageCI.format = format;
	imageCI.usage = usage;
	return imageCI;
}

VkSamplerCreateInfo CreateInfo::SamplerCI(uint32_t mipLevels, VkSamplerAddressMode addressMode, VkFilter filterMode, VkSamplerMipmapMode mipmapMode, float maxAnisotropy)
{
	VkSamplerCreateInfo samplerInfo{};
	samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerInfo.addressModeU = addressMode;
	samplerInfo.addressModeV = addressMode;
	samplerInfo.addressModeW = addressMode;
	samplerInfo.minFilter = filterMode;
	samplerInfo.magFilter = filterMode;
	samplerInfo.mipmapMode = mipmapMode;
	samplerInfo.maxLod = static_cast<float>(mipLevels);
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = maxAnisotropy;
	return samplerInfo;
}

// ----

void Detail::setName(Device& device, VkObjectType type, void* ptr, const std::string& name)
{
#ifndef NDEBUG
	VkDebugUtilsObjectNameInfoEXT nameInfo{};
	nameInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	nameInfo.objectType = type;
	nameInfo.objectHandle = reinterpret_cast<uint64_t>(ptr);
	nameInfo.pObjectName = name.c_str();
	check(vkSetDebugUtilsObjectNameEXT(device.device, &nameInfo));
#endif // !NDEBUG
}
