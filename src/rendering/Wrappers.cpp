#include "Wrappers.h"

#include "Initializers.h"

#include "Device.h"
#include "Buffer.h"

VulkanFence::VulkanFence(VulkanDevice& device, long int timeout,
	VkFenceCreateFlags flags)
	:device(device), timeout(timeout)
{
	this->timeout = timeout;
	VkFenceCreateInfo fenceInfo =
		initializers::fenceCreateInfo(flags);
	VK_CHECK_RESULT(vkCreateFence(device.device, &fenceInfo, nullptr, &fence))

}

VulkanFence::~VulkanFence()
{
	vkDestroyFence(device.device, fence, nullptr);
}

bool VulkanFence::Check() {
	VkResult out = vkGetFenceStatus(device.device, fence);
	if (out == VK_SUCCESS)
		return true;
	else if (out == VK_NOT_READY)
		return false;
	throw std::runtime_error("DEVICE_LOST");
	return false;
}

void VulkanFence::WaitTillTrue() {
	vkWaitForFences(device.device, 1, &fence, VK_TRUE, timeout);
}
void VulkanFence::WaitTillFalse() {
	vkWaitForFences(device.device, 1, &fence, VK_FALSE, timeout);
}
VkFence VulkanFence::Get() {
	return fence;
}

void VulkanFence::Reset() {
	vkResetFences(device.device, 1, &fence);
}

std::vector<VkFence> CreateFenceArray(std::vector<VulkanFence>& fences)
{
	std::vector<VkFence> outFences(fences.size());
	for (auto& fence : fences)
		outFences.push_back(fence.Get());
	return outFences;
}

VulkanSemaphore::VulkanSemaphore(VulkanDevice& device)
	:device(device) {
	VkSemaphoreCreateInfo semaphoreInfo = initializers::semaphoreCreateInfo();

	VK_CHECK_RESULT(vkCreateSemaphore(device.device, &semaphoreInfo, nullptr, &semaphore));

}

VulkanSemaphore::~VulkanSemaphore() {
	vkDestroySemaphore(device.device, semaphore, nullptr);
}

VkSemaphore VulkanSemaphore::Get() {
	return semaphore;
}

VkSemaphore* VulkanSemaphore::GetPtr() {
	return &semaphore;
}

std::vector<VkSemaphore> CreateSemaphoreArray(std::vector<std::shared_ptr<VulkanSemaphore>>& sems)
{
	std::vector<VkSemaphore> outSems;
	for (auto& sem : sems)
		outSems.push_back(sem->Get());

	return outSems;
}

CommandQueue::CommandQueue(const VulkanDevice& device, int queueFamily) :
	device(device)
{
	vkGetDeviceQueue(device.device, queueFamily, 0, &queue);
	this->queueFamily = queueFamily;
	//Log::Debug << "Queue on " << queueFamily << " type\n";
}

//void CommandQueue::SetupQueue(int queueFamily) {
//	vkGetDeviceQueue(device.device, queueFamily, 0, &queue);
//	this->queueFamily = queueFamily;
//}

void CommandQueue::Submit(VkSubmitInfo submitInfo, VkFence fence) {
	std::lock_guard<std::mutex> lock(submissionMutex);
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, fence));
}

