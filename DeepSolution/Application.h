#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include "Core/Swapchain.h"
#include "Core/Device.h" 
#include "ImGuiAdapter.h"

#include <memory>
#include "Scene/Scene.h"
#include "Camera.h"
#include "Timer.h"
#include "Light.h"
#include "State.h"

class Application
{
public:
	Application() = default;
	Application(int width, int height);

	void run();

	~Application();
private:
	GLFWwindow* window_;
	
	Device device_;

	std::unique_ptr<Swapchain> swapchain_;
	std::unique_ptr<ImGuiAdapter> imgui_;
	std::unique_ptr<Scene> scene_;

	State state_{};
	//
	bool showLightMenu_ = true;
	glm::vec3 lightPosition_{};

	

public:
	bool isFocused;

private:


	void Draw();
};