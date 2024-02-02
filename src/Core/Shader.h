#pragma once

#include <vulkan/vulkan.h>
#include <spirv_reflect.hpp>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>

VkShaderModule loadShader(VkDevice device, const std::string& path);

enum class BindingOption
{
	Dynamic
};

class ShaderReflect
{
public:
	void add(const std::string& path, VkShaderStageFlagBits stage, const std::map<std::pair<uint32_t, uint32_t>, BindingOption>& options = {});

	std::vector <std::pair<VkShaderStageFlagBits,VkShaderModule>> retrieveShaderModule(VkDevice device);
	std::vector<VkDescriptorSetLayout> retrieveDescriptorSetLayout(VkDevice device);
	VkDescriptorPool retrieveDescriptorPool(VkDevice device, uint32_t multipleOf = 1);
	std::vector<VkDescriptorSet> retrieveSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count = 1);
	VkPipelineLayout retrievePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts) const;

	static std::vector<VkPipelineShaderStageCreateInfo> getStages(const std::vector <std::pair<VkShaderStageFlagBits, VkShaderModule>>& modules);
	static void deleteModules(VkDevice device, const std::vector <std::pair<VkShaderStageFlagBits, VkShaderModule>>& modules);
private:
	std::vector <std::pair<VkShaderStageFlagBits, std::vector<char>> > codes;
	std::unordered_map<uint32_t, std::unordered_map<uint32_t, std::pair<VkShaderStageFlags, VkDescriptorType>>> shaderUsage; // set, binding
	std::unordered_map<VkDescriptorType, uint32_t> descriptorCount;
	size_t pushConstantSize = 0; // there can only be one.
	VkShaderStageFlags pushConstantFlag = 0;
};