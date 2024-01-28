#include "Shader.h"
#include "Common.h"

#include <vector>
#include <fstream>
#include <fmt/format.h>
#include <volk.h>

namespace {
	std::vector<char> getFileBinary(const std::string& filePath)
	{
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);
		check(file.is_open(), fmt::format("Failed to open file at {}", filePath));
		size_t fileSize = file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);
		file.read(buffer.data(), fileSize);
		return buffer;
	}

	std::vector<uint32_t> read_spirv_file(const char* path)
	{
		FILE* file = fopen(path, "rb");
		if (!file)
		{
			fprintf(stderr, "Failed to open SPIR-V file: %s\n", path);
			return {};
		}

		fseek(file, 0, SEEK_END);
		long len = ftell(file) / sizeof(uint32_t);
		rewind(file);

		std::vector<uint32_t> spirv(len);
		if (fread(spirv.data(), sizeof(uint32_t), len, file) != size_t(len))
			spirv.clear();

		fclose(file);
		return spirv;
	}
}

VkShaderModule loadShader(VkDevice device, const std::string& path)
{
	auto code = getFileBinary(path);
	VkShaderModuleCreateInfo moduleCI{};
	moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCI.codeSize = static_cast<uint32_t>(code.size());
	moduleCI.pCode = reinterpret_cast<const uint32_t*>(code.data());
	VkShaderModule shaderModule;
	check(vkCreateShaderModule(device, &moduleCI, nullptr, &shaderModule), "Failed to create shader module!");
	return shaderModule;
}

void ShaderReflect::add(const std::string& path, VkShaderStageFlagBits stage, const std::map<std::pair<uint32_t, uint32_t>, BindingOption>& options)
{
	auto code = getFileBinary(path);
	auto spirv = read_spirv_file(path.c_str());

	spirv_cross::CompilerGLSL glsl(std::move(spirv));

	const auto resources = glsl.get_shader_resources();
	for (const auto& uniform : resources.uniform_buffers)
	{
		const auto set = glsl.get_decoration(uniform.id, spv::DecorationDescriptorSet);
		const auto binding = glsl.get_decoration(uniform.id, spv::DecorationBinding);
		auto type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		if (auto it = options.find({ set, binding }); it != options.end())
		{
			if (it->second == BindingOption::Dynamic)
			{
				type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
			}
		}
		shaderUsage[set][binding].first |= stage;
		shaderUsage[set][binding].second = type;
		descriptorCount[type]++;
	}

	for (const auto& si : resources.sampled_images)
	{
		const auto set = glsl.get_decoration(si.id, spv::DecorationDescriptorSet);
		const auto binding = glsl.get_decoration(si.id, spv::DecorationBinding);
		const auto type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shaderUsage[set][binding].first |= stage;
		shaderUsage[set][binding].second = type;
		descriptorCount[type]++;
	}

	for (const auto& push : resources.push_constant_buffers)
	{
		const auto& type = glsl.get_type(push.base_type_id);
		pushConstantSize = glsl.get_declared_struct_size(type);
		pushConstantFlag |= stage;
	}

	codes.emplace_back(stage, std::move(code));
}

std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>> ShaderReflect::retrieveShaderModule(VkDevice device)
{
	std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>> shaders;
	for (const auto& code : codes)
	{
		VkShaderModuleCreateInfo moduleCI{};
		moduleCI.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCI.codeSize = static_cast<uint32_t>(code.second.size());
		moduleCI.pCode = reinterpret_cast<const uint32_t*>(code.second.data());
		VkShaderModule shaderModule;
		check(vkCreateShaderModule(device, &moduleCI, nullptr, &shaderModule), "Failed to create shader module!");
		shaders.push_back(std::make_pair(code.first, shaderModule));
	}
	return shaders;
}

std::vector<VkDescriptorSetLayout> ShaderReflect::retrieveDescriptorSetLayout(VkDevice device)
{
	std::vector<VkDescriptorSetLayout> layouts;
	for (const auto& set : shaderUsage)
	{
		VkDescriptorSetLayout layout;
		std::vector<VkDescriptorSetLayoutBinding> bindings{};
		for (const auto& binding : set.second)
		{
			VkDescriptorSetLayoutBinding layoutBinding{};
			layoutBinding.binding = binding.first;
			layoutBinding.descriptorCount = 1;
			layoutBinding.descriptorType = binding.second.second;
			layoutBinding.stageFlags = binding.second.first;
			bindings.push_back(layoutBinding);
		}

		VkDescriptorSetLayoutCreateInfo ci{};
		ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		ci.bindingCount = static_cast<uint32_t>(bindings.size());
		ci.pBindings = bindings.data();
		check(vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout));
		layouts.push_back(layout);
	}

	return layouts;
}

VkDescriptorPool ShaderReflect::retrieveDescriptorPool(VkDevice device, uint32_t multipleOf)
{
	VkDescriptorPool pool;
	std::vector<VkDescriptorPoolSize> poolSizes{};

	for (const auto& descType : descriptorCount)
	{
		VkDescriptorPoolSize poolSize{};
		poolSize.descriptorCount = descType.second * multipleOf;
		poolSize.type = descType.first;
		poolSizes.push_back(poolSize);
	}

	VkDescriptorPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolCI.maxSets = multipleOf;
	poolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	poolCI.pPoolSizes = poolSizes.data();
	check(vkCreateDescriptorPool(device, &poolCI, nullptr, &pool));
	return pool;
}

std::vector<VkDescriptorSet> ShaderReflect::retrieveSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout, uint32_t count)
{
	std::vector<VkDescriptorSet> sets{ count };
	std::vector<VkDescriptorSetLayout> layouts{ count, layout };
	VkDescriptorSetAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocateInfo.descriptorPool = pool;
	allocateInfo.descriptorSetCount = count;
	allocateInfo.pSetLayouts = layouts.data();
	check(vkAllocateDescriptorSets(device, &allocateInfo, sets.data()));
	return sets;
}

VkPipelineLayout ShaderReflect::retrievePipelineLayout(VkDevice device, const std::vector<VkDescriptorSetLayout>& layouts) const
{
	VkPipelineLayout layout; 

	VkPushConstantRange pushRange{};
	pushRange.size = pushConstantSize;
	pushRange.stageFlags = pushConstantFlag;

	VkPipelineLayoutCreateInfo layoutCI{};
	layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutCI.setLayoutCount = static_cast<uint32_t>(layouts.size());
	layoutCI.pSetLayouts = layouts.data();
	
	if (pushConstantSize != 0)
	{
		layoutCI.pushConstantRangeCount = 1;
		layoutCI.pPushConstantRanges = &pushRange;
	}
	
	check(vkCreatePipelineLayout(device, &layoutCI, nullptr, &layout));
	return layout;
}

std::vector<VkPipelineShaderStageCreateInfo> ShaderReflect::getStages(const std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>>& modules)
{
	std::vector<VkPipelineShaderStageCreateInfo> infos{};

	for (const auto& module : modules)
	{
		auto ci = CreateInfo::ShaderStage(module.first, module.second);
		infos.push_back(ci);
	}

	return infos;
}

void ShaderReflect::deleteModules(VkDevice device, const std::vector<std::pair<VkShaderStageFlagBits, VkShaderModule>>& modules)
{
	for (const auto& module : modules)
	{
		vkDestroyShaderModule(device, module.second, nullptr);
	}
}

