#include "VulkanApp.h"


#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "VulkanInitializers.hpp"

VulkanApp::VulkanApp()
{
	camera = new Camera(glm::vec3(-2,2,0), glm::vec3(0,1,0), 0, -45);

	initWindow();
	initVulkan();
	prepareScene();
	PrepareImGui();

	startTime = std::chrono::high_resolution_clock::now();
	mainLoop();
	cleanup(); //all resources
}


VulkanApp::~VulkanApp()
{
}

static void onWindowResized(GLFWwindow* window, int width, int height) {
	if (width == 0 || height == 0) return;

	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->recreateSwapChain();
}

void onMouseMoved(GLFWwindow* window, double xpos, double ypos)
{
	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->MouseMoved(xpos, ypos);
}

void onMouseClicked(GLFWwindow* window, int button, int action, int mods) {
	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->MouseClicked(button, action, mods);
	ImGui_ImplGlfwVulkan_MouseButtonCallback(window, button, action, mods);
}

void onKeyboardEvent(GLFWwindow* window, int key, int scancode, int action, int mods) {
	VulkanApp* app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
	app->KeyboardEvent(key, scancode, action, mods);
	ImGui_ImplGlfwVulkan_KeyCallback(window, key, scancode, action, mods);
}

void VulkanApp::initWindow() {
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	vulkanDevice.window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

	glfwSetWindowUserPointer(vulkanDevice.window, this);
	glfwSetWindowSizeCallback(vulkanDevice.window, onWindowResized);
	glfwSetCursorPosCallback(vulkanDevice.window, onMouseMoved);
	glfwSetMouseButtonCallback(vulkanDevice.window, onMouseClicked);
	glfwSetKeyCallback(vulkanDevice.window, onKeyboardEvent);
	SetMouseControl(true);
}

void VulkanApp::initVulkan() {
	vulkanDevice.initVulkanDevice(vulkanSwapChain.surface);

	vulkanSwapChain.initSwapChain(vulkanDevice.instance, vulkanDevice.physical_device, vulkanDevice.device, vulkanDevice.window);

	//createImageViews();
	createRenderPass();

	createDepthResources();
	createFramebuffers();

	createCommandBuffers();
}

void VulkanApp::prepareScene(){

	pointLights.resize(5);
	pointLights[0] = PointLight(glm::vec4(0, 10, 0, 1),	  glm::vec4(0, 0, 0, 0), glm::vec4(1.0, 0.045f, 0.0075f, 1.0f));
	pointLights[1] = PointLight(glm::vec4(10, 10, 50, 1), glm::vec4(0, 0, 0, 0), glm::vec4(1.0, 0.045f, 0.0075f, 1.0f));
	pointLights[2] = PointLight(glm::vec4(50, 10, 10, 1), glm::vec4(0, 0, 0, 0), glm::vec4(1.0, 0.045f, 0.0075f, 1.0f));
	pointLights[3] = PointLight(glm::vec4(50, 10, 50, 1), glm::vec4(0, 0, 0, 0), glm::vec4(1.0, 0.045f, 0.0075f, 1.0f));
	pointLights[4] = PointLight(glm::vec4(75, 10, 75, 1), glm::vec4(0, 0, 0, 0), glm::vec4(1.0, 0.045f, 0.0075f, 1.0f));

	createUniformBuffers();
	createDescriptorSets();

	skybox = new Skybox();
	skybox->InitSkybox(&vulkanDevice, "Resources/Textures/Skybox/Skybox2", ".png", renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height);
	
	cubeObject = new GameObject();
	cubeObject->LoadModel(createCube());
	cubeObject->LoadTexture("Resources/Textures/ColorGradientCube.png");
	cubeObject->InitGameObject(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer);

	RecreateTerrain();
	createSemaphores();
}

