#pragma once

#include <array>
#include <vector>
#include <string>

#include <vulkan/vulkan.h>

#include "VulkanTools.h"
#include "RendererStructs.h"
#include "VulkanDevice.hpp"
#include "VulkanInitializers.hpp"
#include "VulkanPipeline.hpp"
#include "VulkanShader.hpp"
#include "VulkanSwapChain.hpp"

class Scene;

class VulkanRenderer
{
public:
	VulkanRenderer(std::shared_ptr<Scene> scene);
	VulkanRenderer(const VulkanRenderer& other) = default; //copy
	VulkanRenderer(VulkanRenderer&& other) = default; //move
	VulkanRenderer& operator=(const VulkanRenderer&) = default;
	VulkanRenderer& operator=(VulkanRenderer&&) = default;
	~VulkanRenderer();

	void InitVulkanRenderer(GLFWwindow* window);
	void RenderFrame();
	void CleanVulkanResources();

	//void InitSwapchain();
	void RecreateSwapChain();

	void CreateRenderPass();
	void CreateDepthResources();

	void BuildCommandBuffers();
	void ReBuildCommandBuffers();
	
	void CreatePrimaryCommandBuffer(); //testing out multiple command buffers

	void CreateCommandBuffers();
	void CreateSemaphores();

	void PrepareFrame();
	void SubmitFrame();

	VkFormat FindSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
	VkFormat FindDepthFormat();

	void CreateImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);
	void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

	VkCommandBuffer BeginSingleTimeCommands();
	void EndSingleTimeCommands(VkCommandBuffer commandBuffer);
	void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

	bool SaveScreenshot(const std::string filename);
	void SetWireframe(bool wireframe);

	VulkanDevice device;
	VulkanSwapChain vulkanSwapChain;
	VkRenderPass renderPass;

	VulkanPipeline pipelineManager;
	VulkanShader shaderManager;

	//uint32_t frameIndex = 1; // which frame of the swapchain it is on

	//Depth buffer
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;

	//Command buffer per frame
	std::vector<VkCommandBuffer> commandBuffers;

	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

private:
	std::shared_ptr<Scene> scene;

	uint32_t frameIndex; //which of the swapchain images the app is rendering to
	bool wireframe = false; //whether or not to use the wireframe pipeline for the scene.


	VkClearColorValue clearColor = { 0.2f, 0.3f, 0.3f, 1.0f };
	VkClearDepthStencilValue depthClearColor = { 0.0f, 0 };

	std::array<VkClearValue, 2> GetFramebufferClearValues();
};

