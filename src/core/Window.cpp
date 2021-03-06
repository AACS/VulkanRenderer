#include "Window.h"

#include <mutex>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>

#include "../../third-party/ImGui/imgui.h"
#include "../gui/ImGuiImpl.h"

#include "Input.h"
#include "Logger.h"

Window::Window(bool isFullscreen, const glm::ivec2& size, const glm::ivec2& position) {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	if (isFullscreen) {
		GLFWmonitor* primary = glfwGetPrimaryMonitor();
		const GLFWvidmode* mode = glfwGetVideoMode(primary);

		window = glfwCreateWindow(mode->width, mode->height, "Vulkan Renderer", primary, NULL);

		Log::Debug << "Monitor Width " << mode->width << "\n";
		Log::Debug << "Monitor Height " << mode->height << "\n";

	}
	else {
		window = glfwCreateWindow(size.x, size.y, "Vulkan Renderer", NULL, NULL);
		if (position != glm::ivec2{ INT_MIN, INT_MIN }) {
			glfwSetWindowPos(window, position.x, position.y);
		}

	}

	//Prepare window
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, FramebufferSizeHandler);
	glfwSetWindowSizeCallback(window, WindowResizeHandler);
	glfwSetWindowIconifyCallback(window, WindowIconifyHandler);
	glfwSetWindowFocusCallback(window, WindowFocusHandler);
	glfwSetWindowCloseCallback(window, WindowCloseHandler);
	glfwSetErrorCallback(ErrorHandler);

	//Set input callbacks
	glfwSetKeyCallback(window, KeyboardHandler);
	glfwSetCharCallback(window, CharInputHandler);
	glfwSetMouseButtonCallback(window, MouseButtonHandler);
	glfwSetCursorPosCallback(window, MouseMoveHandler);
	glfwSetScrollCallback(window, MouseScrollHandler);
	glfwSetJoystickCallback(JoystickConfigurationChangeHandler);

	isWindowIconofied = glfwGetWindowAttrib(window, GLFW_ICONIFIED);
	isWindowFocused = glfwGetWindowAttrib(window, GLFW_FOCUSED);

	currentWindowSize = GetWindowSize();
}

Window::~Window() {
	glfwDestroyWindow(window);
	glfwTerminate();
	window = nullptr;
}

void Window::setSizeLimits(const glm::ivec2& minSize, const glm::ivec2& maxSize) {
	glfwSetWindowSizeLimits(window, minSize.x, minSize.y,
		maxSize.x ? maxSize.x : minSize.x,
		maxSize.y ? maxSize.y : minSize.y);
}

void Window::showWindow(bool show) {
	if (show) {
		glfwShowWindow(window);
	}
	else {
		glfwHideWindow(window);
	}
}

GLFWwindow* Window::getWindowContext() {
	return window;
}

bool Window::CheckForWindowResizing() {
	return updateWindowSize;
}

void Window::SetWindowResizeDone() {
	updateWindowSize = false;
}


bool Window::CheckForWindowIconified() {
	return isWindowIconofied;
}

bool Window::CheckForWindowFocus() {
	return isWindowFocused;
}

bool Window::CheckForWindowClose() {
	return shouldCloseWindow;
}

void Window::SetWindowToClose() {
	shouldCloseWindow = true;
	glfwSetWindowShouldClose(window, true);
}

glm::ivec2 Window::GetWindowSize() {
	int width, height;
	glfwGetWindowSize(window, &width, &height);
	return glm::ivec2(width, height);
}

void Window::ErrorHandler(int error, const char* description)
{
	Log::Error << description << "\n";
	puts(description);
}

void Window::KeyboardHandler(GLFWwindow* window, int key, int scancode, int action, int mods) {
	Input::inputDirector.keyEvent(key, scancode, action, mods);
	ImGui_ImplGlfwVulkan_KeyCallback(window, key, scancode, action, mods);
}

void Window::CharInputHandler(GLFWwindow* window, uint32_t codePoint) {

	ImGui_ImplGlfwVulkan_CharCallback(window, codePoint);
}

void Window::MouseButtonHandler(GLFWwindow* window, int button, int action, int mods) {
	Input::inputDirector.mouseButtonEvent(button, action, mods);
	ImGui_ImplGlfwVulkan_MouseButtonCallback(window, button, action, mods);
}

void Window::MouseMoveHandler(GLFWwindow* window, double posx, double posy) {
	Input::inputDirector.mouseMoveEvent(posx, posy);
}

void Window::MouseScrollHandler(GLFWwindow* window, double xoffset, double yoffset) {
	Input::inputDirector.mouseScrollEvent(xoffset, yoffset);
	ImGui_ImplGlfwVulkan_ScrollCallback(window, xoffset, yoffset);
}

void Window::JoystickConfigurationChangeHandler(int joy, int event)
{
	if (event == GLFW_CONNECTED)
	{
		Log::Debug << "Controller " << joy << " Connected \n";
		Input::ConnectJoystick(joy);
	}
	else if (event == GLFW_DISCONNECTED)
	{
		Log::Debug << "Controller " << joy << " Disconnected \n";
		Input::DisconnectJoystick(joy);
	}
}

void Window::WindowCloseHandler(GLFWwindow* window) {
	Window* w = (Window*)glfwGetWindowUserPointer(window);
	w->SetWindowToClose();
}

void Window::FramebufferSizeHandler(GLFWwindow* window, int width, int height) {
	Window* w = (Window*)glfwGetWindowUserPointer(window);

	w->updateWindowSize = true;
}

void Window::WindowResizeHandler(GLFWwindow* window, int width, int height) {
	if (width == 0 || height == 0) return;

	Window* w = (Window*)glfwGetWindowUserPointer(window);
	//glfwSetWindowSize(window, width, height);
	w->updateWindowSize = true;

}

void Window::WindowIconifyHandler(GLFWwindow* window, int iconified) {

	Window* w = (Window*)glfwGetWindowUserPointer(window);
	if (iconified)
	{
		w->isWindowIconofied = true;
	}
	else
	{
		w->isWindowIconofied = false;
	}
}

void Window::WindowFocusHandler(GLFWwindow* window, int focused) {
	Window* w = (Window*)glfwGetWindowUserPointer(window);
	if (focused)
	{
		w->isWindowFocused = true;
	}
	else
	{
		w->isWindowFocused = false;
	}
}

std::vector<const char*> GetWindowExtensions() {

	std::vector<const char*> extensions;

	unsigned int glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

	for (unsigned int i = 0; i < glfwExtensionCount; i++) {
		extensions.push_back(glfwExtensions[i]);
	}

	return extensions;
}