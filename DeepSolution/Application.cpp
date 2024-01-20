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

	camera_ = std::make_unique<Camera>(glm::vec3(0, 0, 0), 0.f, -177.f, width, height, 0.01f, 100.0f);

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
	scene_->loadGLTF("assets/subway/scene.gltf");
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
	auto commandBuffer = swapchain_->Acquire();

	Transition::UndefinedToColorAttachment(swapchain_->GetCurrentImage(), commandBuffer);

	// Begin Rendering
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = swapchain_->GetCurrentImageView();
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue = { 0.7f, 0.7f, 0.7f, 1.f };

	VkRenderingAttachmentInfo depthAttachment{};
	depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachment.imageView = swapchain_->GetDepthImageView();
	depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachment.clearValue = { 1.0f, 0 };

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.renderArea.extent = swapchain_->GetExtent();
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	renderInfo.pDepthAttachment = &depthAttachment;
	
	vkCmdBeginRendering(commandBuffer, &renderInfo);

	VkViewport viewport{}; 
	viewport.x = 0.0f;
	viewport.y = static_cast<float>(swapchain_->GetExtent().height);
	viewport.width = static_cast<float>(swapchain_->GetExtent().width);
	viewport.height = -static_cast<float>(swapchain_->GetExtent().height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

	VkRect2D scissor{};
	scissor.extent = swapchain_->GetExtent();
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	scene_->draw(commandBuffer, *camera_);
	
	vkCmdEndRendering(commandBuffer);

	// ImGui Rendering
	imgui_->Draw(swapchain_->GetCurrentImageView(), swapchain_->GetExtent(), commandBuffer);
	
	// Transition Then Present.
	auto image = swapchain_->GetCurrentImage();
	Transition::ColorAttachmentToPresentable(image, commandBuffer);
	swapchain_->Present();
}
