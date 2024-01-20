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
