#pragma once

#include <memory>
#include <vector>
#include "Camera.h"
#include "Light.h"
#include "Timer.h"

struct State
{
	std::unique_ptr<Camera> camera_;
	std::vector<Light> lights_; 
	Timer timer_;
};