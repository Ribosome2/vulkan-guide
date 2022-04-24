
#include "vk_engine.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <vk_types.h>
#include <vk_initializers.h>

//bootstrap library
#include "VkBootstrap.h"

#include "iostream"
#include <fstream>
#include "PipelineBuilder.h"
//we want to abort when there is an error,
using namespace std;
#define VK_CHECK(x)                                            \
do{                                                            \
    VkResult err=x;                                            \
    if(err){                                                \
        std::cout<<"Detect Vulkan error:"<<err<<std::endl;    \
        abort();                                            \
    }                                                        \
}while(0)

void VulkanEngine::init() {
	// We initialize SDL and create a window with it. 
	SDL_Init(SDL_INIT_VIDEO);

	SDL_WindowFlags window_flags = (SDL_WindowFlags) (SDL_WINDOW_VULKAN);

	_window = SDL_CreateWindow(
			"Vulkan Engine",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			_windowExtent.width,
			_windowExtent.height,
			window_flags
	);

	//load the core vulkan structures
	init_vulkan();
	init_swapchain();
	init_commands();

	init_default_renderpass();
	init_frameBuffers();

	init_sync_structures();
	init_pipelines();

	//everything went fine
	_isInitialized = true;
}

void VulkanEngine::cleanup() {
	if (_isInitialized) {

		SDL_DestroyWindow(_window);
		vkDeviceWaitIdle(_device);
		_mainDeletionQueue.flush();



		vkDestroySurfaceKHR(_instance, _surface, nullptr);
		vkb::destroy_debug_utils_messenger(_instance, _debug_messenger);
		vkDestroyDevice(_device, nullptr);
		vkDestroyInstance(_instance, nullptr);
		SDL_DestroyWindow(_window);
	}
}

void VulkanEngine::draw() {
	//wait until the GPU has finished rendering the last frame. Timeout of 1 second
	const uint64_t timeout1Second = 1000000000;
	VK_CHECK(vkWaitForFences(_device, 1, &_renderFence, true, timeout1Second));
	VK_CHECK(vkResetFences(_device, 1, &_renderFence));

	//request image from the swapchain,
	uint32_t swapchainIndex;
	VK_CHECK(vkAcquireNextImageKHR(_device, _swapchain, timeout1Second, _presentSemaphore, nullptr, &swapchainIndex));

	//now that we are sure that the commands finished executing,we can safely reset the command buffer to begin recording again
	VK_CHECK(vkResetCommandBuffer(_mainCommandBuffer, 0));

	//naming it cmd for shorter writing
	VkCommandBuffer cmd = _mainCommandBuffer;

	//begin the command buffer recording. we will use this command buffer exactly once
	VkCommandBufferBeginInfo cmdBeginInfo = vkinit::command_buffer_begin_info(
			VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));
	{
		//make a clear-color from frame number, This will flash with 1 120*pi frame period
		VkClearValue clearValue;
		float flash = abs(sin(_frameNumber / 120.0f));
		clearValue.color = {{0.0f, 0.0f, flash, 1.0f}};

		//start the main renderpass
		//we will use the clear color from above, and the framebuffer of the index the swapchain gave us
		VkRenderPassBeginInfo rpInfo = vkinit::renderpass_begin_info(_renderPass, _windowExtent,
																	 _framebuffers[swapchainIndex]);

		//connect clear values
		rpInfo.clearValueCount = 1;
		rpInfo.pClearValues = &clearValue;
		vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
		{
			//once we start adding rendering commands ,they will be  here
			if(_selectedShader==0)
			{
				vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, _trianglePipeline);
			}else{
				vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,_redTrianglePipeline);
			}
			vkCmdDraw(cmd, 3, 1, 0, 0);
		}
		vkCmdEndRenderPass(cmd);
	}
	VK_CHECK(vkEndCommandBuffer(cmd));

	//prepare the submission to the queue
	//we want to wait on the _presentSemaphore, as that semaphore is signaled when the swapchain is ready
	//we will signal the _renderSemaphore, to signal that rendering has finished
	VkSubmitInfo submit = vkinit::submit_info(&cmd);

	VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submit.pWaitDstStageMask = &waitStage;
	submit.waitSemaphoreCount = 1;
	submit.pWaitSemaphores = &_presentSemaphore;

	submit.signalSemaphoreCount = 1;
	submit.pSignalSemaphores = &_renderSemaphore;

	//submit  command buffer to the queue and execute it.
	//_renderFence will now block until the graphic commands finish execution
	VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submit, _renderFence));


	//this will put the image we just rendered into the visible window.
	//we want to wait on the _renderSemaphore for that
	//as it's necessary that  drawing commands have finished before the image is displayed to the user
	VkPresentInfoKHR presentInfo = vkinit::present_info();

	presentInfo.pSwapchains = &_swapchain;
	presentInfo.swapchainCount = 1;

	presentInfo.pWaitSemaphores = &_renderSemaphore;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pImageIndices = &swapchainIndex;
	VK_CHECK(vkQueuePresentKHR(_graphicsQueue, &presentInfo));

	//increase the number of frames drawn
	_frameNumber++;
}

