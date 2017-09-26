#pragma once

#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <string>
#include <array>
#include <set>

#include <vulkan\vulkan.h>

#include <glm\common.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <GLFW/glfw3.h>

//#define VMA_IMPLEMENTATION
//#include <vk_mem_alloc.h>

struct SwapChainSupportDetails
{
	VkSurfaceCapabilitiesKHR capabilities;
	std::vector<VkSurfaceFormatKHR> formats;
	std::vector<VkPresentModeKHR> present_modes;
};

struct QueueFamilyIndices {
	int graphicsFamily = -1;
	int presentFamily = -1;
	int transferFamily = -1;
	int computeFamily = -1;

	bool isComplete() {
		return graphicsFamily >= 0 && presentFamily >= 0;
	}
};

/* Common uniform buffers */

//Global info buffer
struct GlobalVariableUniformBuffer {
	glm::mat4 view;
	glm::mat4 proj;
	glm::vec3 cameraDir;
	float time;
};

//model specific data (position, normal matrix)
struct ModelBufferObject {
	glm::mat4 model;
	glm::mat4 normal;
};

//Lighting struct
struct PointLight {
	glm::vec4 lightPos = glm::vec4(50.0f, 25.0f, 50.0f, 1.0f);
	glm::vec4 color = glm::vec4(1.0, 1.0, 1.0f, 1.0f);
	glm::vec4 attenuation = glm::vec4(1.0, 0.007f, 0.0002f, 1.0f);

	PointLight() {};
	PointLight(glm::vec4 pos, glm::vec4 col, glm::vec4 atten) : lightPos(pos), color(col), attenuation(atten) {};
};

//struct DirectionalLight {
//	glm::vec4 lightDir = glm::vec4(50.0f, -65.0f, 50.0f, 1.0f);
//	glm::vec4 color = glm::vec4(1.0, 1.0, 1.0f, 1.0f);
//	glm::vec4 attenuation = glm::vec4(1.0, 0.007f, 0.0002f, 1.0f);
//
//	DirectionalLight() {};
//	DirectionalLight(glm::vec4 dir, glm::vec4 col, glm::vec4 atten) : lightDir(dir), color(col), attenuation(atten) {};
//};

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes);

VkExtent2D chooseSwapExtent(GLFWwindow *window, const VkSurfaceCapabilitiesKHR& capabilities);

bool fileExists(const std::string &filename);

static std::vector<char> readFile(const std::string& filename);

VkShaderModule loadShaderModule(VkDevice device, const std::string& codePath);

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR windowSurface);

uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);

VkImageView createImageView(VkDevice device, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, VkImageSubresourceRange subresourceRange, 
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

// Fixed sub resource on first mip level and layer
void setImageLayout(VkCommandBuffer cmdbuffer, VkImage image, VkImageAspectFlags aspectMask, VkImageLayout oldImageLayout, VkImageLayout newImageLayout, 
	VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VkPipelineStageFlags dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

/** @brief Returns an error code as a string */
std::string errorString(VkResult errorCode);

#define VK_CHECK_RESULT(f)																				\
{																										\
	VkResult res = (f);																					\
	if (res != VK_SUCCESS)																				\
	{																									\
		std::cout << "Fatal : VkResult is \"" << errorString(res) << "\" in " << __FILE__ << " at line " << __LINE__ << std::endl; \
		assert(res == VK_SUCCESS);																		\
	}																									\
}

// Custom define for better code readability
#define VK_FLAGS_NONE 0
// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000


class SimpleTimer {
private:
	std::chrono::time_point<std::chrono::high_resolution_clock> endTime;
	std::chrono::time_point<std::chrono::high_resolution_clock> startTime;

	std::chrono::nanoseconds elapsedTime;
public:

	//Begin timer
	void StartTimer() {
		startTime = std::chrono::high_resolution_clock::now();
	}
	
	//End timer
	void EndTimer() {
		endTime = std::chrono::high_resolution_clock::now();
		elapsedTime = endTime - startTime;
	}

	std::chrono::time_point<std::chrono::high_resolution_clock> GetStartTime() {
		return startTime;
	}
	
	std::chrono::time_point<std::chrono::high_resolution_clock> GetEndTime() {
		return endTime;
	}

	uint64_t GetElapsedTimeSeconds() {
		return std::chrono::duration_cast<std::chrono::seconds>(elapsedTime).count();
	}

	uint64_t GetElapsedTimeMilliSecondsSeconds() {
		return std::chrono::duration_cast<std::chrono::milliseconds>(elapsedTime).count();
	}

	uint64_t GetElapsedTimeMicroSeconds() {
		return std::chrono::duration_cast<std::chrono::microseconds>(elapsedTime).count();
	}

	uint64_t GetElapsedTimeNanoSeconds() {
		return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsedTime).count();
	}

};