#pragma once

#include <string>
#include <source_location>
#include <functional>

#include <vulkan/vulkan.h>
#include <volk.h>

// Check

void check(bool condition, const std::string& message = "", const std::source_location& location = std::source_location::current());

inline void check(VkResult result, const std::string& message = "", const std::source_location& location = std::source_location::current())
{
	return check(result == VK_SUCCESS, message, location);
}

template<typename T> requires std::is_pointer_v<T>
void check(T ptr, const std::string& message = "", const std::source_location& location = std::source_location::current())
{
	check(ptr != nullptr, message, location);
}

void check(auto condition, const std::string& message = "", const std::source_location& location = std::source_location::current())
{
	check(static_cast<bool>(condition), message, location);
}

//

template<class T>
struct AsyncTransfer
{
	VkCommandBuffer commandBuffer = nullptr;
	uint32_t frameCount = 0;
	VkSemaphore semaphore = nullptr;
	VkFence fence = nullptr;
	T discardResource;
};

namespace CreateInfo {
	VkFence createFence(VkDevice device, VkFenceCreateFlags flags);
	VkSemaphore createBinarySemaphore(VkDevice device);
	VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool);

	VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamily, uint32_t flags = 0);
	void performOneTimeAction(VkDevice device, VkQueue queue, VkCommandPool commandPool, const std::function<void(VkCommandBuffer)>& action);

	VkRenderPass createRenderPass(VkDevice device, const VkAttachmentDescription2* pAttachments, uint32_t attachmentCount, const VkSubpassDescription2* pSubpass, uint32_t subpassCount, const VkSubpassDependency2* pDependency, uint32_t dependencyCount);

	VkPipelineShaderStageCreateInfo ShaderStage(VkShaderStageFlagBits stage, VkShaderModule module);
	VkPipelineVertexInputStateCreateInfo VertexInputState(VkVertexInputBindingDescription* pBinding, size_t bindingCount, VkVertexInputAttributeDescription* pAttribute, size_t attributeCount);
	VkPipelineInputAssemblyStateCreateInfo InputAssemblyState(VkPrimitiveTopology topology);
	VkPipelineViewportStateCreateInfo ViewportState();
	VkPipelineRasterizationStateCreateInfo RasterizationState(VkBool32 depthClamp, VkCullModeFlags cullMode, VkFrontFace frontFace);
	VkPipelineMultisampleStateCreateInfo MultisampleState();
	VkPipelineDepthStencilStateCreateInfo DepthStencilState();
	VkPipelineColorBlendAttachmentState ColorBlendAttachment();
	VkPipelineColorBlendStateCreateInfo ColorBlendState(VkPipelineColorBlendAttachmentState* pAttachment, size_t attachmentCount);
	VkPipelineDynamicStateCreateInfo DynamicState(VkDynamicState* pDynamicState, size_t dynamicStateCount);
	VkPipelineRenderingCreateInfo Rendering(VkFormat* colorFormats, size_t colorFormatCount, VkFormat depthFormat);

	template<class T>
	AsyncTransfer<T> performAsyncAction(VkDevice device, VkQueue queue, VkCommandPool commandPool, const std::function<T(VkCommandBuffer)>& action)
	{
		AsyncTransfer<T> async{};
		async.commandBuffer = allocateCommandBuffer(device, commandPool);
		async.semaphore = createBinarySemaphore(device);
		async.fence = createFence(device, 0);

		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		vkBeginCommandBuffer(async.commandBuffer, &beginInfo);

		async.discardResource = action(async.commandBuffer);

		vkEndCommandBuffer(async.commandBuffer);
		VkSubmitInfo submitInfo{};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &async.commandBuffer;
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &async.semaphore;

		check(vkQueueSubmit(queue, 1, &submitInfo, async.fence));
		return async;
	}

	template<class T>
	void cleanupAsync(VkDevice device, VkCommandPool commandPool, uint32_t framesToWait, AsyncTransfer<T>& transfer)
	{
		if (transfer.fence && vkGetFenceStatus(device, transfer.fence) == VK_SUCCESS)
		{
			vkFreeCommandBuffers(device, commandPool, 1, &transfer.commandBuffer);
			vkDestroyFence(device, transfer.fence, nullptr);
			transfer.fence = nullptr;
		}

		if (transfer.semaphore && transfer.frameCount >= framesToWait)
		{
			transfer.discardResource.reset();
			vkDestroySemaphore(device, transfer.semaphore, nullptr);
			transfer.semaphore = nullptr;
		}
		transfer.frameCount++;
	}
}

// Connecting pNext
template<typename A, typename B>
void Connect(A& from, B& to)
{
	const void* temp = from.pNext;
	from.pNext = &to;
	to.pNext = const_cast<void*>(temp);
}

template<typename A, typename ...Args>
void Connect(A& from, Args& ...args)
{
	(Connect(from, args), ...);
}


// Set debug
class Device;

namespace Detail {
	void setName(Device& device, VkObjectType type, void* ptr, const std::string& name);
}

template<typename T>
void setName(Device& device, T object, const std::string& name) = delete;

template<>
inline void setName<VkQueue>(Device& device, VkQueue queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_QUEUE, queue, name);
}

template<>
inline void setName<VkDescriptorSet>(Device& device, VkDescriptorSet queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET, queue, name);
}

template<>
inline void setName<VkDescriptorSetLayout>(Device& device, VkDescriptorSetLayout queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, queue, name);
}

template<>
inline void setName<VkDescriptorPool>(Device& device, VkDescriptorPool queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_DESCRIPTOR_POOL, queue, name);
}

template<>
inline void setName<VkImage>(Device& device, VkImage queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_IMAGE, queue, name);
}

// Misc
template<typename T>
constexpr size_t SizeInBytes(const std::vector<T>& vector)
{
	return vector.size() * sizeof(T);
}