#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.h>
#include <array>
#include <vector>

struct BasicVertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 uv;

	VkVertexInputBindingDescription static BindingDescription()
	{
		VkVertexInputBindingDescription bd{};
		bd.binding = 0;
		bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		bd.stride = sizeof(BasicVertex);
		return bd;
	}
	std::array<VkVertexInputAttributeDescription, 3> static AttributesDescription()
	{
		std::array<VkVertexInputAttributeDescription, 3> ats{};
		ats[0].binding = 0;
		ats[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		ats[0].location = 0;
		ats[0].offset = offsetof(BasicVertex, position);
		ats[1].binding = 0;
		ats[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		ats[1].location = 1;
		ats[1].offset = offsetof(BasicVertex, normal);
		ats[2].binding = 0;
		ats[2].format = VK_FORMAT_R32G32_SFLOAT;
		ats[2].location = 2;
		return ats;
	}
	std::array<VkVertexInputAttributeDescription, 1> static PositionAttributesDescription()
	{
		std::array<VkVertexInputAttributeDescription, 1> ats{};
		ats[0].binding = 0;
		ats[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		ats[0].location = 0;
		ats[0].offset = offsetof(BasicVertex, position);
		return ats;
	}
};

const static std::vector<BasicVertex> cubeVertices = {
		{{ -1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
		{{1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
		{{1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 0.0f}}, // bottom-right         
		{{1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {1.0f, 1.0f}}, // top-right
		{{ -1.0f, -1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 0.0f}}, // bottom-left
		{{ -1.0f,  1.0f, -1.0f}, {0.0f,  0.0f, -1.0f}, {0.0f, 1.0f}}, // top-left
		// front face
		{{-1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}}, // bottom-left
		{{1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 0.0f}}, // bottom-right
		{{1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}}, // top-right
		{{1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {1.0f, 1.0f}}, // top-right
		{{-1.0f,  1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 1.0f}}, // top-left
		{{-1.0f, -1.0f,  1.0f}, {0.0f,  0.0f,  1.0f}, {0.0f, 0.0f}}, // bottom-left
		// left face

		{{-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-right
		{{-1.0f,  1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // top-left
		{{-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-left
		{{-1.0f, -1.0f, -1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-left
		{{-1.0f, -1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // bottom-right
		{{-1.0f,  1.0f,  1.0f}, {-1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-right

		// // right face
		{{1.0f,  1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-left
		{{1.0f, -1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-right
		{{1.0f,  1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 1.0f}}, // top-right         
		{{1.0f, -1.0f, -1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 1.0f}}, // bottom-right
		{{1.0f,  1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {1.0f, 0.0f}}, // top-left
		{{1.0f, -1.0f,  1.0f},  {1.0f,  0.0f,  0.0f}, {0.0f, 0.0f}}, // bottom-left     
		// bottom face								  
		{{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-right
		{{1.0f, -1.0f, -1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 1.0f}}, // top-left
		{{1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-left
		{{1.0f, -1.0f,  1.0f }, { 0.0f, -1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-left
		{{-1.0f, -1.0f,  1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 0.0f}}, // bottom-right
		{{-1.0f, -1.0f, -1.0f}, { 0.0f, -1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-right
		// top face												  
		{{-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-left
		{{1.0f,  1.0f , 1.0f }, { 0.0f,  1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-right
		{{ 1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 1.0f, 1.0f}}, // top-right     
		{{1.0f,  1.0f,  1.0f }, { 0.0f,  1.0f,  0.0f},{ 1.0f, 0.0f}}, // bottom-right
		{{-1.0f,  1.0f, -1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 1.0f}}, // top-left
		{{-1.0f,  1.0f,  1.0f}, { 0.0f,  1.0f,  0.0f},{ 0.0f, 0.0f}}  // bottom-left   
};