#pragma once
// ImGui GLFW binding with Vulkan + shaders
// FIXME: Changes of ImTextureID aren't supported by this binding! Please, someone add it!

// You can copy and use unmodified imgui_impl_* files in your project. See main.cpp for an example of using this.
// If you use this binding you'll need to call 5 functions: ImGui_ImplXXXX_Init(), ImGui_ImplXXX_CreateFontsTexture(), ImGui_ImplXXXX_NewFrame(), ImGui_ImplXXXX_Render() and ImGui_ImplXXXX_Shutdown().
// If you are new to ImGui, see examples/README.txt and documentation at the top of imgui.cpp.
// https://github.com/ocornut/imgui

#include <memory>
#include <vulkan/vulkan.h>

//forward declarations
class Window;
class VulkanRenderer;
struct GLFWwindow;

#define IMGUI_VK_QUEUED_FRAMES 2

struct ImGui_ImplGlfwVulkan_Init_Data
{
	VkAllocationCallbacks* allocator;
	VkPhysicalDevice       gpu;
	VkDevice               device;
	VkRenderPass           render_pass;
	VkPipelineCache        pipeline_cache;
	VkDescriptorPool       descriptor_pool;
	void(*check_vk_result)(VkResult err);
};

bool        ImGui_ImplGlfwVulkan_Init(GLFWwindow* window, bool install_callbacks, ImGui_ImplGlfwVulkan_Init_Data *init_data);
void        ImGui_ImplGlfwVulkan_Shutdown();
void        ImGui_ImplGlfwVulkan_NewFrame();
void        ImGui_ImplGlfwVulkan_Render(VkCommandBuffer command_buffer);

// Use if you want to reset your rendering device without losing ImGui state.
void        ImGui_ImplGlfwVulkan_InvalidateFontUploadObjects();
void        ImGui_ImplGlfwVulkan_InvalidateDeviceObjects();
bool        ImGui_ImplGlfwVulkan_CreateFontsTexture(VkCommandBuffer command_buffer);
bool        ImGui_ImplGlfwVulkan_CreateDeviceObjects();

// GLFW callbacks (installed by default if you enable 'install_callbacks' during initialization)
// Provided here if you want to chain callbacks.
// You can also handle inputs yourself and use those as a reference.
void        ImGui_ImplGlfwVulkan_MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void        ImGui_ImplGlfwVulkan_ScrollCallback(GLFWwindow* window, double xoffset, double yoffset);
void        ImGui_ImplGlfwVulkan_KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
void        ImGui_ImplGlfwVulkan_CharCallback(GLFWwindow* window, unsigned int c);

void PrepareImGui(GLFWwindow* window, VulkanRenderer* vulkanRenderer);