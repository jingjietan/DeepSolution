#pragma once

#include <memory>
#include <vector>
#include "Camera/Camera.h"
#include "Light.h"
#include "Timer.h"

struct GlobalUniform
{
	glm::mat4 view;
	glm::mat4 projection;
	glm::vec3 viewPos;
};


struct alignas(16) LightUpload // base alignment
{
	int32_t count;
};

struct State
{
	std::unique_ptr<Camera> camera_;
	std::vector<Light> lights_; 
	Timer timer_;
};