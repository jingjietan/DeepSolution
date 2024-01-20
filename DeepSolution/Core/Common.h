#pragma once

#include <string>
#include <source_location>
#include <functional>

#include <vulkan/vulkan.h>

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


namespace CreateInfo {
	VkFence createFence(VkDevice device, VkFenceCreateFlags flags);
	VkSemaphore createBinarySemaphore(VkDevice device);
	VkCommandBuffer allocateCommandBuffer(VkDevice device, VkCommandPool commandPool);
	
	VkCommandPool createCommandPool(VkDevice device, uint32_t queueFamily, uint32_t flags = 0);
	void performOneTimeAction(VkDevice device, VkQueue queue, VkCommandPool commandPool, const std::function<void(VkCommandBuffer)>& action);

	VkRenderPass createRenderPass(VkDevice device, const VkAttachmentDescription2* pAttachments, uint32_t attachmentCount, const VkSubpassDescription2* pSubpass, uint32_t subpassCount, const VkSubpassDependency2* pDependency, uint32_t dependencyCount);
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