void VulkanEngine::run() {
	SDL_Event e;
	bool bQuit = false;

	//main loop
	while (!bQuit) {
		//Handle events on queue
		while (SDL_PollEvent(&e) != 0) {
			//close the window when user alt-f4s or clicks the X button			
			if (e.type == SDL_QUIT) {
				bQuit = true;
			} else if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_SPACE) {
					_selectedShader += 1;
					if (_selectedShader > 1) {
						_selectedShader = 0;
					}
				}else if(e.key.keysym.sym == SDLK_ESCAPE){
					bQuit=true;
				}
			}
		}

		draw();
	}
}

void VulkanEngine::init_vulkan() {
	vkb::InstanceBuilder builder;

	//make the vulkan instance , with basic debug features
	auto inst_ret = builder.set_app_name("Release the Vulkan!!!")
			.request_validation_layers(true)
			.require_api_version(1, 1, 0)
			.use_default_debug_messenger()
			.build();

	vkb::Instance vkb_inst = inst_ret.value();

	//store the instance
	_instance = vkb_inst.instance;
	_debug_messenger = vkb_inst.debug_messenger;

	//get the surface of the window we opened with SDL
	SDL_Vulkan_CreateSurface(_window, _instance, &_surface);

	//use vkBootstrap to select a GPU
	//we want a GPU that can write to the SDL surface and supports Vulkan 1.1
	vkb::PhysicalDeviceSelector deviceSelector{vkb_inst};
	vkb::PhysicalDevice physicalDevice = deviceSelector
			.set_minimum_version(1, 1)
			.set_surface(_surface)
			.select()
			.value();

	//create the final Vulkan device
	vkb::DeviceBuilder deviceBuilder{physicalDevice};
	vkb::Device vkbDevice = deviceBuilder.build().value();

	//get the VkDevice handle used in the rest of Vulkan application
	_device = vkbDevice.device;
	_chosenGPU = physicalDevice.physical_device;

	//use vkBootstrap strap to ge t a graphics queue
	_graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
	_graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

}

void VulkanEngine::init_swapchain() {
	vkb::SwapchainBuilder swapchainBuilder{_chosenGPU, _device, _surface};
	vkb::Swapchain vkbSwapchain = swapchainBuilder
			.use_default_format_selection()
					//use vsync present mode
			.set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
			.set_desired_extent(_windowExtent.width, _windowExtent.height)
			.build()
			.value();

	_swapchain = vkbSwapchain.swapchain;
	_swapchainImages = vkbSwapchain.get_images().value();
	_swapchainImageViews = vkbSwapchain.get_image_views().value();

	_swapchainImageFormat = vkbSwapchain.image_format;

	_mainDeletionQueue.push_function([=](){
		vkDestroySwapchainKHR(_device,_swapchain, nullptr);
	});
}

void VulkanEngine::init_commands() {
	//create  a command pool for commands submitted to  the graphics queue.
	//we also want the pool to allow  for resetting  of individual command buffers
	VkCommandPoolCreateInfo commandPoolInfo = vkinit::command_pool_create_info(_graphicsQueueFamily,
																			   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	VK_CHECK(vkCreateCommandPool(_device, &commandPoolInfo, nullptr, &_commandPool));

	//allocate the default command buffer that we will use for rendering
	VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_commandPool, 1);
	VK_CHECK(vkAllocateCommandBuffers(_device, &cmdAllocInfo, &_mainCommandBuffer));

	_mainDeletionQueue.push_function([=](){
		vkDestroyCommandPool(_device,_commandPool, nullptr);
	});
}

void VulkanEngine::init_default_renderpass() {
	//the renderpass will use this color attachment
	VkAttachmentDescription color_attachment = {};
	//the attachment will have the format needed by the swapchain;
	color_attachment.format = _swapchainImageFormat;
	//1 sample ,we won't be doing MSAA
	color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
	//we clear when the attachment is loaded
	color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	//we keep the attachment stored when the renderpass ends
	color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	//dont' care about stencil
	color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	//we don't know or care about the starting layout of the attachment
	color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	//after the renderpass ends, wht image has to be on a layout ready for display
	color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	VkAttachmentReference color_attachment_ref = {};
	//attachment number wil index into the pAttachment array in the parent renderpass itself
	color_attachment_ref.attachment = 0;
	color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	//we are going to creat a subpass ,which is the minimum you can do
	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &color_attachment_ref;

	VkRenderPassCreateInfo render_pass_info = {};
	render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;

	//connect the color attachment to the info
	render_pass_info.attachmentCount = 1;
	render_pass_info.pAttachments = &color_attachment;
	//connect the subpass to the info
	render_pass_info.subpassCount = 1;
	render_pass_info.pSubpasses = &subpass;
	VK_CHECK(vkCreateRenderPass(_device, &render_pass_info, nullptr, &_renderPass));

	_mainDeletionQueue.push_function([=](){
		vkDestroyRenderPass(_device,_renderPass, nullptr);
	});
}