void VulkanApp::RecreateTerrain() {
	//free resources then delete all created terrains/waters
	for (Terrain* ter : terrains) {
		ter->CleanUp();
	}
	terrains.clear();
	for (Water* water : waters) {
		water->CleanUp();
	}
	waters.clear();

	for (int i = 0; i < terrainGridDimentions; i++){ //creates a grid of terrains centered around 0,0,0
		for (int j = 0; j < terrainGridDimentions; j++)	{
			terrains.push_back(new Terrain(100, terrainMaxLevels, (i - terrainGridDimentions / 2) * terrainWidth - terrainWidth / 2, (j - terrainGridDimentions / 2) * terrainWidth - terrainWidth / 2, terrainWidth, terrainWidth));
		}
	}

	for (Terrain* ter : terrains) {
		ter->InitTerrain(&vulkanDevice, renderPass, vulkanDevice.graphics_queue, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer, camera->Position);
	}

	for (int i = 0; i < terrainGridDimentions; i++) {
		for (int j = 0; j < terrainGridDimentions; j++)	{
			waters.push_back(new Water(200, (i - terrainGridDimentions / 2) * terrainWidth - terrainWidth / 2, (j - terrainGridDimentions / 2) * terrainWidth - terrainWidth / 2, terrainWidth, terrainWidth));
		}
	}

	for (Water* water : waters) {
		water->LoadTexture("Resources/Textures/TileableWaterTexture.jpg");
		water->InitWater(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer);
	}

	recreateTerrain = false;
}

void VulkanApp::mainLoop() {
	while (!glfwWindowShouldClose(vulkanDevice.window)) {
		frameTimer.StartTimer();


		glfwPollEvents();
		HandleInputs();

		updateScene();
		BuildImgui();

		buildCommandBuffer(frameIndex);
		drawFrame();
		
		frameTimer.EndTimer();
		
		deltaTime = frameTimer.GetElapsedTimeNanoSeconds() / 1.0e9;
		timeSinceStart = std::chrono::duration_cast<std::chrono::microseconds>(frameTimer.GetEndTime() - startTime).count() / 1.0e6;

		// Update frame timings display
		{
			std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end());
			double frameTime = 1000.0f / (deltaTime * 1000.0f);
			frameTimes.back() = frameTime;
			if (frameTime < frameTimeMin) {
				frameTimeMin = frameTime;
			}
			if (frameTime > frameTimeMax) {
				frameTimeMax = frameTime;
			}
		}
	}

	vkDeviceWaitIdle(vulkanDevice.device);
}

void VulkanApp::cleanup() {
	CleanUpImgui();

	skybox->CleanUp();
	cubeObject->CleanUp();
	for (Terrain* ter : terrains) {
		ter->CleanUp();
	}
	for (Water* water : waters) {
		water->CleanUp();
	}
	cleanupSwapChain();

	globalVariableBuffer.cleanBuffer();
	lightsInfoBuffer.cleanBuffer();
	
	vkDestroySemaphore(vulkanDevice.device, renderFinishedSemaphore, nullptr);
	vkDestroySemaphore(vulkanDevice.device, imageAvailableSemaphore, nullptr);

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(vulkanDevice.device, shaderModule, nullptr);
	}
	
	vulkanDevice.cleanup(vulkanSwapChain.surface);
}

void VulkanApp::cleanupSwapChain() {
	

	vkDestroyImageView(vulkanDevice.device, depthImageView, nullptr);
	vkDestroyImage(vulkanDevice.device, depthImage, nullptr);
	vkFreeMemory(vulkanDevice.device, depthImageMemory, nullptr);

	for (size_t i = 0; i < vulkanSwapChain.swapChainFramebuffers.size(); i++) {
		vkDestroyFramebuffer(vulkanDevice.device, vulkanSwapChain.swapChainFramebuffers[i], nullptr);
	}

	vkDestroyRenderPass(vulkanDevice.device, renderPass, nullptr);

	for (size_t i = 0; i < vulkanSwapChain.swapChainImageViews.size(); i++) {
		vkDestroyImageView(vulkanDevice.device, vulkanSwapChain.swapChainImageViews[i], nullptr);
	}

	vkDestroySwapchainKHR(vulkanDevice.device, vulkanSwapChain.swapChain, nullptr);
}

