#pragma once

#include <vector>
#include <memory>
#include <set>
#include <thread>
#include <mutex>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan.hpp>

#include <GLFW/glfw3.h>

#include "../../third-party/VulkanMemoryAllocator/vk_mem_alloc.h"

#include "RenderTools.h"
#include "RenderStructs.h"

const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_LUNARG_standard_validation"

};

const std::vector<const char*> DEVICE_EXTENSIONS = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME
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

struct VmaBuffer {
	VkBuffer buffer = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo allocationInfo;
};

struct VmaImage {
	VkImage image = VK_NULL_HANDLE;
	VmaAllocation allocation = VK_NULL_HANDLE;
	VmaAllocationInfo allocationInfo;
};

class VulkanDevice;
class VulkanBuffer;

class CommandBuffer {
public:
	CommandBuffer(VulkanDevice& device);
	VkCommandBuffer* GetCommandBuffer();

	void BeginBufferRecording(VkCommandBufferUsageFlagBits flags = (VkCommandBufferUsageFlagBits)(0));
	void EndBufferRecording();

	void ResetBuffer(VkCommandBufferResetFlags flags);
private:
	VulkanDevice& device;
	VkCommandBuffer buf;
};

class CommandQueue {
public:
	CommandQueue(VulkanDevice& device, uint32_t queueFamily);
	void SubmitCommandBuffer(CommandBuffer buf, VkFence fence);

	uint32_t GetQueueFamily();
private:
	VulkanDevice& device;
	std::mutex submissionMutex;
	VkQueue queue;
	uint32_t queueFamily;
};

class CommandPool {
public:
	CommandPool(VulkanDevice& device, CommandQueue& queue, VkCommandPoolCreateFlags flags);
	VkBool32 CleanUp();

	CommandBuffer GetOneTimeUseCommandBuffer(int count);
	CommandBuffer GetPrimaryCommandBuffer(int count);
	CommandBuffer GetSecondaryCommandBuffer(int count);

	void AllocateCommandBuffer(CommandBuffer buf, VkCommandBufferLevel);
	void FreeCommandBuffer(CommandBuffer buf);

	VkBool32 SubmitOneTimeUseCommandBuffer(CommandBuffer cmdBuffer, VkFence fence);
	VkBool32 SubmitPrimaryCommandBuffer(CommandBuffer buf, VkFence fence);
	VkBool32 SubmitSecondaryCommandBuffer(CommandBuffer buf, VkFence fence);

private:
	VulkanDevice& device;
	std::mutex poolLock;
	VkCommandPool commandPool;
	CommandQueue& queue;

	std::vector<CommandBuffer> cmdBuffers;
};

class TransferQueue {
public:
	TransferQueue(VulkanDevice& device, uint32_t transferFamily);
	void CleanUp();

	VkDevice GetDevice();
	VkCommandPool GetCommandPool();
	std::mutex& GetTransferMutex();

	VkCommandBuffer GetTransferCommandBuffer();
	void SubmitTransferCommandBuffer(VkCommandBuffer buf, std::vector<Signal> readySignal);
	void SubmitTransferCommandBuffer(VkCommandBuffer buf, std::vector<Signal> readySignal, std::vector<VulkanBuffer> bufsToClean);
	void SubmitTransferCommandBufferAndWait(VkCommandBuffer buf);

private:
	VulkanDevice& device;
	VkQueue transfer_queue;
	VkCommandPool transfer_queue_command_pool;
	std::mutex transferMutex;
};

class VulkanDevice {
public:
	GLFWwindow* window;

	VkDevice device;
	VkInstance instance;
	VkDebugReportCallbackEXT callback;
	
	//VDeleter<VkSurfaceKHR> window_surface{ instance, vkDestroySurfaceKHR };

	VkPhysicalDevice physical_device;

	std::mutex graphics_lock;
	std::mutex graphics_command_pool_lock;

	VkQueue graphics_queue;
	VkQueue compute_queue;
	VkQueue present_queue;
	
	VkCommandPool graphics_queue_command_pool;
	VkCommandPool compute_queue_command_pool;


	VkPhysicalDeviceProperties physical_device_properties;
	VkPhysicalDeviceFeatures physical_device_features;
	VkPhysicalDeviceMemoryProperties memoryProperties;

	VmaAllocator allocator;

	VkMemoryPropertyFlags uniformBufferMemPropertyFlags = 
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

	VulkanDevice(bool validationLayers);

	~VulkanDevice();

	void InitVulkanDevice(VkSurfaceKHR &surface);

