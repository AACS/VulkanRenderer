#pragma once

#include <array>
#include <vector>
#include <string>
#include <memory>
#include <set>

#include <functional>
#include <mutex>
#include <thread>
#include <condition_variable>

#include <vulkan/vulkan.h>

#include "RenderStructs.h"
#include "RenderTools.h"
#include "Buffer.h"

#include "../util/ConcurrentQueue.h"


class VulkanDevice;

class VulkanFence {
public:
	VulkanFence(const VulkanDevice& device,
		long int timeout = DEFAULT_FENCE_TIMEOUT,
		VkFenceCreateFlags flags = VK_FLAGS_NONE);
	void CleanUp();

	void WaitTillTrue();
	void WaitTillFalse();
	//void CleanUp();
	VkFence Get();

private:
	const VulkanDevice& device;
	VkFence fence;
	int timeout;
};

std::vector<VkFence> CreateFenceArray(std::vector<VulkanFence>& fences);

class VulkanSemaphore {
public:
	VulkanSemaphore(const VulkanDevice& device);

	VkSemaphore Get();
	VkSemaphore* GetPtr();

	void CleanUp(const VulkanDevice & device);

private:
	VkSemaphore semaphore;
};

std::vector<VkSemaphore> CreateSemaphoreArray(std::vector<VulkanSemaphore>& sems);

class CommandQueue {
public:
	CommandQueue(const VulkanDevice&  device, int queueFamily);

	//No copy or move construction (so that it won't get duplicated)
	CommandQueue(const CommandQueue& cmd) = delete;
	CommandQueue& operator=(const CommandQueue& cmd) = delete;
	CommandQueue(CommandQueue&& cmd) = delete;
	CommandQueue& operator=(CommandQueue&& cmd) = delete;

	void Submit(VkSubmitInfo info, VkFence fence);

	void SubmitCommandBuffer(VkCommandBuffer buf, VkFence fence);

	void SubmitCommandBuffer(VkCommandBuffer buffer,
		VulkanFence& fence,
		std::vector<VulkanSemaphore>& waitSemaphores,
		std::vector<VulkanSemaphore>& signalSemaphores);

	int GetQueueFamily();
	std::mutex& GetQueueMutex();
	VkQueue GetQueue();

	void WaitForFences(VkFence fence);

private:
	const VulkanDevice & device;
	std::mutex submissionMutex;
	VkQueue queue;
	int queueFamily;
};


class CommandPool {
public:
	CommandPool(VulkanDevice& device);

	VkBool32 Setup(VkCommandPoolCreateFlags flags, CommandQueue* queue);
	VkBool32 CleanUp();

	VkBool32 ResetPool();
	VkBool32 ResetCommandBuffer(VkCommandBuffer cmdBuf);

	VkCommandBuffer GetOneTimeUseCommandBuffer();
	VkCommandBuffer GetPrimaryCommandBuffer(bool beginBufferRecording = true);
	VkCommandBuffer GetSecondaryCommandBuffer(bool beginBufferRecording = true);

	//VkBool32 SubmitCommandBuffer(VkCommandBuffer, VulkanFence& fence,
	//	std::vector<VkSemaphore>& waitSemaphores, std::vector<VkSemaphore>& signalSemaphores);
	VkBool32 SubmitCommandBuffer(VkCommandBuffer, VulkanFence& fence,
		std::vector<VulkanSemaphore>& waitSemaphores, std::vector<VulkanSemaphore>& signalSemaphores);

	VkBool32 SubmitOneTimeUseCommandBuffer(VkCommandBuffer cmdBuffer, VkFence fence = nullptr);
	VkBool32 SubmitPrimaryCommandBuffer(VkCommandBuffer buf, VkFence fence = nullptr);

	VkCommandBuffer AllocateCommandBuffer(VkCommandBufferLevel level);

	void BeginBufferRecording(VkCommandBuffer buf, VkCommandBufferUsageFlagBits flags = (VkCommandBufferUsageFlagBits)(0));
	void EndBufferRecording(VkCommandBuffer buf);
	void FreeCommandBuffer(VkCommandBuffer buf);

	void WriteToBuffer(VkCommandBuffer buf, std::function<void(VkCommandBuffer)> cmd);

private:
	VulkanDevice & device;
	std::mutex poolLock;
	VkCommandPool commandPool;
	CommandQueue* queue;

};

class CommandBuffer{
private:
	CommandBuffer(VulkanDevice device, CommandPool& pool);
	void CleanUp();

	void Begin();
	void End();

	void Write(std::function<void(VkCommandBuffer)> cmds);

	void Submit(std::vector<VulkanSemaphore>& waitSemaphores, std::vector<VulkanSemaphore>& signalSemaphores);
public:
	CommandPool& pool;
	VkCommandBuffer buf;
	VulkanFence fence;
};