void VulkanApp::recreateSwapChain() {
	vkDeviceWaitIdle(vulkanDevice.device);

	cleanupSwapChain();

	vulkanSwapChain.recreateSwapChain(vulkanDevice.window);
	
	createRenderPass();
	createDepthResources();
	createFramebuffers();
	
	skybox->ReinitSkybox(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height);
	cubeObject->ReinitGameObject(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer);
	
	for (Terrain* ter : terrains) {
		ter->ReinitTerrain(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer);
	}
	for (Water* water : waters) {
		water->ReinitWater(&vulkanDevice, renderPass, vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, globalVariableBuffer, lightsInfoBuffer);
	}

	frameIndex = 1; //cause it needs it to be synced back to zero (yes I know it says one, thats intended, build command buffers uses the "next" frame index since it has to sync with the swapchain so it starts at one....)
	reBuildCommandBuffers();
}

void VulkanApp::reBuildCommandBuffers() {
	vkFreeCommandBuffers(vulkanDevice.device, vulkanDevice.graphics_queue_command_pool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
	vkResetCommandPool(vulkanDevice.device, vulkanDevice.graphics_queue_command_pool, 0);

	createCommandBuffers();
	buildCommandBuffer(frameIndex);
}


//8
void VulkanApp::createRenderPass() {
	VkAttachmentDescription colorAttachment = {};
	colorAttachment.format = vulkanSwapChain.swapChainImageFormat;
	colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentDescription depthAttachment = {};
	depthAttachment.format = findDepthFormat();
	depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
	depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorAttachmentRef = {};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthAttachmentRef = {};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorAttachmentRef;
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	VkSubpassDependency dependency = {};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.srcAccessMask = 0;
	dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

	std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
	VkRenderPassCreateInfo renderPassInfo = initializers::renderPassCreateInfo();
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;

	if (vkCreateRenderPass(vulkanDevice.device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
		throw std::runtime_error("failed to create render pass!");
	}
}

//11
void VulkanApp::createDepthResources() {
	VkFormat depthFormat = findDepthFormat();

	createImage(vulkanSwapChain.swapChainExtent.width, vulkanSwapChain.swapChainExtent.height, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);
	depthImageView = createImageView(vulkanDevice.device, depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

	transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat VulkanApp::findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
	for (VkFormat format : candidates) {
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(vulkanDevice.physical_device, format, &props);

		if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
			return format;
		}
	}

	throw std::runtime_error("failed to find supported format!");
}

VkFormat VulkanApp::findDepthFormat() {
	return findSupportedFormat(
	{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
		VK_IMAGE_TILING_OPTIMAL,
		VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
	);
}

bool VulkanApp::hasStencilComponent(VkFormat format) {
	return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
}

//12
void VulkanApp::createFramebuffers() {
	vulkanSwapChain.swapChainFramebuffers.resize(vulkanSwapChain.swapChainImageViews.size());

	for (size_t i = 0; i < vulkanSwapChain.swapChainImageViews.size(); i++) {
		std::array<VkImageView, 2> attachments = {
			vulkanSwapChain.swapChainImageViews[i],
			depthImageView
		};

		VkFramebufferCreateInfo framebufferInfo = initializers::framebufferCreateInfo();
		framebufferInfo.renderPass = renderPass;
		framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		framebufferInfo.pAttachments = attachments.data();
		framebufferInfo.width = vulkanSwapChain.swapChainExtent.width;
		framebufferInfo.height = vulkanSwapChain.swapChainExtent.height;
		framebufferInfo.layers = 1;

		if (vkCreateFramebuffer(vulkanDevice.device, &framebufferInfo, nullptr, &vulkanSwapChain.swapChainFramebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to create framebuffer!");
		}
	}
}

//13
void VulkanApp::createTextureImage(VkImage image, VkDeviceMemory imageMemory) {
	int texWidth, texHeight, texChannels;
	stbi_uc* pixels = stbi_load("Resources/Textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	VkDeviceSize imageSize = texWidth * texHeight * 4;

	if (!pixels) {
		throw std::runtime_error("failed to load texture image!");
	}

	VkBuffer stagingBuffer;
	VkDeviceMemory stagingBufferMemory;
	createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer, stagingBufferMemory);

	void* data;
	vkMapMemory(vulkanDevice.device, stagingBufferMemory, 0, imageSize, 0, &data);
	memcpy(data, pixels, static_cast<size_t>(imageSize));
	vkUnmapMemory(vulkanDevice.device, stagingBufferMemory);

	stbi_image_free(pixels);

	createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, image, imageMemory);

	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_PREINITIALIZED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
	copyBufferToImage(stagingBuffer, image, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
	transitionImageLayout(image, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	vkDestroyBuffer(vulkanDevice.device, stagingBuffer, nullptr);
	vkFreeMemory(vulkanDevice.device, stagingBufferMemory, nullptr);
}

//15
void VulkanApp::createTextureSampler(VkSampler* textureSampler) {
	VkSamplerCreateInfo samplerInfo = initializers::samplerCreateInfo();
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerInfo.anisotropyEnable = VK_TRUE;
	samplerInfo.maxAnisotropy = 16;
	samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerInfo.unnormalizedCoordinates = VK_FALSE;
	samplerInfo.compareEnable = VK_FALSE;
	samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

	if (vkCreateSampler(vulkanDevice.device, &samplerInfo, nullptr, textureSampler) != VK_SUCCESS) {
		throw std::runtime_error("failed to create texture sampler!");
	}
}

void VulkanApp::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
	VkImageCreateInfo imageInfo = initializers::imageCreateInfo();
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = width;
	imageInfo.extent.height = height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = format;
	imageInfo.tiling = tiling;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageInfo.usage = usage;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateImage(vulkanDevice.device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("failed to create image!");
	}

	VkMemoryRequirements memRequirements;
	vkGetImageMemoryRequirements(vulkanDevice.device, image, &memRequirements);

	VkMemoryAllocateInfo allocInfo = initializers::memoryAllocateInfo();
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(vulkanDevice.physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(vulkanDevice.device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate image memory!");
	}

	vkBindImageMemory(vulkanDevice.device, image, imageMemory, 0);
}

void VulkanApp::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkImageMemoryBarrier barrier = initializers::imageMemoryBarrier();
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;

	if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

		if (hasStencilComponent(format)) {
			barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
		}
	}
	else {
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	}

	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;

	if (oldLayout == VK_IMAGE_LAYOUT_PREINITIALIZED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_HOST_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	}
	else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
		barrier.srcAccessMask = 0;
		barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}
	else {
		throw std::invalid_argument("unsupported layout transition!");
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

	endSingleTimeCommands(commandBuffer);
}

void VulkanApp::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferImageCopy region = {};
	region.bufferOffset = 0;
	region.bufferRowLength = 0;
	region.bufferImageHeight = 0;
	region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	region.imageSubresource.mipLevel = 0;
	region.imageSubresource.baseArrayLayer = 0;
	region.imageSubresource.layerCount = 1;
	region.imageOffset = { 0, 0, 0 };
	region.imageExtent = {width, height, 1};

	vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

	endSingleTimeCommands(commandBuffer);
}

//17
void VulkanApp::createUniformBuffers() {
	vulkanDevice.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (VkMemoryPropertyFlags)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), &globalVariableBuffer, sizeof(GlobalVariableUniformBuffer));
	vulkanDevice.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, (VkMemoryPropertyFlags)(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT), &lightsInfoBuffer, sizeof(PointLight) * pointLights.size());

	for (int i = 0; i < pointLights.size(); i++)
	{
		PointLight lbo;
		lbo.lightPos = pointLights[i].lightPos;
		lbo.color = pointLights[i].color;
		lbo.attenuation = pointLights[i].attenuation;

		lightsInfoBuffer.map(vulkanDevice.device, sizeof(PointLight), i * sizeof(PointLight));
		lightsInfoBuffer.copyTo(&lbo, sizeof(lbo));
		lightsInfoBuffer.unmap();
	}
}