void CommandQueue::SubmitCommandBuffer(VkCommandBuffer buffer, VkFence fence)
{
	const auto stageMasks = std::vector<VkPipelineStageFlags>{ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	auto submitInfo = initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &buffer;
	submitInfo.pWaitDstStageMask = stageMasks.data();
	Submit(submitInfo, fence);
}

void CommandQueue::SubmitCommandBuffer(VkCommandBuffer cmdBuffer,
	VulkanFence& fence,
	std::vector<std::shared_ptr<VulkanSemaphore>>& waitSemaphores,
	std::vector<std::shared_ptr<VulkanSemaphore>>& signalSemaphores)
{
	auto waits = CreateSemaphoreArray(waitSemaphores);
	auto sigs = CreateSemaphoreArray(signalSemaphores);

	const auto stageMasks = std::vector<VkPipelineStageFlags>{ VK_PIPELINE_STAGE_ALL_COMMANDS_BIT };
	auto submitInfo = initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	submitInfo.signalSemaphoreCount = (uint32_t)sigs.size();
	submitInfo.pSignalSemaphores = sigs.data();
	submitInfo.waitSemaphoreCount = (uint32_t)waits.size();
	submitInfo.pWaitSemaphores = waits.data();
	submitInfo.pWaitDstStageMask = stageMasks.data();
	Submit(submitInfo, fence.Get());
}


int CommandQueue::GetQueueFamily() {
	return queueFamily;
}

VkQueue CommandQueue::GetQueue() {
	return queue;
}

std::mutex& CommandQueue::GetQueueMutex() {
	return submissionMutex;
}

void CommandQueue::WaitForFences(VkFence fence) {
	vkWaitForFences(device.device, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
}

CommandPool::CommandPool(VulkanDevice& device,
	VkCommandPoolCreateFlags flags, CommandQueue* queue) :
	device(device)
{
	this->queue = queue;

	VkCommandPoolCreateInfo cmd_pool_info = initializers::commandPoolCreateInfo();
	cmd_pool_info.queueFamilyIndex = queue->GetQueueFamily();
	cmd_pool_info.flags = flags;

	if (vkCreateCommandPool(device.device, &cmd_pool_info, nullptr, &commandPool) != VK_SUCCESS) {
		throw std::runtime_error("failed to create graphics command pool!");
	}

	//return VK_TRUE;
}

CommandPool::~CommandPool() {
	std::lock_guard<std::mutex> lock(poolLock);
	vkDestroyCommandPool(device.device, commandPool, nullptr);
}

VkBool32 CommandPool::ResetPool() {

	std::lock_guard<std::mutex> lock(poolLock);
	vkResetCommandPool(device.device, commandPool, 0);
	return VK_TRUE;
}

VkBool32 CommandPool::ResetCommandBuffer(VkCommandBuffer cmdBuf) {
	std::lock_guard<std::mutex> lock(poolLock);
	vkResetCommandBuffer(cmdBuf, {});
	return VK_TRUE;
}

VkCommandBuffer CommandPool::AllocateCommandBuffer(VkCommandBufferLevel level)
{
	VkCommandBuffer buf;

	VkCommandBufferAllocateInfo allocInfo =
		initializers::commandBufferAllocateInfo(commandPool, level, 1);

	std::lock_guard<std::mutex> lock(poolLock);
	vkAllocateCommandBuffers(device.device, &allocInfo, &buf);

	return buf;
}

void CommandPool::BeginBufferRecording(VkCommandBuffer buf, VkCommandBufferUsageFlagBits flags) {

	VkCommandBufferBeginInfo beginInfo = initializers::commandBufferBeginInfo();
	beginInfo.flags = flags;

	vkBeginCommandBuffer(buf, &beginInfo);
}

void CommandPool::EndBufferRecording(VkCommandBuffer buf) {
	vkEndCommandBuffer(buf);
}

void CommandPool::FreeCommandBuffer(VkCommandBuffer buf) {

	std::lock_guard<std::mutex> lock(poolLock);
	vkFreeCommandBuffers(device.device, commandPool, 1, &buf);

}

VkCommandBuffer CommandPool::GetOneTimeUseCommandBuffer() {
	VkCommandBuffer cmdBuffer = AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	BeginBufferRecording(cmdBuffer, VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	return cmdBuffer;
}

VkCommandBuffer CommandPool::GetPrimaryCommandBuffer(bool beginBufferRecording) {

	VkCommandBuffer cmdBuffer = AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY);

	if (beginBufferRecording == true)
		BeginBufferRecording(cmdBuffer);

	return cmdBuffer;
}

VkCommandBuffer CommandPool::GetSecondaryCommandBuffer(bool beginBufferRecording) {
	VkCommandBuffer cmdBuffer = AllocateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_SECONDARY);

	if (beginBufferRecording == true)
		BeginBufferRecording(cmdBuffer);

	return cmdBuffer;
}

// VkBool32 CommandPool::SubmitCommandBuffer(VkCommandBuffer cmdBuffer, VulkanFence& fence,
// 	std::vector<VkSemaphore>& waitSemaphores, std::vector<VkSemaphore>& signalSemaphores)
// {
// 	EndBufferRecording(cmdBuffer);
// 	queue->SubmitCommandBuffer(cmdBuffer, fence, waitSemaphores, signalSemaphores);

// 	return VK_TRUE;
// }

VkBool32 CommandPool::SubmitCommandBuffer(VkCommandBuffer cmdBuffer, VulkanFence& fence,
	std::vector<std::shared_ptr<VulkanSemaphore>>& waitSemaphores,
	std::vector<std::shared_ptr<VulkanSemaphore>>& signalSemaphores)
{
	EndBufferRecording(cmdBuffer);
	queue->SubmitCommandBuffer(cmdBuffer, fence, waitSemaphores, signalSemaphores);

	return VK_TRUE;
}


VkBool32 CommandPool::SubmitOneTimeUseCommandBuffer(VkCommandBuffer cmdBuffer, VkFence fence) {
	EndBufferRecording(cmdBuffer);

	if (fence == nullptr) {
		VkFenceCreateInfo fenceInfo = initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VK_CHECK_RESULT(vkCreateFence(device.device, &fenceInfo, nullptr, &fence))
	}

	queue->SubmitCommandBuffer(cmdBuffer, fence);

	return VK_TRUE;
}

VkBool32 CommandPool::SubmitPrimaryCommandBuffer(VkCommandBuffer cmdBuffer, VkFence fence) {
	EndBufferRecording(cmdBuffer);

	if (fence == nullptr) {
		VkFenceCreateInfo fenceInfo = initializers::fenceCreateInfo(VK_FLAGS_NONE);
		VK_CHECK_RESULT(vkCreateFence(device.device, &fenceInfo, nullptr, &fence))
	}

	queue->SubmitCommandBuffer(cmdBuffer, fence);

	return VK_TRUE;
}


void CommandPool::WriteToBuffer(VkCommandBuffer buf, std::function<void(VkCommandBuffer)> cmds)
{
	std::lock_guard<std::mutex> lock(poolLock);
	cmds(buf);
}

GraphicsCommandWorker::GraphicsCommandWorker(
	VulkanDevice &device,
	ConcurrentQueue<GraphicsWork>& workQueue,
	std::vector<GraphicsCleanUpWork>& finishQueue,
	std::mutex& finishQueueLock,
	bool startActive) :

	device(device),
	graphicsPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &device.GraphicsQueue()),
	transferPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &device.TransferQueue()),
	computePool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT, &device.ComputeQueue()),
	workQueue(workQueue), finishQueue(finishQueue), finishQueueLock(finishQueueLock)
{
	workingThread = std::thread{ &GraphicsCommandWorker::Work, this };

}

