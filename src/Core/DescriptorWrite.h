#pragma once

#include <volk.h>
#include <vector>
#include <list>
#include <map>
#include <string>

enum class BufferType
{
	Uniform,
	DynamicUniform,
	Storage
};

enum class ImageType
{
	CombinedSampler
};

class DescriptorWrite
{
public:
	void add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, BufferType type, uint32_t count, VkBuffer buffer, VkDeviceSize offset, VkDeviceSize range);
	void add(VkDescriptorSet set, uint32_t binding, uint32_t arrayElement, ImageType type, uint32_t count, VkSampler sampler, VkImageView imageView, VkImageLayout imageLayout);
	void write(VkDevice device);
private:
	std::list<VkDescriptorBufferInfo> bufferInfos; // to prevent invalidation :)
	std::list<VkDescriptorImageInfo> imageInfos;
	std::vector<VkWriteDescriptorSet> writes;
};

class DescriptorCreator
{
public:
	void add(const std::string& name, uint32_t binding, VkDescriptorType type, uint32_t count, VkShaderStageFlags stages, VkDescriptorBindingFlags flags = 0);

	VkDescriptorSetLayout createLayout(const std::string& name, VkDevice device, VkDescriptorSetLayoutCreateFlags flags = 0);
	VkDescriptorPool createPool(const std::string& name, VkDevice device, uint32_t multiplesOf = 1u, VkDescriptorPoolCreateFlags flags = 0);
	VkPipelineLayout createPipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts, VkPushConstantRange* range = nullptr);
	void allocateSets(const std::string& name, VkDevice device, uint32_t count, VkDescriptorSet* pSets);
	void allocateVariableSets(const std::string& name, VkDevice device, uint32_t count, VkDescriptorSet* pSets, uint32_t variableCount);
private:
	std::map<std::string, std::pair<std::vector<VkDescriptorSetLayoutBinding>, std::vector<VkDescriptorBindingFlags>>> pipelineBindings;
	std::map<std::string, VkDescriptorSetLayout> createdLayouts;
	std::map<std::string, VkDescriptorPool> createdPools;
};