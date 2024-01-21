#include "Image.h"
#include "Common.h"
#include "Device.h"
#include "Buffer.h"

#include <volk.h>

#include <cassert>

Image::Image(Device& device, const VkImageCreateInfo& imageInfo): device_(device), format_(imageInfo.format), width(imageInfo.extent.width), height(imageInfo.extent.height)
{
	VmaAllocationCreateInfo allocCI{};
	allocCI.usage = VMA_MEMORY_USAGE_AUTO;

	check(vmaCreateImage(device_.allocator, &imageInfo, &allocCI, &image_, &allocation_, nullptr));
}

void Image::AttachImageView(VkImageAspectFlags flags)
{
	assert(image_ && "Image is not initialised!");
	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.image = image_;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.format = format_;
	imageViewCI.subresourceRange = { flags, 0, 1, 0, 1 };

	check(vkCreateImageView(device_.device, &imageViewCI, nullptr, &imageView_));
}

void Image::AttachSampler(const VkSamplerCreateInfo& samplerCI)
{
	assert(image_ && "Image is not initialised!");
	check(vkCreateSampler(device_.device, &samplerCI, nullptr, &sampler_));
}

void Image::Upload(VkCommandBuffer commandBuffer, VkBuffer buffer) const
{
	VkBufferImageCopy2 imageCopy{};
	imageCopy.sType = VK_STRUCTURE_TYPE_BUFFER_IMAGE_COPY_2;
	imageCopy.imageOffset = { 0, 0, 0 };
	imageCopy.imageExtent = { uint32_t(width), uint32_t(height), 1 };
	imageCopy.imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };

	VkCopyBufferToImageInfo2 copyInfo{};
	copyInfo.sType = VK_STRUCTURE_TYPE_COPY_BUFFER_TO_IMAGE_INFO_2;
	copyInfo.dstImage = image_;
	copyInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	copyInfo.srcBuffer = buffer;
	copyInfo.regionCount = 1;
	copyInfo.pRegions = &imageCopy;

	vkCmdCopyBufferToImage2(commandBuffer, &copyInfo);
}

VkImage Image::Get() const
{
	return image_;
}

VkImageView Image::GetView() const
{
	return imageView_;
}

VkSampler Image::GetSampler() const
{
	return sampler_;
}

Image::~Image()
{
	vkDestroySampler(device_.device, sampler_, nullptr);
	vkDestroyImageView(device_.device, imageView_, nullptr);
	vmaDestroyImage(device_.allocator, image_, allocation_);
}