void VulkanEngine::init_frameBuffers() {
	//create the framebuffers  for the swapchain images. this will connect the render-pass to the images  for rendering
	VkFramebufferCreateInfo fb_info = {};
	fb_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	fb_info.pNext = nullptr;

	fb_info.renderPass = _renderPass;
	fb_info.attachmentCount = 1;
	fb_info.width = _windowExtent.width;
	fb_info.height = _windowExtent.height;
	fb_info.layers = 1;

	//grab how manny images we have in the swapchain
	const size_t swapchain_image_count = _swapchainImages.size();
	_framebuffers = std::vector<VkFramebuffer>(swapchain_image_count);

	//create framebuffers for each of the swapchain image views
	for (int i = 0; i < swapchain_image_count; i++) {
		fb_info.pAttachments = &_swapchainImageViews[i];
		VK_CHECK(vkCreateFramebuffer(_device, &fb_info, nullptr, &_framebuffers[i]));

		_mainDeletionQueue.push_function([=](){
			vkDestroyFramebuffer(_device,_framebuffers[i], nullptr);
			vkDestroyImageView(_device,_swapchainImageViews[i], nullptr);
		});
	}
}

void VulkanEngine::init_sync_structures() {
	VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
	VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_renderFence));

	_mainDeletionQueue.push_function([=](){
		vkDestroyFence(_device,_renderFence, nullptr);
	});

	VkSemaphoreCreateInfo semaphoreCreateInfo=vkinit::semaphore_create_info();

	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_presentSemaphore));
	VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_renderSemaphore));

	//enqueue the destruction of semaphores
	_mainDeletionQueue.push_function([=](){
		vkDestroySemaphore(_device,_presentSemaphore, nullptr);
		vkDestroySemaphore(_device,_renderSemaphore, nullptr);
	});
}

bool VulkanEngine::load_shader_module(const char *filePath, VkShaderModule *outShaderModule) {
	//open the file. With cursor at the end
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		return false;
	}
	//find what the size of the file is by looking up the location of the cursor
	//because the cursor is at the end, it gives the size directly in bytes
	size_t fileSize = (size_t) file.tellg();

	//spirv expects the buffer to be on uint32, so make sure to reserve an int vector big enough for the entire file
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

	//put file cursor at beginning
	file.seekg(0);

	//load the entire file into the buffer
	file.read((char *) buffer.data(), fileSize);

	//now that the file is loaded into the buffer, we can close it
	file.close();

	//create a new shader module, using the buffer we loaded
	VkShaderModuleCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	createInfo.pNext = nullptr;

	//codeSize has to be in bytes, so multiply the ints in the buffer by size of int to know the real size of the buffer
	createInfo.codeSize = buffer.size() * sizeof(uint32_t);
	createInfo.pCode = buffer.data();

	//check that the creation goes well.
	VkShaderModule shaderModule;
	if (vkCreateShaderModule(_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
		return false;
	}
	*outShaderModule = shaderModule;
	return true;
}

static void compileShader(std::string shaderPath, std::string outputPath) {
	char command[1024];
	sprintf_s(command, "glslc.exe %s -o %s", shaderPath.c_str(), outputPath.c_str());
	auto result = system(command);
	if (result != 0) {
		std::cout << "compile shader failed " << shaderPath << std::endl;
	}
}

