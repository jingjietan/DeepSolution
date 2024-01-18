#pragma once

#include <vulkan/vulkan.h>

struct Queue
{
	VkQueue queue;
	uint32_t family;
};