//19
void VulkanApp::createDescriptorSets() {
	globalVariableBuffer.setupDescriptor();
	lightsInfoBuffer.setupDescriptor();
}

void VulkanApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
	VkBufferCreateInfo bufferInfo = initializers::bufferCreateInfo(usage,size);
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	if (vkCreateBuffer(vulkanDevice.device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("failed to create buffer!");
	}

	VkMemoryRequirements memRequirements;
	vkGetBufferMemoryRequirements(vulkanDevice.device, buffer, &memRequirements);

	VkMemoryAllocateInfo allocInfo = initializers::memoryAllocateInfo();
	allocInfo.allocationSize = memRequirements.size;
	allocInfo.memoryTypeIndex = findMemoryType(vulkanDevice.physical_device, memRequirements.memoryTypeBits, properties);

	if (vkAllocateMemory(vulkanDevice.device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate buffer memory!");
	}

	vkBindBufferMemory(vulkanDevice.device, buffer, bufferMemory, 0);
}

VkCommandBuffer VulkanApp::beginSingleTimeCommands() {
	VkCommandBufferAllocateInfo allocInfo = initializers::commandBufferAllocateInfo(vulkanDevice.graphics_queue_command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);

	VkCommandBuffer commandBuffer;
	vkAllocateCommandBuffers(vulkanDevice.device, &allocInfo, &commandBuffer);

	VkCommandBufferBeginInfo beginInfo = {};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(commandBuffer, &beginInfo);

	return commandBuffer;
}

void VulkanApp::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
	vkEndCommandBuffer(commandBuffer);

	VkSubmitInfo submitInfo = initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vkQueueSubmit(vulkanDevice.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE);
	vkQueueWaitIdle(vulkanDevice.graphics_queue);

	vkFreeCommandBuffers(vulkanDevice.device, vulkanDevice.graphics_queue_command_pool, 1, &commandBuffer);
}

void VulkanApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
	VkCommandBuffer commandBuffer = beginSingleTimeCommands();

	VkBufferCopy copyRegion = {};
	copyRegion.size = size;
	vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

	endSingleTimeCommands(commandBuffer);
}


//20
void VulkanApp::createCommandBuffers() {

	commandBuffers.resize(vulkanSwapChain.swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = initializers::commandBufferAllocateInfo(vulkanDevice.graphics_queue_command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)commandBuffers.size());

	if (vkAllocateCommandBuffers(vulkanDevice.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}

}

void VulkanApp::buildCommandBuffer(uint32_t index){
	//std::cout << frameIndex << std::endl;

	//for (size_t i = 0; i < commandBuffers.size(); i++) {
		
	int i = (index + 1) % 2;
	VkCommandBufferBeginInfo beginInfo = initializers::commandBufferBeginInfo();
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

	vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

	VkRenderPassBeginInfo renderPassInfo = initializers::renderPassBeginInfo();
	renderPassInfo.renderPass = renderPass;
	renderPassInfo.framebuffer = vulkanSwapChain.swapChainFramebuffers[i];
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = vulkanSwapChain.swapChainExtent;

	std::array<VkClearValue, 2> clearValues = {};
	clearValues[0].color = { 0.2f, 0.3f, 0.3f, 1.0f };
	clearValues[1].depthStencil = { 1.0f, 0 };

	renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
	renderPassInfo.pClearValues = clearValues.data();

	vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	VkDeviceSize offsets[] = { 0 };

	//terrain mesh
	for (Terrain* ter : terrains) {
		ter->DrawTerrain(commandBuffers, (int)i, offsets, ter, wireframe);
	}



	//water
	vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? waters.at(0)->wireframe : waters.at(0)->pipeline);
	vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &waters.at(0)->WaterModel.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffers[i], waters.at(0)->WaterModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	
	for (Water* water : waters) {
		vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, water->pipelineLayout, 0, 1, &water->descriptorSet, 0, nullptr);

		vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(water->WaterModel.indexCount), 1, 0, 0, 0);
	}

	//cubeObject
	vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, cubeObject->pipelineLayout, 0, 1, &cubeObject->descriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, wireframe ? cubeObject->wireframe : cubeObject->pipeline);

	vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &cubeObject->gameObjectModel.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffers[i], cubeObject->gameObjectModel.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(cubeObject->gameObjectModel.indexCount), 1, 0, 0, 0);


	//skybox
	vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, skybox->pipelineLayout, 0, 1, &skybox->descriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, skybox->pipeline);

	vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &skybox->model.vertices.buffer, offsets);
	vkCmdBindIndexBuffer(commandBuffers[i], skybox->model.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(skybox->model.indexCount), 1, 0, 0, 0);


	//Imgui rendering
	ImGui_ImplGlfwVulkan_Render(commandBuffers[i]);

	vkCmdEndRenderPass(commandBuffers[i]);

	if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
		throw std::runtime_error("failed to record command buffer!");
	}
	
}