void VulkanEngine::init_pipelines() {
	std::string vert_shader_src_path = "../shaders/colored_triangle.vert";
	std::string frag_shader_src_path = "../shaders/colored_triangle.frag";

	std::string frag_shader_spv_path = "../spv_files/colored_triangle.frag.spv";
	std::string vert_shader_spv_path = "../spv_files/colored_triangle.vert.spv";

	compileShader(vert_shader_src_path, vert_shader_spv_path);
	compileShader(frag_shader_src_path, frag_shader_spv_path);
	VkShaderModule triangleFragShader;
	if (!load_shader_module(frag_shader_spv_path.c_str(), &triangleFragShader)) {
		std::cerr << "Error when building the triangle fragment shader module" << std::endl;
	} else {
		std::cout << "Triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule triangleVertexShader;
	if (!load_shader_module(vert_shader_spv_path.c_str(), &triangleVertexShader)) {
		std::cerr << "Error when building the triangle vertex shader module" << std::endl;
	} else {
		std::cout << "Triangle vertex shader successfully loaded" << std::endl;
	}

	std::string redTriangleVertShaderSrc = "../shaders/triangle.vert";
	std::string redTriangleFragShaderSrc = "../shaders/triangle.frag";

	std::string redTriangleFragShaderSPV = "../spv_files/triangle.frag.spv";
	std::string redTriangleVertShaderSPV = "../spv_files/triangle.vert.spv";

	compileShader(redTriangleVertShaderSrc, redTriangleVertShaderSPV);
	compileShader(redTriangleFragShaderSrc, redTriangleFragShaderSPV);
	VkShaderModule redTriangleFragShader;
	if (!load_shader_module(redTriangleFragShaderSPV.c_str(), &redTriangleFragShader)) {
		std::cerr << "Error when building the triangle fragment shader module" << std::endl;
	} else {
		std::cout << "Triangle fragment shader successfully loaded" << std::endl;
	}

	VkShaderModule redTriangleVertexShader;
	if (!load_shader_module(redTriangleVertShaderSPV.c_str(), &redTriangleVertexShader)) {
		std::cerr << "Error when building the triangle vertex shader module" << std::endl;
	} else {
		std::cout << "Triangle vertex shader successfully loaded" << std::endl;
	}

	//build the pipeline layout that controls the inputs/outputs of the shader
	//we are not using descriptor sets or other system yet, so no need to use anything other than empty default
	VkPipelineLayoutCreateInfo pipeline_layout_info = vkinit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &_trianglePipelineLayout));

	//build the stage-create-info for both vertex and fragment stages. This lets the pipeline know the shader modules per stage
	PipelineBuilder pipelineBuilder;
	pipelineBuilder._shaderStages.push_back(
			vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, triangleVertexShader)
	);
	pipelineBuilder._shaderStages.push_back(
			vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, triangleFragShader)
	);
	//vertex input controls how to read vertices from vertex buffers. We aren't using it yet
	pipelineBuilder._vertexInputInfo = vkinit::vertex_input_state_create_info();

	//input assembly is the configuration for drawing triangle lists,strips,or individual points
	//we are just going to draw triangle list
	pipelineBuilder._inputAssembly = vkinit::input_assembly_create_info(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
	//build viewport and scissor from the swapchain extents
	pipelineBuilder._viewport.x = 0.0f;
	pipelineBuilder._viewport.y = 0.0f;
	pipelineBuilder._viewport.width = (float) _windowExtent.width;
	pipelineBuilder._viewport.height = (float) _windowExtent.height;
	pipelineBuilder._viewport.minDepth = 0.0f;
	pipelineBuilder._viewport.maxDepth = 1.0f;

	pipelineBuilder._scissor.offset = {0, 0};
	pipelineBuilder._scissor.extent = _windowExtent;

	//configure the rasterizer to draw filled triangles
	pipelineBuilder._rasterizer = vkinit::rasterization_state_create_info(VK_POLYGON_MODE_FILL);

	//we don't use multisampling, so just run the default one
	pipelineBuilder._multisampling = vkinit::multisampling_state_create_info();

	//a single blend attachment with no blending and writing to RGBA
	pipelineBuilder._colorBlendAttachment = vkinit::color_blend_attachment_state();

	//use the triangle layout we created
	pipelineBuilder._pipelineLayout = _trianglePipelineLayout;

	//finally, build the pipeline
	_trianglePipeline = pipelineBuilder.build_pipeline(_device, _renderPass);
	vkDestroyShaderModule(_device, triangleFragShader, nullptr);
	vkDestroyShaderModule(_device, triangleVertexShader, nullptr);

	//clear the shader stages for the builder
	pipelineBuilder._shaderStages.clear();
	//add the other shaders
	pipelineBuilder._shaderStages.push_back(
			vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT,redTriangleVertexShader)
			);
	pipelineBuilder._shaderStages.push_back(
			vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT,redTriangleFragShader)
			);

	//build the red triangle pipeline
	_redTrianglePipeline=pipelineBuilder.build_pipeline(_device,_renderPass);

	vkDestroyShaderModule(_device, redTriangleFragShader, nullptr);
	vkDestroyShaderModule(_device, redTriangleVertexShader, nullptr);

	_mainDeletionQueue.push_function([=](){
		//destroy the 2 pipelines we have created
		vkDestroyPipeline(_device, _trianglePipeline, nullptr);
		vkDestroyPipeline(_device, _redTrianglePipeline, nullptr);

		//destroy the pipeline layout that they use
		vkDestroyPipelineLayout(_device, _trianglePipelineLayout, nullptr);
	});


}