struct CommandBufferWork {
	std::function<void(const VkCommandBuffer)> work;
	std::vector<Signal> flags;
	std::vector<VulkanSemaphore> waitSemaphores;
	std::vector<VulkanSemaphore> signalSemaphores;
};

struct TransferCommandWork {
	std::function<void(const VkCommandBuffer)> work;
	std::vector<Signal> flags;
	std::vector<VulkanSemaphore> waitSemaphores;
	std::vector<VulkanSemaphore> signalSemaphores;

	//Optional buffers to clean once 
	std::vector<VulkanBuffer> buffersToClean;
};

template<typename WorkType>
class CommandBufferWorkQueue {
public:
	CommandBufferWorkQueue();

	~CommandBufferWorkQueue();

	void AddWork(WorkType data);
	void AddWork(WorkType&& data);

	bool HasWork();

	std::optional<WorkType> GetWork();

	std::mutex lock;
	std::condition_variable condVar;

private:
	ConcurrentQueue<WorkType> workQueue;
};

template<typename WorkType>
class CommandBufferWorker {
public:
	CommandBufferWorker(VulkanDevice& device,
		CommandQueue* queue, CommandBufferWorkQueue<WorkType>& workQueue,
		bool startActive = true);

	CommandBufferWorker(const CommandBufferWorker& other) = delete; //copy
	CommandBufferWorker(CommandBufferWorker&& other) = delete; //move
	CommandBufferWorker& operator=(const CommandBufferWorker&) = default;
	CommandBufferWorker& operator=(CommandBufferWorker&&) = default;

	~CommandBufferWorker();

	void CleanUp();

	void StopWork();

private:
	VulkanDevice & device;
	void Work();
	std::thread workingThread;

	bool keepWorking = true; //default to start working
	CommandBufferWorkQueue<WorkType>& workQueue;
	std::condition_variable waitVar;
	CommandPool pool;
};

template <typename WorkType>
CommandBufferWorkQueue<WorkType>::CommandBufferWorkQueue() {}

template <typename WorkType>
CommandBufferWorkQueue<WorkType>::~CommandBufferWorkQueue() {
	condVar.notify_all();
}

template <typename WorkType>
void CommandBufferWorkQueue<WorkType>::AddWork(WorkType data) {
	workQueue.push_back(data);
	condVar.notify_one();
}

template <typename WorkType>
void CommandBufferWorkQueue<WorkType>::AddWork(WorkType &&data) {
	workQueue.push_back(std::move(data));
	condVar.notify_one();
}

template <typename WorkType> 
bool CommandBufferWorkQueue<WorkType>::HasWork() {
	return workQueue.empty();
}

template <typename WorkType>
std::optional<WorkType> CommandBufferWorkQueue<WorkType>::GetWork() {
	return workQueue.pop_if();
}

template <typename WorkType>
CommandBufferWorker<WorkType>::CommandBufferWorker(
	VulkanDevice &device, CommandQueue *queue,
	CommandBufferWorkQueue<WorkType> &workQueue, bool startActive)
	: device(device), pool(device), workQueue(workQueue) {
	pool.Setup(VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, queue);
	workingThread = std::thread{ &CommandBufferWorker::Work, this };
}

template <typename WorkType>
CommandBufferWorker<WorkType>::~CommandBufferWorker() {
	//workingThread.join();
}

template <typename WorkType>
void CommandBufferWorker<WorkType>::CleanUp() {
	StopWork();
	workQueue.condVar.notify_all();//Get everyone to wake up
	workingThread.join();
	pool.CleanUp();
}

template <typename WorkType> void CommandBufferWorker<WorkType>::StopWork() {
	keepWorking = false;
}

class FrameObject {
public:
	FrameObject(VulkanDevice& device, int frameD);
	~FrameObject();



	VkResult AquireNextSwapchainImage(VkSwapchainKHR swapchain);
	void PrepareFrame();
	void SubmitFrame();

	VkSubmitInfo GetSubmitInfo();
	VkPresentInfoKHR GetPresentInfo();

	VulkanSemaphore& GetImageAvailSem();
	VulkanSemaphore& GetRenderFinishSem();

	VkCommandBuffer GetPrimaryCmdBuf();

private:
	VulkanDevice & device;
	uint32_t frameIndex; //which frame in the queue it is

	uint32_t swapChainIndex; // which frame to render to

	VulkanSemaphore imageAvailSem;
	VulkanSemaphore renderFinishSem;

	CommandPool commandPool;
	VkCommandBuffer primaryCmdBuf;
	//std::vector<VkCommandBuffer> secondCmdBufs;

};