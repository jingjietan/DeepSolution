#include <VkBootstrap.h>
#include <volk.h>
#include <spdlog/spdlog.h>

#include "Swapchain.h"
#include "Common.h"
#include "Transition.h"
#include "Device.h"

Swapchain::Swapchain(Device& device, VkFormat depthFormat): device_(device), depthFormat(depthFormat)
{
	graphics_ = device_.graphicsQueue.queue;
	framesInFlight_ = device_.getMaxFramesInFlight();

	VkCommandPoolCreateInfo poolCI{};
	poolCI.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolCI.queueFamilyIndex = device_.graphicsQueue.family;
	poolCI.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	check(vkCreateCommandPool(device_.device, &poolCI, nullptr, &commandPool_));

	for (size_t i = 0; i < framesInFlight_; i++)
	{
		FrameResource resource{};
		resource.acquire = CreateInfo::createBinarySemaphore(device_.device);
		resource.present = CreateInfo::createBinarySemaphore(device_.device);
		resource.fence = CreateInfo::createFence(device_.device, VK_FENCE_CREATE_SIGNALED_BIT);
		resource.commandBuffer = CreateInfo::allocateCommandBuffer(device_.device, commandPool_);
		frameResources_.emplace_back(resource);
	}

	Refresh();
}

void Swapchain::Refresh()
{
	if (swapchain_)
	{
		vkDeviceWaitIdle(device_.device);
	}

	vkb::SwapchainBuilder swapchainBuilder(device_.physicalDevice, device_.device, device_.surface);
	auto swapchainResult = swapchainBuilder
		.set_desired_format({ .format = VK_FORMAT_R8G8B8A8_SRGB, .colorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR })
		.set_desired_present_mode(VK_PRESENT_MODE_MAILBOX_KHR)
		.add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
		.set_old_swapchain(swapchain_)
		.build();
	
	check(swapchainResult);

	vkDestroySwapchainKHR(device_.device, swapchain_, nullptr);

	vkb::Swapchain& swapchain = swapchainResult.value();
	imageFormat = swapchain.image_format;
	colorSpace = swapchain.color_space;
	presentMode = swapchain.present_mode;

	for (const auto& imageView : imageViews_)
	{
		vkDestroyImageView(device_.device, imageView, nullptr);
	}

	swapchain_ = swapchain.swapchain;
	images_ = swapchain.get_images().value();
	imageViews_ = swapchain.get_image_views().value();

	surfaceFormat_ = swapchain.image_format;
	surfaceExtent_ = swapchain.extent;

	// Setup depth images if requested.
	if (depthFormat != VK_FORMAT_UNDEFINED)
	{
		if (depthImage_)
		{
			vkDestroyImageView(device_.device, depthImageView_, nullptr);
			vmaDestroyImage(device_.allocator, depthImage_, depthAllocation_);
		}

		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.extent = { surfaceExtent_.width, surfaceExtent_.height, 1 };
		imageCI.format = depthFormat;
		imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		imageCI.mipLevels = 1;
		imageCI.arrayLayers = 1;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;

		VmaAllocationCreateInfo allocationCI{};
		allocationCI.usage = VMA_MEMORY_USAGE_AUTO;
		
		check(vmaCreateImage(device_.allocator, &imageCI, &allocationCI, &depthImage_, &depthAllocation_, nullptr));

		VkImageViewCreateInfo imageViewCI{};
		imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCI.format = depthFormat;
		imageViewCI.image = depthImage_;
		imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCI.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
		check(vkCreateImageView(device_.device, &imageViewCI, nullptr, &depthImageView_));

		CreateInfo::performOneTimeAction(device_.device, device_.graphicsQueue.queue, device_.graphicsPool, [&](VkCommandBuffer commandBuffer) {
			Transition::UndefinedToDepthStencilAttachment(depthImage_, commandBuffer);
		});
	}
}

VkCommandBuffer Swapchain::Acquire(int width, int height)
{
	if (width != surfaceExtent_.width || height != surfaceExtent_.height)
	{
		Refresh();
	}

	const auto& resource = frameResources_[currentFrame_];
	vkWaitForFences(device_.device, 1, &resource.fence, VK_TRUE, UINT64_MAX);
	
	VkResult result = vkAcquireNextImageKHR(device_.device, swapchain_, UINT64_MAX, resource.acquire, nullptr, &imageIndex_);
	while (true)
	{
		if (result != VK_ERROR_OUT_OF_DATE_KHR)
		{
			check(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Failed to acquire swapchain image!");
			break;
		}
		Refresh();
		SPDLOG_INFO("Recreated swapchain at acquire!");
	} 
	vkResetFences(device_.device, 1, &resource.fence);

	check(vkResetCommandBuffer(resource.commandBuffer, 0));

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	check(vkBeginCommandBuffer(resource.commandBuffer, &beginInfo));

	return resource.commandBuffer;
}

void Swapchain::Present()
{
	const auto& resource = frameResources_[currentFrame_];

	check(vkEndCommandBuffer(resource.commandBuffer));

	VkSubmitInfo submitInfo{};
	VkSemaphore wait[] = { resource.acquire };
	VkPipelineStageFlags waitAt[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = wait;
	submitInfo.pWaitDstStageMask = waitAt;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &resource.commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &resource.present;

	check(vkQueueSubmit(graphics_, 1, &submitInfo, resource.fence));

	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &resource.present;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &swapchain_;
	presentInfo.pImageIndices = &imageIndex_;

	VkResult result = vkQueuePresentKHR(graphics_, &presentInfo);
	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
	{
		Refresh();
		SPDLOG_INFO("Recreated swapchain at present!");
	}
	else
	{
		check(result, "Failed to present image!");
	}
	currentFrame_ = (currentFrame_ + 1) % framesInFlight_;
}


VkImage Swapchain::GetCurrentImage() const
{
	return images_[imageIndex_];
}

VkImageView Swapchain::GetCurrentImageView() const
{
	return imageViews_[imageIndex_];
}

VkImageView Swapchain::GetDepthImageView() const
{
	return depthImageView_;
}

VkExtent2D Swapchain::GetExtent() const
{
	return surfaceExtent_;
}

VkFormat Swapchain::GetFormat() const
{
	return imageFormat;
}


Swapchain::~Swapchain()
{
	vkDestroyImageView(device_.device, depthImageView_, nullptr);
	vmaDestroyImage(device_.allocator, depthImage_, depthAllocation_);
	//
	for (const auto& resource : frameResources_)
	{
		vkDestroyFence(device_.device, resource.fence, nullptr);
		vkDestroySemaphore(device_.device, resource.acquire, nullptr);
		vkDestroySemaphore(device_.device, resource.present, nullptr);
	}
	vkDestroyCommandPool(device_.device, commandPool_, nullptr);
	frameResources_.clear();
	//
	for (const auto& imageView : imageViews_)
	{
		vkDestroyImageView(device_.device, imageView, nullptr);
	}
	vkDestroySwapchainKHR(device_.device, swapchain_, nullptr);
}



