#include "ImGuiAdapter.h"
#include "Core/Device.h"
#include "Core/Common.h"
#include "Core/Transition.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>

#include <volk.h>

#include <vector>


ImGuiAdapter::ImGuiAdapter(GLFWwindow* window, Device& device): device_(device)
{
	// Create Descriptor Pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}
	};

	VkDescriptorPoolCreateInfo descriptorPoolCI{};
	descriptorPoolCI.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCI.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
	descriptorPoolCI.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
	descriptorPoolCI.pPoolSizes = poolSizes.data();
	descriptorPoolCI.maxSets = 10;
	check(vkCreateDescriptorPool(device.device, &descriptorPoolCI, nullptr, &descriptorPool_));

	//
	ImGui_ImplGlfw_InitForVulkan(window, true);

	ImGui_ImplVulkan_LoadFunctions([](const char* fn, void* data) {
		return vkGetInstanceProcAddr(reinterpret_cast<VkInstance>(data), fn);
		}, device_.instance);

	ImGui_ImplVulkan_InitInfo initInfo{};
	initInfo.Instance = device_.instance;
	initInfo.PhysicalDevice = device.physicalDevice;
	initInfo.Device = device.device;
	initInfo.Queue = device.graphicsQueue.queue;
	initInfo.QueueFamily = device.graphicsQueue.family;
	initInfo.DescriptorPool = descriptorPool_;
	initInfo.Subpass = 0;
	initInfo.MinImageCount = 2;
	initInfo.ImageCount = 2;
	initInfo.UseDynamicRendering = true;
	initInfo.ColorAttachmentFormat = VK_FORMAT_R8G8B8A8_SRGB; // somehow use swapchain format here?
	ImGui_ImplVulkan_Init(&initInfo, nullptr);
}

void ImGuiAdapter::BeginFrame()
{
	ImGui_ImplVulkan_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
}

void ImGuiAdapter::EndFrame()
{
	ImGui::Render();
	

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();
	}
}

void ImGuiAdapter::Draw(VkImageView imageView, VkExtent2D extent, VkCommandBuffer commandBuffer)
{
	// Begin Rendering
	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = imageView;
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.clearValue = { 1, 1, 1, 1 };

	VkRenderingInfo renderInfo{};
	renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderInfo.renderArea.offset = { 0, 0 };
	renderInfo.renderArea.extent = extent;
	renderInfo.layerCount = 1;
	renderInfo.colorAttachmentCount = 1;
	renderInfo.pColorAttachments = &colorAttachment;
	vkCmdBeginRendering(commandBuffer, &renderInfo);

	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);

	vkCmdEndRendering(commandBuffer);
}

ImGuiAdapter::~ImGuiAdapter()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();

	vkDestroyDescriptorPool(device_.device, descriptorPool_, nullptr);
}