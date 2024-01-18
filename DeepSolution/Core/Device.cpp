#include "Device.h"

#include <vector>

#include <volk.h>
#include "Common.h"

Device::Device(VkPhysicalDevice physicalDevice, VkDevice device, VmaAllocator allocator): 
	device(device), physicalDevice(physicalDevice), allocator(allocator)
{
	
}

VkFormat Device::getDepthFormat() const
{
	if (depthFormat_ != VK_FORMAT_UNDEFINED)
		return depthFormat_;

	const auto formats = {
			VK_FORMAT_D32_SFLOAT_S8_UINT,
			VK_FORMAT_D32_SFLOAT,
			VK_FORMAT_D24_UNORM_S8_UINT, 
			VK_FORMAT_D16_UNORM_S8_UINT, 
			VK_FORMAT_D16_UNORM
	};
	for (const auto& format : formats)
	{
		VkFormatProperties properties;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

		if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			depthFormat_ = format;
			return depthFormat_;
		}
	}
	check(false, "Failed to select depth format!");
}
