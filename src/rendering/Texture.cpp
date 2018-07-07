#include "Texture.h"

#include <algorithm>

#include "../../third-party/stb_image/stb_image.h"

#include "Initializers.h"
#include "RenderTools.h"
#include "Renderer.h"

#include "../core/Logger.h"

VulkanTexture::VulkanTexture(VulkanDevice &device)
	: device(device), resource(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {

	readyToUse = std::make_shared<bool>(false);
}
VulkanTexture2D::VulkanTexture2D(VulkanDevice &device)
	: VulkanTexture(device) {}

VulkanTexture2DArray::VulkanTexture2DArray(VulkanDevice &device)
	: VulkanTexture(device) {}

VulkanCubeMap::VulkanCubeMap(VulkanDevice &device) : VulkanTexture(device) {}

VulkanTextureDepthBuffer::VulkanTextureDepthBuffer(VulkanDevice &device)
	: VulkanTexture(device) {}

void VulkanTexture::updateDescriptor() {
	resource.FillResource(textureSampler, textureImageView, textureImageLayout);
	// descriptor.sampler = textureSampler;
	// descriptor.imageView = textureImageView;
	// descriptor.imageLayout = textureImageLayout;
}

void VulkanTexture::destroy() {
	vmaDestroyImage(image.allocator, image.image, image.allocation);

	if (textureImageView != VK_NULL_HANDLE)
		vkDestroyImageView(device.device, textureImageView, nullptr);
	if (textureSampler != VK_NULL_HANDLE)
		vkDestroySampler(device.device, textureSampler, nullptr);
}

void GenerateMipMaps(VkCommandBuffer cmdBuf, VkImage image,
	VkImageLayout finalImageLayout,
	int width, int height, int depth,
	int layers, int mipLevels) {
	// We copy down the whole mip chain doing a blit from mip-1 to mip
	// An alternative way would be to always blit from the first mip level and
	// sample that one down

	// Copy down mips from n-1 to n
	for (int32_t i = 1; i < mipLevels; i++) {

		VkImageBlit imageBlit{};

		// Source
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.layerCount = layers;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcOffsets[1].x = int32_t(width >> (i - 1));
		imageBlit.srcOffsets[1].y = int32_t(height >> (i - 1));
		imageBlit.srcOffsets[1].z = 1;

		// Destination
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.layerCount = layers;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstOffsets[1].x = int32_t(width >> i);
		imageBlit.dstOffsets[1].y = int32_t(height >> i);
		imageBlit.dstOffsets[1].z = 1;

		VkImageSubresourceRange mipSubRange =
			initializers::imageSubresourceRangeCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,
				1, layers);
		mipSubRange.baseMipLevel = i;

		// Transiton current mip level to transfer dest
		SetImageLayout(cmdBuf, image, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mipSubRange,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);

		// Blit from previous level
		vkCmdBlitImage(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit,
			VK_FILTER_LINEAR);

		// Transiton current mip level to transfer source for read in next iteration
		SetImageLayout(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mipSubRange,
			VK_PIPELINE_STAGE_TRANSFER_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT);
	}

	VkImageSubresourceRange subresourceRange =
		initializers::imageSubresourceRangeCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevels, layers);

	// After the loop, all mip layers are in TRANSFER_SRC layout, so transition
	// all to SHADER_READ

	if (finalImageLayout == VK_IMAGE_LAYOUT_UNDEFINED) Log::Debug << "final layout is undefined!\n";
	SetImageLayout(cmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
		finalImageLayout, subresourceRange);

}

VkSampler VulkanTexture::CreateImageSampler(
	VkFilter mag, VkFilter min, VkSamplerMipmapMode mipMapMode,
	VkSamplerAddressMode textureWrapMode, float mipMapLodBias, bool useMipMaps,
	int mipLevels, bool anisotropy, float maxAnisotropy,
	VkBorderColor borderColor) {

	// Create a defaultsampler
	VkSamplerCreateInfo samplerCreateInfo = initializers::samplerCreateInfo();
	samplerCreateInfo.magFilter = mag;
	samplerCreateInfo.minFilter = min;
	samplerCreateInfo.mipmapMode = mipMapMode;
	samplerCreateInfo.addressModeU = textureWrapMode;
	samplerCreateInfo.addressModeV = textureWrapMode;
	samplerCreateInfo.addressModeW = textureWrapMode;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod =
		(useMipMaps) ? mipLevels
		: 0.0f; // Max level-of-detail should match mip level count
	samplerCreateInfo.anisotropyEnable =
		anisotropy; // Enable anisotropic filtering
	samplerCreateInfo.maxAnisotropy =
		(anisotropy)
		? device.physical_device_properties.limits.maxSamplerAnisotropy
		: 1;
	samplerCreateInfo.borderColor = borderColor;

	VkSampler sampler;
	VK_CHECK_RESULT(
		vkCreateSampler(device.device, &samplerCreateInfo, nullptr, &sampler));

	return sampler;
}