	void Cleanup(VkSurfaceKHR &surface);

	const QueueFamilyIndices GetFamilyIndices() const;

	void VmaMapMemory(VmaBuffer& buffer, void** pData);
	void VmaUnmapMemory(VmaBuffer& buffer);

	void CreateUniformBuffer(VmaBuffer& buffer, VkDeviceSize bufferSize); 
	void CreateUniformBufferMapped(VmaBuffer& buffer, VkDeviceSize bufferSize);
	void CreateStagingUniformBuffer(VmaBuffer& buffer, void* data, VkDeviceSize bufferSize);

	void CreateDynamicUniformBuffer(VmaBuffer& buffer, uint32_t count, VkDeviceSize sizeOfData);

	void CreateMeshBufferVertex(VmaBuffer& buffer, VkDeviceSize bufferSize);
	void CreateMeshBufferIndex(VmaBuffer& buffer, VkDeviceSize bufferSize);
	void CreateMeshStagingBuffer(VmaBuffer& buffer, void* data, VkDeviceSize bufferSize);

	void CreateInstancingBuffer(VmaBuffer& buffer, VkDeviceSize bufferSize);
	void CreateStagingInstancingBuffer(VmaBuffer& buffer, void* data, VkDeviceSize bufferSize);
	
	void DestroyVmaAllocatedBuffer(VkBuffer* buffer, VmaAllocation* allocation);
	void DestroyVmaAllocatedBuffer(VmaBuffer& buffer);

	void CreateImage2D(VkImageCreateInfo imageInfo, VmaImage& image);
	void CreateDepthImage(VkImageCreateInfo imageInfo, VmaImage& image);
	void CreateStagingImage2D(VkImageCreateInfo imageInfo, VmaImage& image);

	void DestroyVmaAllocatedImage(VmaImage& image);

	VkCommandBuffer GetGraphicsCommandBuffer();
	VkCommandBuffer GetSingleUseGraphicsCommandBuffer();
	void SubmitGraphicsCommandBufferAndWait(VkCommandBuffer buffer);

	VkCommandBuffer GetTransferCommandBuffer();

	void SubmitTransferCommandBufferAndWait(VkCommandBuffer buf);
	void SubmitTransferCommandBuffer(VkCommandBuffer buf, std::vector<Signal> readySignal);
	void SubmitTransferCommandBuffer(VkCommandBuffer buf, std::vector<Signal> readySignal, std::vector<VulkanBuffer> bufsToClean);


	/**
	* Get the index of a memory type that has all the requested property bits set
	*
	* @param typeBits Bitmask with bits set for each memory type supported by the resource to request for (from VkMemoryRequirements)
	* @param properties Bitmask of properties for the memory type to request
	* @param (Optional) memTypeFound Pointer to a bool that is set to true if a matching memory type has been found
	*
	* @return Index of the requested memory type
	*
	* @throw Throws an exception if memTypeFound is null and no memory type could be found that supports the requested properties
	*/
	uint32_t getMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr);

private:
	QueueFamilyIndices familyIndices;

	bool enableValidationLayers = false;

	bool separateTransferQueue = false;


	//Non separate transfer queue (no threads where they don't help)
	VkQueue transfer_queue;
	VkCommandPool transfer_queue_command_pool;
	VkCommandBuffer dmaCmdBuf;

	std::unique_ptr<TransferQueue> transferQueue;

	void createInstance(std::string appName);

	bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface);

	bool checkDeviceExtensionSupport(VkPhysicalDevice device);

	bool checkValidationLayerSupport();
	
	void setupDebugCallback();
	
	void createSurface(VkSurfaceKHR &surface);
	
	void pickPhysicalDevice(VkSurfaceKHR &surface);

	void CreateLogicalDevice();
	void CreateQueues();
	void CreateCommandPools();

	void CreateVulkanAllocator();

	void FindQueueFamilies(VkSurfaceKHR windowSurface);
	QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physDevice, VkSurfaceKHR windowSurface);

	VkPhysicalDeviceFeatures QueryDeviceFeatures();
	
	VkResult CreateDebugReportCallbackEXT(VkInstance instance, const VkDebugReportCallbackCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugReportCallbackEXT* pCallback);

	void DestroyDebugReportCallbackEXT(VkInstance instance, VkDebugReportCallbackEXT callback, const VkAllocationCallbacks* pAllocator);
	
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objType, uint64_t obj, size_t location, int32_t code, const char* layerPrefix, const char* msg, void* userData);
};
