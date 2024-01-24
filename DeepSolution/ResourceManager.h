#pragma once

#include <vulkan/vulkan.h>
#include <vector>
class Device;
class ResourceManager
{
public:
	void Connect(Device& device);

	void Destroy();
private:
	Device* device_{};

	VkDescriptorPool pool_{};
	VkDescriptorSetLayout layout_{};
	std::vector<VkDescriptorSet> bindlessSets_{};
};