VkImageView VulkanTexture::CreateImageView(VkImage image,
	VkImageViewType viewType,
	VkFormat format,
	VkImageAspectFlags aspectFlags,
	VkComponentMapping components,
	int mipLevels, int layers) {
	VkImageViewCreateInfo viewInfo = initializers::imageViewCreateInfo();
	viewInfo.image = image;
	viewInfo.viewType = viewType;
	viewInfo.format = format;
	viewInfo.components = components;
	viewInfo.subresourceRange.aspectMask = aspectFlags;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = mipLevels;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = layers;

	VkImageView imageView;
	VK_CHECK_RESULT(
		vkCreateImageView(device.device, &viewInfo, nullptr, &imageView));

	return imageView;
}

void AlignedTextureMemcpy(int layers, int dst_layer_width, int height,
	int width, int src_row_width, int dst_row_width,
	char *src, char *dst) {

	int offset = 0;
	int texOff = 0;
	for (int i = 0; i < layers; i++) {
		for (int r = 0; r < height; r++) {
			memcpy(dst + offset, src + texOff, width * 4);
			offset += dst_row_width;
			texOff += src_row_width;
		}
		if (texOff < dst_layer_width * i) {
			texOff = dst_layer_width * i;
		}
	}
}

void VulkanTexture::InitImage2D(VkImageCreateInfo imageInfo) {

	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT
		| VK_IMAGE_USAGE_TRANSFER_SRC_BIT
		| VK_IMAGE_USAGE_SAMPLED_BIT;

	VmaAllocationCreateInfo imageAllocCreateInfo = {};
	imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	if (imageInfo.tiling == VK_IMAGE_TILING_OPTIMAL) {
		image.allocator = device.GetImageOptimalAllocator();
	}
	else if (imageInfo.tiling == VK_IMAGE_TILING_LINEAR) {
		image.allocator = device.GetImageLinearAllocator();
	}
	VK_CHECK_RESULT(vmaCreateImage(image.allocator, &imageInfo, &imageAllocCreateInfo, &image.image, &image.allocation, &image.allocationInfo));

}

void VulkanTexture::InitDepthImage(VkImageCreateInfo imageInfo) {
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VmaAllocationCreateInfo imageAllocCreateInfo = {};
	imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

	image.allocator = device.GetImageOptimalAllocator();
	VK_CHECK_RESULT(vmaCreateImage(image.allocator, &imageInfo, &imageAllocCreateInfo, &image.image, &image.allocation, &image.allocationInfo));
}

void VulkanTexture::InitStagingImage2D(VkImageCreateInfo imageInfo) {

	imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VmaAllocationCreateInfo stagingImageAllocCreateInfo = {};
	stagingImageAllocCreateInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
	stagingImageAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

	image.allocator = device.GetGeneralAllocator();
	VK_CHECK_RESULT(vmaCreateImage(image.allocator, &imageInfo, &stagingImageAllocCreateInfo, &image.image, &image.allocation, &image.allocationInfo));

}

void SetLayoutAndTransferRegions(
	VkCommandBuffer transferCmdBuf, VkImage image, VkBuffer stagingBuffer,
	const VkImageSubresourceRange subresourceRange,
	std::vector<VkBufferImageCopy> bufferCopyRegions) {

	SetImageLayout(transferCmdBuf, image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, subresourceRange);

	vkCmdCopyBufferToImage(transferCmdBuf, stagingBuffer, image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		bufferCopyRegions.size(), bufferCopyRegions.data());

	SetImageLayout(transferCmdBuf, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, subresourceRange);
}

// void CleanBuffers(std::vector<VulkanBuffer> buffers) {
// 	for (auto& buf : buffers) {
// 		buf.CleanBuffer();

