#include "Application.h"

#include <VkBootstrap.h>

#include "Core/Common.h"
#include <volk.h>
#include <spdlog/spdlog.h>

Application::Application(int width, int height)
{
	check(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_ = glfwCreateWindow(width, height, "DeepSolution", nullptr, nullptr);
	check(window_);

	check(volkInitialize());

	vkb::Instance temporaryInstance;
	vkb::PhysicalDevice temporaryPhysicalDevice;
	vkb::Device temporaryDevice;

	{
#ifndef NDEBUG
		static PFN_vkDebugUtilsMessengerCallbackEXT callback = [](
			VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
			VkDebugUtilsMessageTypeFlagsEXT messageType,
			const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
			void* pUserData) {
				auto severityString = vkb::to_string_message_severity(messageSeverity);
				auto typeString = vkb::to_string_message_type(messageType);

				SPDLOG_WARN("{} {}. {}", severityString, typeString, pCallbackData->pMessage);

				return VK_TRUE;
			};
#endif // !NDEBUG

		vkb::InstanceBuilder instanceBuilder;
		auto instanceBuildResult = instanceBuilder
			.require_api_version(1, 3, 0)
#ifndef NDEBUG
			.request_validation_layers()
			.set_debug_callback(callback)
#endif // !NDEBUG
			.build();

		if (!instanceBuildResult)
		{
			SPDLOG_ERROR("Failed to create instance. {}", instanceBuildResult.error().message());
			std::abort();
		}
		temporaryInstance = instanceBuildResult.value();
	}

	instance_ = temporaryInstance.instance;
	messenger_ = temporaryInstance.debug_messenger;

	volkLoadInstance(instance_);

	check(glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) == VK_SUCCESS, "Failed to create window surface.");

	{
		VkPhysicalDeviceSynchronization2Features synchronization{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES };
		synchronization.synchronization2 = VK_TRUE;
		VkPhysicalDeviceDynamicRenderingFeatures dynamicRendering{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES };
		dynamicRendering.dynamicRendering = VK_TRUE;
		dynamicRendering.pNext = &synchronization;
		VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bufferDeviceAddress{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES };
		bufferDeviceAddress.bufferDeviceAddress = VK_TRUE;
		bufferDeviceAddress.pNext = &dynamicRendering;
		VkPhysicalDeviceDescriptorIndexingFeaturesEXT descriptorIndexing{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES };
		descriptorIndexing.runtimeDescriptorArray = VK_TRUE;
		descriptorIndexing.pNext = &bufferDeviceAddress;

		VkPhysicalDeviceFeatures features{};
		features.samplerAnisotropy = VK_TRUE;

		vkb::PhysicalDeviceSelector physicalDeviceSelector{ temporaryInstance };
		auto physicalDeviceSelectorResult = physicalDeviceSelector
			.set_surface(surface_)
			.add_required_extension_features(descriptorIndexing)
			.add_required_extensions({
				VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
				VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME,
				VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
				VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
			})
			.set_required_features(features)
			.select();

		if (!physicalDeviceSelectorResult)
		{
			SPDLOG_ERROR("Failed to choose physical device. {}", physicalDeviceSelectorResult.error().message());
			std::abort();
		}
		temporaryPhysicalDevice = physicalDeviceSelectorResult.value();
	}

	auto physicalDevice = temporaryPhysicalDevice.physical_device;

	{
		vkb::DeviceBuilder deviceBuilder{ temporaryPhysicalDevice };
		auto deviceBuildResult = deviceBuilder.build();
		if (!deviceBuildResult)
		{
			SPDLOG_ERROR("Failed to create device. {}", deviceBuildResult.error().message());
			std::abort();
		}
		temporaryDevice = deviceBuildResult.value();
	}
	auto& device = temporaryDevice;
	volkLoadDevice(device);

	{
		auto graphicsQueueResult = temporaryDevice.get_queue(vkb::QueueType::graphics);
		auto graphicsQueueIndexResult = temporaryDevice.get_queue_index(vkb::QueueType::graphics);
		if (!graphicsQueueResult || !graphicsQueueIndexResult)
		{
			SPDLOG_ERROR("Failed to retrieve graphics queue/index. {} {}",
				graphicsQueueResult ? "" : graphicsQueueResult.error().message(),
				graphicsQueueIndexResult ? "" : graphicsQueueIndexResult.error().message()
			);
			std::abort();
		}
		graphics_ = { graphicsQueueResult.value(), graphicsQueueIndexResult.value() };
	}
	{
		auto transferQueueResult = temporaryDevice.get_dedicated_queue(vkb::QueueType::transfer);
		auto transferQueueIndexResult = temporaryDevice.get_dedicated_queue_index(vkb::QueueType::transfer);
		if (!transferQueueResult || !transferQueueIndexResult)
		{
			SPDLOG_ERROR("Failed to retrieve transfer queue/index. {} {}",
				transferQueueResult ? "" : transferQueueResult.error().message(),
				transferQueueIndexResult ? "" : transferQueueIndexResult.error().message()
			);
			std::abort();
		}
		transfer_ = { transferQueueResult.value(), transferQueueIndexResult.value() };
	}
	VmaAllocator allocator;
	{
		VmaVulkanFunctions vulkanFunctions{};
		vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
		vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

		// 1.2
		vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
		vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
		vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
		vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
		vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
		vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
		vulkanFunctions.vkCreateImage = vkCreateImage;
		vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
		vulkanFunctions.vkDestroyImage = vkDestroyImage;
		vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
		vulkanFunctions.vkFreeMemory = vkFreeMemory;
		vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
		vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
		vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
		vulkanFunctions.vkMapMemory = vkMapMemory;
		vulkanFunctions.vkUnmapMemory = vkUnmapMemory;

		// 1.3
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
		vulkanFunctions.vkGetDeviceBufferMemoryRequirements = vkGetDeviceBufferMemoryRequirements;
		vulkanFunctions.vkGetDeviceImageMemoryRequirements = vkGetDeviceImageMemoryRequirements;
		vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2;
		vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2;
		vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2;
		vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2;
		vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2;

		VmaAllocatorCreateInfo allocatorCreateInfo{};
		allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
		allocatorCreateInfo.physicalDevice = physicalDevice;
		allocatorCreateInfo.device = device;
		allocatorCreateInfo.instance = instance_;
		allocatorCreateInfo.pVulkanFunctions = &vulkanFunctions;
		allocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
		check(vmaCreateAllocator(&allocatorCreateInfo, &allocator));
	}
	device_ = Device(physicalDevice, device, allocator);
	setName(&device_, graphics_.queue, "Graphics Queue");

	swapchain_.SetDepthFormat(device_.getDepthFormat());
	swapchain_.Connect(device_, surface_, graphics_);
}

void Application::run()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();


	

		Draw();
	}
}

Application::~Application()
{
	vkDeviceWaitIdle(device_.device);

	swapchain_.Release();

	vmaDestroyAllocator(device_.allocator);

	vkDestroyDevice(device_.device, nullptr);
	vkDestroySurfaceKHR(instance_, surface_, nullptr);
	vkDestroyDebugUtilsMessengerEXT(instance_, messenger_, nullptr);
	vkDestroyInstance(instance_, nullptr);
}

void Application::Draw()
{
	// Guard.
	int width, height;
	glfwGetFramebufferSize(window_, &width, &height);
	if (width == 0 || height == 0)
	{
		return;
	}

	auto cb = swapchain_.Acquire({0.7f, 0.7f, 0.7f, 1.f}, {1.0f, 0});

	auto image = swapchain_.GetCurrentImage();


	swapchain_.Present();
}
