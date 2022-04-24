﻿// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <vk_types.h>
#include <vector>
#include <deque>
#include <functional>
#include <vk_mesh.h>
#include <glm/glm.hpp>
#include <unordered_map>


struct Material{
	VkPipeline  pipeline;
	//note that we store the VKPipeline and layout by value ,not pointer
	//They are 64 bit handles to internal driver structures anyway so storing pointers to them isn't very helpful
	VkPipelineLayout pipelineLayout;
};

struct RenderObjcet{
	Mesh * mesh ;
	Material* material;
	glm::mat4 transformMatrix;
};


struct MeshPushConstants{
	glm::vec4 data;
	glm::mat4 render_matrix;
};

struct DeletionQueue{
	std::deque<std::function<void()>> deletors;

	void push_function(std::function<void()> && function){
		deletors.push_back(function);
	}

	void flush(){
		//reverse iterate the deletion queue to execute all the functions
		for(auto it =deletors.rbegin();it!=deletors.rend();it++){
			(*it)();
		}
		deletors.clear();
	}
};

class VulkanEngine {
public:

	bool _isInitialized{ false };
	int _frameNumber {0};

	VkExtent2D _windowExtent{ 800 , 600 };

	struct SDL_Window* _window{ nullptr };

	//initializes everything in the engine
	void init();

	void init_scene();

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
	void init_sync_structures();
	void init_pipelines();
	//loads a shader module from a spir-v file. Returns false if it errors
	bool load_shader_module(const char* filePath, VkShaderModule* outShaderModule);
	void load_meshes();
	void upload_mesh(Mesh& mesh);

	//create material and add it to the map
	Material* create_material(VkPipeline pipeline,VkPipelineLayout layout,const std::string& name);

	//returns nullptr if it can't be found
	Material* get_material(const std::string & name);
	Mesh* get_mesh(const std::string& name);
	void draw_objects(VkCommandBuffer cmd,RenderObjcet * first, int count);

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

	//for render loop
	VkSemaphore _presentSemaphore,_renderSemaphore;
	VkFence _renderFence;

	//pipelines
	VkPipelineLayout _trianglePipelineLayout;
	VkPipeline _trianglePipeline;
	VkPipeline _redTrianglePipeline;
	VkPipeline _meshPipeline;
	VkPipelineLayout _meshPipelineLayout;

	//for depth buffer
	VkImageView _depthImageView;
	AllocatedImage _depthImage;
	VkFormat _depthFormat;


	int _selectedShader{0};

	VmaAllocator _allocator; //vma lib allocator
  	Mesh _triangleMesh;
  	Mesh _monkeyMesh;
	//default array of renderable objects
	std::vector<RenderObjcet> _renderables;
	std::unordered_map<std::string ,Material> _materials;
	std::unordered_map<std::string ,Mesh> _meshes;

	DeletionQueue _mainDeletionQueue;

};