void VulkanApp::CreatePrimaryCommandBuffer() {
	commandBuffers.resize(vulkanSwapChain.swapChainFramebuffers.size());

	VkCommandBufferAllocateInfo allocInfo = initializers::commandBufferAllocateInfo(vulkanDevice.graphics_queue_command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, (uint32_t)commandBuffers.size());

	if (vkAllocateCommandBuffers(vulkanDevice.device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("failed to allocate command buffers!");
	}
	
	for (size_t i = 0; i < commandBuffers.size(); i++) {
		VkCommandBufferBeginInfo beginInfo = initializers::commandBufferBeginInfo();
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

		std::array<VkClearValue, 2> clearValues = {};
		clearValues[0].color = { 0.2f, 0.3f, 0.3f, 1.0f };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassInfo = initializers::renderPassBeginInfo();
		renderPassInfo.renderPass = renderPass;
		renderPassInfo.framebuffer = vulkanSwapChain.swapChainFramebuffers[i];
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = vulkanSwapChain.swapChainExtent;
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

		vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

		

		vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		
		//Render stuff

		// Contains the list of secondary command buffers to be executed
		std::vector<VkCommandBuffer> commandBuffers;



		vkCmdExecuteCommands(commandBuffers[i], commandBuffers.size(), commandBuffers.data());
		
		vkCmdEndRenderPass(commandBuffers[i]);

		if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("failed to record command buffer!");
		}
	}

}

