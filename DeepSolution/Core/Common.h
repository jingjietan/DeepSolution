#pragma once

#include <string>
#include <source_location>

#include <vulkan/vulkan.h>

void check(bool condition, const std::string& message = "", const std::source_location& location = std::source_location::current());

inline void check(VkResult result, const std::string& message = "", const std::source_location& location = std::source_location::current())
{
	return check(result == VK_SUCCESS, message, location);
}

void check(auto condition, const std::string& message = "", const std::source_location& location = std::source_location::current())
{
	check(static_cast<bool>(condition), message, location);
}


namespace CreateInfo {
	VkFence createFence(VkDevice device, VkFenceCreateFlags flags);
	VkSemaphore createBinarySemaphore(VkDevice device);
	VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool);

	VkRenderPass createRenderPass(VkDevice device, const VkAttachmentDescription2* pAttachments, uint32_t attachmentCount, const VkSubpassDescription2* pSubpass, uint32_t subpassCount, const VkSubpassDependency2* pDependency, uint32_t dependencyCount);
}

class Device;


namespace Detail {
	void setName(Device* device, VkObjectType type, void* ptr, const std::string& name);
}

template<typename T>
void setName(Device* device, T object, const std::string& name) = delete;

template<>
inline void setName<VkQueue>(Device* device, VkQueue queue, const std::string& name)
{
	Detail::setName(device, VK_OBJECT_TYPE_QUEUE, queue, name);
}