#include "Application.h"

#include "Core/Common.h"
#include "Core/Transition.h"

#include <VkBootstrap.h>
#include <volk.h>
#include <spdlog/spdlog.h>

Application::Application(int width, int height)
{
	check(glfwInit());
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window_ = glfwCreateWindow(width, height, "DeepSolution", nullptr, nullptr);
	check(window_);

	glfwSetWindowUserPointer(window_, this);
	glfwSetCursorPosCallback(window_, [](GLFWwindow* window, double xpos, double ypos) {
		auto app = static_cast<Application*>(glfwGetWindowUserPointer(window));
		static float xPos = xpos;
		static float yPos = ypos;
		float dX = xpos - xPos;
		float dY = yPos - ypos;

		xPos = xpos;
		yPos = ypos;
		if (!app->isFocused)
		{
			return;
		}

		app->camera_->moveDirection(dX, dY);
		});

	camera_ = std::make_unique<Camera>(glm::vec3(0, 2, 0), 0.f, -177.f, width, height, 0.01f, 50.0f);

	device_.init(window_);
	
	// ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         
	io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;     

	ImGuiStyle& style = ImGui::GetStyle();
	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		style.WindowRounding = 0.0f;
		style.Colors[ImGuiCol_WindowBg].w = 1.0f;
	}

	imgui_ = std::make_unique<ImGuiAdapter>(window_, device_);

	//
	swapchain_ = std::make_unique<Swapchain>(device_, device_.getDepthFormat());

	scene_ = std::make_unique<Scene>(device_);
	// scene_->loadGLTF("assets/subway/scene.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/BoomBoxWithAxes/glTF/BoomBoxWithAxes.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/SciFiHelmet/glTF/SciFiHelmet.gltf");
	// scene_->loadGLTF("assets/glTF-Sample-Assets/Models/ABeautifulGame/glTF/ABeautifulGame.gltf");
}

void Application::run()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();

		// Update Camera
		int width, height;
		glfwGetFramebufferSize(window_, &width, &height);
		camera_->updateViewport(width, height);

		// 
		if (glfwGetKey(window_, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		{
			glfwSetWindowShouldClose(window_, true);
			break;
		}

		static float timeout = 0.f;
		if (timeout <= 0 && glfwGetKey(window_, GLFW_KEY_J) == GLFW_PRESS)
		{
			if (isFocused)
			{
				glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
				isFocused = false;
			}
			else
			{
				glfwSetInputMode(window_, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
				isFocused = true;
			}
			timeout = 1.0f;
		}


		auto deltaTime = timer_.getDeltaTime();
		timeout -= deltaTime;

		if (glfwGetKey(window_, GLFW_KEY_W) == GLFW_PRESS)
		{
			camera_->movePosition(Direction::Forward, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_S) == GLFW_PRESS)
		{
			camera_->movePosition(Direction::Backward, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_A) == GLFW_PRESS)
		{
			camera_->movePosition(Direction::Left, deltaTime);
		}
		if (glfwGetKey(window_, GLFW_KEY_D) == GLFW_PRESS)
		{
			camera_->movePosition(Direction::Right, deltaTime);
		}

		imgui_->BeginFrame();

		ImGui::ShowDemoWindow();

		imgui_->EndFrame();

		Draw();
	}
}

Application::~Application()
{
	vkDeviceWaitIdle(device_.device);

	scene_.reset();

	imgui_.reset();
	swapchain_.reset();

	device_.deinit();
}

void Application::Draw()
{
	// Guard.
	int width, height;
	glfwGetFramebufferSize(window_, &width, &height);
	if (width == 0 || height == 0)
	{
		return;
	}

	// Scene Rendering
	auto commandBuffer = swapchain_->Acquire(width , height);

	Transition::UndefinedToColorAttachment(swapchain_->GetCurrentImage(), commandBuffer);

	scene_->draw(commandBuffer, *camera_, swapchain_->GetCurrentImageView(), swapchain_->GetDepthImageView());
	
	// ImGui Rendering
	imgui_->Draw(swapchain_->GetCurrentImageView(), swapchain_->GetExtent(), commandBuffer);
	
	// Transition Then Present.
	auto image = swapchain_->GetCurrentImage();
	Transition::ColorAttachmentToPresentable(image, commandBuffer);
	swapchain_->Present();
}