// 	}
// }

void BeginTransferAndMipMapGenWork(
	VulkanRenderer & renderer,
	std::shared_ptr<VulkanBuffer> buffer,
	const VkImageSubresourceRange subresourceRange,
	const std::vector<VkBufferImageCopy> bufferCopyRegions,
	VkImageLayout imageLayout,
	VkImage image,
	VkBuffer vk_buffer,
	int width,
	int height,
	Signal signal,
	int layers,
	int mipLevels)
{
	if (renderer.device.singleQueueDevice) {
		std::function<void(const VkCommandBuffer)> work =
			[=](const VkCommandBuffer cmdBuf) {
			SetLayoutAndTransferRegions(cmdBuf, image, vk_buffer,
				subresourceRange, bufferCopyRegions);

			GenerateMipMaps(cmdBuf, image, imageLayout, width, height, 1, layers, mipLevels);
		};

		renderer.SubmitGraphicsWork(work, {}, {}, {buffer}, { std::move(signal) });
	}
	else {
		VulkanSemaphore sem(renderer.device);

		std::function<void(const VkCommandBuffer)> transferWork =
			[=](const VkCommandBuffer cmdBuf) {
			SetLayoutAndTransferRegions(cmdBuf, image, vk_buffer,
				subresourceRange, bufferCopyRegions);
		};

		std::function<void(const VkCommandBuffer)> mipMapGenWork =
			[=](const VkCommandBuffer cmdBuf) {
			GenerateMipMaps(cmdBuf, image, imageLayout, width, height, 1, layers, mipLevels);
		};

		renderer.SubmitTransferWork(transferWork, {}, { sem },  {buffer}, {});

		renderer.SubmitGraphicsWork(mipMapGenWork, { sem }, {}, {}, { signal });

	}
}

void VulkanTexture2D::loadFromTexture(std::shared_ptr<Texture> texture,
	VkFormat format, VulkanRenderer &renderer,
	VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout,
	bool forceLinear, bool genMipMaps,
	int mipMapLevelsToGen, bool wrapBorder) {
	// VmaImage stagingImage;

	this->texture = texture;
	int maxMipMapLevelsPossible =
		(int)floor(log2(std::max(texture->width, texture->height))) + 1;
	this->mipLevels = genMipMaps ? mipMapLevelsToGen : 1;
	this->textureImageLayout = imageLayout;
	this->layers = 1;

	// Get device properites for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(device.physical_device, format,
		&formatProperties);

	// Mip-chain generation requires support for blit source and destination
	assert(formatProperties.optimalTilingFeatures &
		VK_FORMAT_FEATURE_BLIT_SRC_BIT);
	assert(formatProperties.optimalTilingFeatures &
		VK_FORMAT_FEATURE_BLIT_DST_BIT);

	// Only use linear tiling if requested (and supported by the device)
	// Support for linear tiling is mostly limited, so prefer to use
	// optimal tiling instead
	// On most implementations linear tiling will only support a very
	// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
	VkBool32 useStaging = !forceLinear;

	VkExtent3D imageExtent = { texture->width, texture->height, 1 };

	VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo(
		VK_IMAGE_TYPE_2D, format, (uint32_t)mipLevels, (uint32_t)1,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_SHARING_MODE_EXCLUSIVE,
		VK_IMAGE_LAYOUT_UNDEFINED, imageExtent,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT);


	auto buffer = std::make_shared<VulkanBufferStagingResource>(
		device, texture->texImageSize, texture->pixels.data());

	/*buffer.CreateStagingResourceBuffer(texture->pixels.data(),
		texture->texImageSize);*/

	InitImage2D(imageCreateInfo);

	VkImageSubresourceRange subresourceRange =
		initializers::imageSubresourceRangeCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevels, 1);

	std::vector<VkBufferImageCopy> bufferCopyRegions;

	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texture->width);
	bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texture->height);
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;
	bufferCopyRegions.push_back(bufferCopyRegion);
	// Increase offset into staging buffer for next level / face

	BeginTransferAndMipMapGenWork(renderer, buffer, subresourceRange, bufferCopyRegions, 
	imageLayout, image.image, buffer->buffer.buffer,
		texture->width, texture->height, readyToUse, layers, mipLevels);


	// With mip mapping and anisotropic filtering
	textureSampler = CreateImageSampler(
		VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
		wrapBorder ? VK_SAMPLER_ADDRESS_MODE_REPEAT
		: VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
		0.0f, true, mipLevels,
		device.physical_device_features.samplerAnisotropy ? true : false, 8,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

	textureImageView = CreateImageView(
		image.image, VK_IMAGE_VIEW_TYPE_2D, format, VK_IMAGE_ASPECT_COLOR_BIT,
		VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
						   VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		(useStaging) ? mipLevels : 1, 1);

	updateDescriptor();

}

