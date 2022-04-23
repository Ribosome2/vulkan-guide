﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 800 , 600 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	//shuts down the engine
	void cleanup();

	//draw loop
	void draw();

	//run main loop
	void run();

private:
	void init_vulkan();
	void init_swapchain();
	void init_commands();
	void init_default_renderpass();
	void init_frameBuffers();
private:
	//--- omitted ---
	VkInstance _instance;
	VkDebugUtilsMessengerEXT  _debug_messenger; //Vulkan debug output handle
	VkPhysicalDevice _chosenGPU;
	VkDevice _device;
	VkSurfaceKHR _surface;

	//swapchain
	VkSwapchainKHR  _swapchain;
	//image format expected by the window system
	VkFormat _swapchainImageFormat;
	std::vector<VkImage >	_swapchainImages;
	std::vector<VkImageView> _swapchainImageViews;

	//for commands
	VkQueue _graphicsQueue; //queue we will submit to
	uint32_t _graphicsQueueFamily; //family of that queue
	VkCommandPool _commandPool;//that command pool for out commands
	VkCommandBuffer _mainCommandBuffer; //the buffer we will record into

	//for RenderPass
	VkRenderPass _renderPass;
	std::vector<VkFramebuffer > _framebuffers;


};
