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

void Image::AttachImageView(const VkImageSubresourceRange& range)
{
	assert(image_ && "Image is not initialised!");
	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.image = image_;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.format = format_;
	imageViewCI.subresourceRange = range;

	check(vkCreateImageView(device_.device, &imageViewCI, nullptr, &imageView_));
}

void Image::AttachCubeMapImageView(const VkImageSubresourceRange& range)
{
    assert(image_ && "Image is not initialised!");
    VkImageViewCreateInfo imageViewCI{};
    imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCI.image = image_;
    imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    imageViewCI.format = format_;
    imageViewCI.subresourceRange = range;

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

uint32_t Image::calculateMaxMiplevels(int width, int height)
{
    return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
}

void Image::generateMipmaps(VkImage image, int width, int height, uint32_t mipLevels, VkCommandBuffer commandBuffer)
{
    VkImageMemoryBarrier2 barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier2.image = image;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 1;
    barrier2.subresourceRange.levelCount = 1;

    VkDependencyInfo dependencyInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier2;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier2.subresourceRange.baseMipLevel = i - 1;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier2.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        VkImageBlit2 blit{};
        blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { width, height, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, height > 1 ? height / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.dstImage = image;
        blitInfo.srcImage = image;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blit;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(commandBuffer, &blitInfo);

        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        if (width > 1) width /= 2;
        if (height > 1) height /= 2;
    }

    barrier2.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

void Image::generateCubemapMipmaps(VkImage image, int width, uint32_t mipLevels, VkCommandBuffer commandBuffer)
{
    VkImageMemoryBarrier2 barrier2{};
    barrier2.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier2.image = image;
    barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier2.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier2.subresourceRange.baseArrayLayer = 0;
    barrier2.subresourceRange.layerCount = 6;
    barrier2.subresourceRange.levelCount = 1;

    VkDependencyInfo dependencyInfo{ VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
    dependencyInfo.imageMemoryBarrierCount = 1;
    dependencyInfo.pImageMemoryBarriers = &barrier2;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier2.subresourceRange.baseMipLevel = i - 1;
        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier2.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier2.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);

        VkImageBlit2 blit{};
        blit.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { width, width, 1 };
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 6;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { width > 1 ? width / 2 : 1, width > 1 ? width / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 6;

        VkBlitImageInfo2 blitInfo{};
        blitInfo.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
        blitInfo.dstImage = image;
        blitInfo.srcImage = image;
        blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        blitInfo.regionCount = 1;
        blitInfo.pRegions = &blit;
        blitInfo.filter = VK_FILTER_LINEAR;

        vkCmdBlitImage2(commandBuffer, &blitInfo);

        barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier2.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
        barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
        barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

        vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
        if (width > 1) width /= 2;
    }

    barrier2.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier2.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier2.newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;
    barrier2.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier2.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    barrier2.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    barrier2.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;

    vkCmdPipelineBarrier2(commandBuffer, &dependencyInfo);
}

Image::~Image()
{
	vkDestroySampler(device_.device, sampler_, nullptr);
	vkDestroyImageView(device_.device, imageView_, nullptr);
	vmaDestroyImage(device_.allocator, image_, allocation_);
}