void VulkanTexture2DArray::loadTextureArray(
	std::shared_ptr<TextureArray> textures, VkFormat format,
	VulkanRenderer &renderer, VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout, bool genMipMaps, int mipMapLevelsToGen) {


	this->textures = textures;
	this->mipLevels = genMipMaps ? mipMapLevelsToGen : 1;
	this->textureImageLayout = imageLayout;
	this->layers = textures->layerCount;

	VkExtent3D imageExtent = { textures->width, textures->height, 1 };

	VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo(
		VK_IMAGE_TYPE_2D, format, (uint32_t)mipLevels,
		(uint32_t)layers, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL, VK_SHARING_MODE_EXCLUSIVE,
		VK_IMAGE_LAYOUT_UNDEFINED, imageExtent,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT);


	auto buffer = std::make_shared<VulkanBufferStagingResource>(
		device, textures->texImageSize, textures->pixels.data());

	//buffer.CreateStagingResourceBuffer(textures->pixels.data(),
	//	textures->texImageSize);

	InitImage2D(imageCreateInfo);

	VkImageSubresourceRange subresourceRange =
		initializers::imageSubresourceRangeCreateInfo(
			VK_IMAGE_ASPECT_COLOR_BIT, mipLevels, layers);

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	size_t offset = 0;

	for (uint32_t layer = 0; layer < layers; layer++) {
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(textures->width);
		bufferCopyRegion.imageExtent.height =
			static_cast<uint32_t>(textures->height);
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = offset;
		bufferCopyRegions.push_back(bufferCopyRegion);
		// Increase offset into staging buffer for next level / face
		offset += textures->texImageSizePerTex;
	}

	BeginTransferAndMipMapGenWork(renderer, buffer, subresourceRange, bufferCopyRegions, 
		imageLayout, image.image, buffer->buffer.buffer,
		textures->width, textures->height, readyToUse, layers, mipLevels);

	textureSampler = CreateImageSampler(
		VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_REPEAT, 0.0f, true, mipLevels, true, 8,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

	textureImageView = CreateImageView(
		image.image, VK_IMAGE_VIEW_TYPE_2D_ARRAY, format,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
						   VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		mipLevels, layers);


	updateDescriptor();
}

void VulkanCubeMap::loadFromTexture(std::shared_ptr<CubeMap> cubeMap,
	VkFormat format, VulkanRenderer &renderer,
	VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout, bool genMipMaps,
	int mipMapLevelsToGen)
{
	this->cubeMap = cubeMap;
	this->mipLevels = genMipMaps ? mipMapLevelsToGen : 1;
	this->textureImageLayout = imageLayout;


	VkExtent3D imageExtent = { cubeMap->width, cubeMap->height, 1 };

	VkImageCreateInfo imageCreateInfo = initializers::imageCreateInfo(
		VK_IMAGE_TYPE_2D, format, (uint32_t)mipLevels, (uint32_t)6,
		VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, VK_SHARING_MODE_EXCLUSIVE,
		VK_IMAGE_LAYOUT_UNDEFINED, imageExtent,
		VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
		VK_IMAGE_USAGE_SAMPLED_BIT);
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;

	InitImage2D(imageCreateInfo);

	auto buffer = std::make_shared<VulkanBufferStagingResource>(
		device, cubeMap->texImageSize, cubeMap->pixels.data());

	//buffer.CreateStagingResourceBuffer(cubeMap->pixels.data(),
	//	cubeMap->texImageSize);

	VkImageSubresourceRange subresourceRange =
		initializers::imageSubresourceRangeCreateInfo(VK_IMAGE_ASPECT_COLOR_BIT,
			mipLevels, 6);

	std::vector<VkBufferImageCopy> bufferCopyRegions;
	size_t offset = 0;

	for (uint32_t layer = 0; layer < 6; layer++) {
		VkBufferImageCopy bufferCopyRegion = {};
		bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		bufferCopyRegion.imageSubresource.mipLevel = 0;
		bufferCopyRegion.imageSubresource.baseArrayLayer = layer;
		bufferCopyRegion.imageSubresource.layerCount = 1;
		bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(cubeMap->width);
		bufferCopyRegion.imageExtent.height =
			static_cast<uint32_t>(cubeMap->height);
		bufferCopyRegion.imageExtent.depth = 1;
		bufferCopyRegion.bufferOffset = offset;
		bufferCopyRegions.push_back(bufferCopyRegion);
		// Increase offset into staging buffer for next level / face
		offset += cubeMap->texImageSizePerTex;
	}

	BeginTransferAndMipMapGenWork(renderer, buffer, subresourceRange, 
		bufferCopyRegions, imageLayout, image.image, buffer->buffer.buffer,
		cubeMap->width, cubeMap->height, readyToUse, layers, mipLevels);

	textureSampler = CreateImageSampler(
		VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_MIPMAP_MODE_LINEAR,
		VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, 0.0f, true, mipLevels, true, 8,
		VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE);

	textureImageView = CreateImageView(
		image.image, VK_IMAGE_VIEW_TYPE_CUBE, format, VK_IMAGE_ASPECT_COLOR_BIT,
		VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
						   VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		mipLevels, 6);

	updateDescriptor();
}

void VulkanTextureDepthBuffer::CreateDepthImage(VulkanRenderer &renderer,
	VkFormat depthFormat, int width,
	int height) {

	VkImageCreateInfo imageInfo = initializers::imageCreateInfo(
		VK_IMAGE_TYPE_2D, depthFormat, 1, 1, VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_TILING_OPTIMAL, VK_SHARING_MODE_EXCLUSIVE,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VkExtent3D{ (uint32_t)width, (uint32_t)height, (uint32_t)1 },
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);

	InitDepthImage(imageInfo);

	textureImageView = VulkanTexture::CreateImageView(
		image.image, VK_IMAGE_VIEW_TYPE_2D, depthFormat,
		VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
		VkComponentMapping{ VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G,
						   VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A },
		1, 1);


	VkImageSubresourceRange subresourceRange =
		initializers::imageSubresourceRangeCreateInfo(
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);

	VkCommandBuffer cmdBuf = renderer.GetGraphicsCommandBuffer();

	SetImageLayout(cmdBuf, image.image, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		subresourceRange);

	renderer.SubmitGraphicsCommandBufferAndWait(cmdBuf);

}

VulkanTextureManager::VulkanTextureManager(VulkanDevice &device)
	: device(device) {}

VulkanTextureManager::~VulkanTextureManager() {
	for (auto& tex : vulkanTextures) {
		tex.destroy();
	}
}

std::shared_ptr<VulkanTexture2D> VulkanTextureManager::CreateTexture2D(
	std::shared_ptr<Texture> texture, VkFormat format, VulkanRenderer &renderer,
	VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout,
	bool forceLinear, bool genMipMaps, int mipMapLevelsToGen, bool wrapBorder) {
	return std::make_shared<VulkanTexture2D>(device);
}

std::shared_ptr<VulkanTexture2DArray>
VulkanTextureManager::CreateTexture2DArray(
	std::shared_ptr<TextureArray> textures, VkFormat format,
	VulkanRenderer &renderer, VkImageUsageFlags imageUsageFlags,
	VkImageLayout imageLayout, bool genMipMaps, int mipMapLevelsToGen) {
	return std::make_shared<VulkanTexture2DArray>(device);
}

std::shared_ptr<VulkanCubeMap> VulkanTextureManager::CreateCubeMap(
	std::shared_ptr<CubeMap> cubeMap, VkFormat format, VulkanRenderer &renderer,
	VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout,
	bool genMipMaps, int mipMapLevelsToGen) {
	return std::make_shared<VulkanCubeMap>(device);
}
std::shared_ptr<VulkanTextureDepthBuffer>
VulkanTextureManager::CreateDepthImage(VulkanRenderer &renderer,
	VkFormat depthFormat, int width,
	int height) {
	return std::make_shared<VulkanTextureDepthBuffer>(device);
}