//21
void VulkanApp::createSemaphores() {
	VkSemaphoreCreateInfo semaphoreInfo = initializers::semaphoreCreateInfo();

	if (vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
		vkCreateSemaphore(vulkanDevice.device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS) {

		throw std::runtime_error("failed to create semaphores!");
	}
}

VkPipelineShaderStageCreateInfo VulkanApp::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;

	shaderStage.module = loadShaderModule(vulkanDevice.device, fileName.c_str());

	shaderStage.pName = "main"; 
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

static void imgui_check_vk_result(VkResult err)
{
	if (err == 0) return;
	printf("VkResult %d\n", err);
	if (err < 0)
		abort();
}

void  VulkanApp::PrepareImGui()
{
	//Creates a descriptor pool for imgui
	{	VkDescriptorPoolSize pool_size[11] =
		{
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
			{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
			{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
		};
		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 1000 * 11;
		pool_info.poolSizeCount = 11;
		pool_info.pPoolSizes = pool_size;
		VK_CHECK_RESULT(vkCreateDescriptorPool(vulkanDevice.device, &pool_info, VK_NULL_HANDLE, &imgui_descriptor_pool));
	}

	ImGui_ImplGlfwVulkan_Init_Data init_data = {};
	init_data.allocator = VK_NULL_HANDLE;
	init_data.gpu = vulkanDevice.physical_device;
	init_data.device = vulkanDevice.device;
	init_data.render_pass = renderPass;
	init_data.pipeline_cache = VK_NULL_HANDLE;
	init_data.descriptor_pool = imgui_descriptor_pool;
	init_data.check_vk_result = imgui_check_vk_result;
	
	ImGui_ImplGlfwVulkan_Init(vulkanDevice.window, false, &init_data);

	VkCommandBuffer fontUploader = vulkanDevice.createCommandBuffer(vulkanDevice.graphics_queue_command_pool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	ImGui_ImplGlfwVulkan_CreateFontsTexture(fontUploader);
	vulkanDevice.flushCommandBuffer(fontUploader, vulkanDevice.graphics_queue, true);
}

// Build imGui windows and elements
void VulkanApp::BuildImgui() {
	ImGui_ImplGlfwVulkan_NewFrame();
	
	bool show_test_window = true;

	//Application Debuf info
	{
		ImGui::Begin("Application Debug Information", &show_test_window);
		static float f = 0.0f;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		ImGui::Text("Delta Time: %f(s)", deltaTime);
		ImGui::Text("Start Time: %f(s)", timeSinceStart);
		ImGui::PlotLines("Frame Times", &frameTimes[0], 50, 0, "", frameTimeMin, frameTimeMax, ImVec2(0, 80));
		ImGui::Text("Camera");
		ImGui::InputFloat3("position", &camera->Position.x, 2);
		ImGui::InputFloat3("rotation", &camera->Front.x, 2);
		ImGui::Text("Camera Movement Speed");
		ImGui::SliderFloat("float", &camera->MovementSpeed, 0.1f, 100.0f);
		//ImGui::ColorEdit3("clear color", (float*)&clear_color);
		//if (ImGui::Button("Test Window")) show_test_window ^= 1;
		//if (ImGui::Button("Another Window")) show_another_window ^= 1;
		ImGui::Text("Frame index (of swap chain) : %u", (frameIndex));
		ImGui::End();
	}

	//Terrain info window
	{
		ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
		ImGui::Begin("Terrain Debug Info", &show_test_window);
		ImGui::SliderFloat("Terrain Width", &terrainWidth, 1, 10000);
		ImGui::SliderInt("Terrain Max Subdivision", &terrainMaxLevels, 0, 10);
		ImGui::SliderInt("Terrain Grid Width", &terrainGridDimentions, 1, 10);

		if (ImGui::Button("Recreate Terrain", ImVec2(130, 20))) {
			recreateTerrain = true;
		}
		
		ImGui::Text("All terrains update Time: %u(uS)", terrainUpdateTimer.GetElapsedTimeMicroSeconds());
		for(Terrain* ter : terrains)
		{
			ImGui::Text("Terrain Draw Time: %u(uS)", ter->drawTimer.GetElapsedTimeMicroSeconds());
			ImGui::Text("Terrain Quad Count %d", ter->numQuads);
		}
		ImGui::End(); 
	}
}

//Release associated resources and shutdown imgui
void VulkanApp::CleanUpImgui() {
	ImGui_ImplGlfwVulkan_Shutdown();
	vkDestroyDescriptorPool(vulkanDevice.device, imgui_descriptor_pool, VK_NULL_HANDLE);
}

void VulkanApp::updateScene() {
	if (walkOnGround) {
		//very choppy movement for right now, but since its just a quick 'n dirty way to put the camera at walking height, its just fine
		camera->Position.y = terrains.at(0)->terrainGenerator->SampleHeight(camera->Position.x, 0, camera->Position.z) * terrains.at(0)->heightScale + 2.0;
		if (camera->Position.y < 2) //for over water
			camera->Position.y = 2;
	}

	if (recreateTerrain) {
		RecreateTerrain();
	}

	GlobalVariableUniformBuffer cbo = {};
	cbo.view = camera->GetViewMatrix();
	cbo.proj = glm::perspective(glm::radians(45.0f), vulkanSwapChain.swapChainExtent.width / (float)vulkanSwapChain.swapChainExtent.height, 0.1f, 10000.0f);
	cbo.proj[1][1] *= -1;
	cbo.cameraDir = camera->Front;
	cbo.time = timeSinceStart;

	globalVariableBuffer.map(vulkanDevice.device);
	globalVariableBuffer.copyTo(&cbo, sizeof(cbo));
	globalVariableBuffer.unmap();

	cubeObject->UpdateUniformBuffer(timeSinceStart);

	skybox->UpdateUniform(cbo.proj, camera->GetViewMatrix());

	for (Water* water : waters) {
		water->UpdateUniformBuffer(timeSinceStart, camera->GetViewMatrix());
	}

	for (Terrain* ter : terrains) {
		terrainUpdateTimer.StartTimer();
		ter->UpdateTerrain(camera->Position, vulkanDevice.graphics_queue, globalVariableBuffer, lightsInfoBuffer);
		terrainUpdateTimer.EndTimer();
	}

}

void VulkanApp::HandleInputs() {
	//std::cout << camera->Position.x << " " << camera->Position.y << " " << camera->Position.z << std::endl;

	if (keys[GLFW_KEY_W])
		camera->ProcessKeyboard(FORWARD, deltaTime);
	if (keys[GLFW_KEY_S])
		camera->ProcessKeyboard(BACKWARD, deltaTime);
	if (keys[GLFW_KEY_A])
		camera->ProcessKeyboard(LEFT, deltaTime);
	if (keys[GLFW_KEY_D])
		camera->ProcessKeyboard(RIGHT, deltaTime);
	if (keys[GLFW_KEY_SPACE])
		camera->ProcessKeyboard(UP, deltaTime);
	if (keys[GLFW_KEY_LEFT_SHIFT])
		camera->ProcessKeyboard(DOWN, deltaTime);

}

void VulkanApp::KeyboardEvent(int key, int scancode, int action, int mods) {
	
	if (key == GLFW_KEY_ESCAPE  && action == GLFW_PRESS)
		glfwSetWindowShouldClose(vulkanDevice.window, true);
	if (key == GLFW_KEY_ENTER  && action == GLFW_PRESS)
		SetMouseControl(!mouseControlEnabled);

	if (key == GLFW_KEY_E)
		camera->ChangeCameraSpeed(UP);
	if (key == GLFW_KEY_Q )
		camera->ChangeCameraSpeed(DOWN);
	//std::cout << camera->MovementSpeed << std::endl;
	if (key == GLFW_KEY_X  && action == GLFW_PRESS) {
		wireframe = !wireframe;
		reBuildCommandBuffers();
		std::cout << "wireframe toggled" << std::endl;
	}

	if (key == GLFW_KEY_F  && action == GLFW_PRESS) {
		walkOnGround = !walkOnGround;
		std::cout << "flight mode toggled " << std::endl;
	}

	if (action == GLFW_PRESS)
		keys[key] = true;
	if (action == GLFW_RELEASE)
		keys[key] = false;

}

void VulkanApp::MouseMoved(double xpos, double ypos) {
	if (firstMouse)
	{
		lastX = xpos;
		lastY = ypos;
		firstMouse = false;
	}

	float xoffset = float(xpos - lastX);
	float yoffset = float(lastY - ypos); // reversed since y-coordinates go from bottom to top

	lastX = xpos;
	lastY = ypos;
	if(mouseControlEnabled)
		camera->ProcessMouseMovement(xoffset, yoffset);
}

void VulkanApp::MouseClicked(int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
		if (!ImGui::IsMouseHoveringAnyWindow()) {
			SetMouseControl(true);
		}
	}
}

void VulkanApp::SetMouseControl(bool value) {
	mouseControlEnabled = value;
	if(mouseControlEnabled)
		glfwSetInputMode(vulkanDevice.window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	else
		glfwSetInputMode(vulkanDevice.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
}

void VulkanApp::drawFrame() {
	//uint32_t frameIndex; //which of the swapchain images the app is rendering to
	VkResult result = vkAcquireNextImageKHR(vulkanDevice.device, vulkanSwapChain.swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &frameIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("failed to acquire swap chain image!");
	}

	VkSubmitInfo submitInfo = initializers::submitInfo();

	VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
	VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffers[frameIndex];

	VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	if (vkQueueSubmit(vulkanDevice.graphics_queue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("failed to submit draw command buffer!");
	}

	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	VkSwapchainKHR swapChains[] = { vulkanSwapChain.swapChain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &frameIndex;

	result = vkQueuePresentKHR(vulkanDevice.present_queue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to present swap chain image!");
	}

	vkQueueWaitIdle(vulkanDevice.present_queue);
}
