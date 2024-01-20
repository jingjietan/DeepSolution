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
}

void Application::run()
{
	while (!glfwWindowShouldClose(window_))
	{
		glfwPollEvents();

		imgui_->BeginFrame();

		ImGui::ShowDemoWindow();

		imgui_->EndFrame();

		Draw();
	}
}

Application::~Application()
{
	vkDeviceWaitIdle(device_.device);

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
	
	vkCmdEndRendering(commandBuffer);

	// ImGui Rendering
	imgui_->Draw(swapchain_->GetCurrentImageView(), swapchain_->GetExtent(), commandBuffer);
	
	// Transition Then Present.
	auto image = swapchain_->GetCurrentImage();
	Transition::ColorAttachmentToPresentable(image, commandBuffer);
	swapchain_->Present();
}
