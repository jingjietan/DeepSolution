#include "Image.h"
#include "Common.h"

#include <volk.h>

#include <cassert>

Image::Image(VkDevice device, VmaAllocator allocator, const VkImageCreateInfo& imageInfo): device_(device), allocator_(allocator), format_(imageInfo.format)
{
	VmaAllocationCreateInfo allocCI{};
	allocCI.usage = VMA_MEMORY_USAGE_AUTO;

	check(vmaCreateImage(allocator, &imageInfo, &allocCI, &image_, &allocation_, nullptr));
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

	check(vkCreateImageView(device_, &imageViewCI, nullptr, &imageView_));
}

void Image::AttachSampler(const VkSamplerCreateInfo& samplerCI)
{
	assert(image_ && "Image is not initialised!");
	check(vkCreateSampler(device_, &samplerCI, nullptr, &sampler_));
}

VkImage Image::Get() const
{
	return image_;
}

void Image::Release()
{
	vkDestroySampler(device_, sampler_, nullptr);
	vkDestroyImageView(device_, imageView_, nullptr);
	vmaDestroyImage(allocator_, image_, allocation_);
}
