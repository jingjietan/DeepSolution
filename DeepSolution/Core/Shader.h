#pragma once

#include <vulkan/vulkan.h>
#include <string>

VkShaderModule loadShader(VkDevice device, const std::string& path);