GraphicsCommandWorker::~GraphicsCommandWorker() {
	StopWork();
	workingThread.join();
}

void GraphicsCommandWorker::StopWork() {
	keepWorking = false;
}

bool GraphicsCommandWorker::IsFinishedWorking() {
	return isDoneWorking;
}

void GraphicsCommandWorker::Work() {
	while (keepWorking)
	{

		workQueue.wait_on_value();

		while (!workQueue.empty()) {
			auto pos_work = workQueue.pop_if();
			if (pos_work.has_value()) {


				VkCommandBuffer cmdBuf;
				CommandPool* pool;

				switch (pos_work->type) {
				case(WorkType::graphics):
					pool = &graphicsPool;
					break;
				case(WorkType::transfer):
					pool = &transferPool;
					break;
				case(WorkType::compute):
					pool = &computePool;
					break;
				}

				cmdBuf = pool->GetOneTimeUseCommandBuffer();
				//pool->BeginBufferRecording(cmdBuf);
				pos_work->work(cmdBuf);
				//pool->EndBufferRecording(cmdBuf);
				pool->SubmitCommandBuffer(cmdBuf, *pos_work->fence,
					pos_work->waitSemaphores, pos_work->signalSemaphores);

				{
					std::lock_guard<std::mutex>lk(finishQueueLock);
					finishQueue.push_back(GraphicsCleanUpWork(*pos_work, pool, cmdBuf));
				}
			}
		}
	}
	isDoneWorking = true;
}


FrameObject::FrameObject(VulkanDevice& device, int frameIndex) :
	device(device),
	frameIndex(frameIndex),
	commandPool(device, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
		&device.GraphicsQueue()),
	imageAvailSem(device),
	renderFinishSem(device),
	commandFence(device),
	depthFence(device)
{
	primaryCmdBuf = commandPool.GetPrimaryCommandBuffer(false);
}

FrameObject::~FrameObject() {
	commandPool.FreeCommandBuffer(primaryCmdBuf);
}

VkResult FrameObject::AquireNextSwapchainImage(VkSwapchainKHR swapchain) {
	return vkAcquireNextImageKHR(
		device.device, swapchain,
		std::numeric_limits<uint64_t>::max(), imageAvailSem.Get(),
		VK_NULL_HANDLE, &swapChainIndex);
}

void FrameObject::PrepareDepthPass(){
	WaitTillReady();
	commandPool.BeginBufferRecording(depthCmdBuf, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
}

void FrameObject::EndDepthPass(){
	commandPool.EndBufferRecording(depthCmdBuf);

}

void FrameObject::PrepareFrame() {
	WaitTillReady();
	commandPool.BeginBufferRecording(primaryCmdBuf, VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT);
}

void FrameObject::WaitTillReady() {
	if (!firstUse) {
		commandFence.WaitTillTrue();
		commandFence.Reset();
	}
	else {
		firstUse = false;
	}
}

void FrameObject::SubmitFrame()
{
	commandPool.EndBufferRecording(primaryCmdBuf);

}

VkSubmitInfo FrameObject::GetDepthSubmitInfo() {

	VkSubmitInfo submitInfo = initializers::submitInfo();
	//submitInfo.signalSemaphoreCount = 1;
	//submitInfo.pSignalSemaphores = renderFinishSem.GetPtr();
	//submitInfo.waitSemaphoreCount = 1;
	//submitInfo.pWaitSemaphores = imageAvailSem.GetPtr();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &depthCmdBuf;

	return submitInfo;
}

VkSubmitInfo FrameObject::GetSubmitInfo() {

	VkSubmitInfo submitInfo = initializers::submitInfo();
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = renderFinishSem.GetPtr();
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = imageAvailSem.GetPtr();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &primaryCmdBuf;

	return submitInfo;
}

VkPresentInfoKHR FrameObject::GetPresentInfo() {
	VkPresentInfoKHR presentInfo = {};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = renderFinishSem.GetPtr();

	presentInfo.pImageIndices = &swapChainIndex;
	return presentInfo;
}

VkCommandBuffer FrameObject::GetDepthCmdBuf() {
	return depthCmdBuf;
}

VkCommandBuffer FrameObject::GetPrimaryCmdBuf() {
	return primaryCmdBuf;
}

VkFence FrameObject::GetCommandFence() {
	return commandFence.Get();
}

VkFence FrameObject::GetDepthFence(){
	return depthFence.Get();
}