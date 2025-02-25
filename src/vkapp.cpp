
#include <array>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream


#ifdef WIN64
#else
#include <unistd.h>
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "vkapp.h"

#include "app.h"

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>

VkApp::VkApp(App* _app) : app(_app)
{
    uint32_t version;
    vkEnumerateInstanceVersion(&version);
    printf("SDK Version: %d.%d.%d\n", VK_API_VERSION_MAJOR(version),
           VK_API_VERSION_MINOR(version), VK_API_VERSION_PATCH(version));

    createInstance(app->doApiDump);	// -> m_instance
    assert (m_instance);
    createPhysicalDevice();		// -> m_physicalDevice i.e. the GPU
    chooseQueueIndex();		// -> m_graphicsQueueIndex
    createDevice();			// -> m_device
    getCommandQueue();		// -> m_queue
    createCommandPool();		// -> m_cmdPool
    loadExtensions();		// Auto generated; loads namespace of all known extensions
    getSurface();			// -> m_surface
    
    createSwapchain();		// -> m_swapchain
    createDepthResource();		// -> m_depthImage, ...
    createPostRenderPass();		// -> m_postRenderPass
    createPostFrameBuffers();	// -> m_framebuffers

    createRenderTarget();		// -> m_renderTarget
    createPostDescriptor();		// -> m_postDesc
    createPostPipeline();		// -> m_postPipelineLayout

    #ifdef GUI
    initGUI();
    #endif
    
    // Load model and create related entities
    loadModel();
    createMatrixBuffer();
    createObjDescriptionBuffer();
    
    // Scanline: Initialize scanline capabilities
    createScRenderPass();
    createScDescriptorSet();
    createScPipeline();

    // Raycasting ...: Initialize ray tracing capabilities
    createRtBuffers();
    initRayTracing();
    createRtAccelerationStructure();
    createRtDescriptorSet();
    createRtPipeline();
    createRtShaderBindingTable();

    // Denoising: Initialize denoising capabilities
    createDenoiseBuffer();
    createDenoiseDescriptorSet();
    createDenoiseCompPipeline();

}

void VkApp::drawFrame()
{
     prepareFrame();
    
     VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
     beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
     vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
    
    {   // Extra indent for code clarity
        updateCameraBuffer();
        
        // Draw scene
        if (useRaytracer) {
             raytrace();
             denoise();
         } else
             rasterize();
        
        postProcess(); //  tone mapper and output to swapchain image.
    }   // Done recording;  Execute!
    
    vkEndCommandBuffer(m_commandBuffer);
    submitFrame();  // Submit for display
}


VkCommandBuffer VkApp::createTempCmdBuffer()
{
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandBufferCount = 1;
    allocateInfo.commandPool        = m_cmdPool;
    allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    VkCommandBuffer cmdBuffer;
    vkAllocateCommandBuffers(m_device, &allocateInfo, &cmdBuffer);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);
    return cmdBuffer;
}

void VkApp::submitTempCmdBuffer(VkCommandBuffer cmdBuffer)
{
    vkEndCommandBuffer(cmdBuffer);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &cmdBuffer;
    vkQueueSubmit(m_queue, 1, &submitInfo, {});
    vkQueueWaitIdle(m_queue);
    vkFreeCommandBuffers(m_device, m_cmdPool, 1, &cmdBuffer);
}

void VkApp::prepareFrame()
{
    // Use a fence to wait until the command buffer has finished execution before using it again
    vkWaitForFences(m_device, 1, &m_waitFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_waitFence);
        
    // Acquire the next image from the swap chain --> m_swapchainIndex
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_readSemaphore,
                                            (VkFence)VK_NULL_HANDLE, &m_swapchainIndex);

    // Check if window has been resized -- or other(??) swapchain specific event
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSizedResources(m_windowSize); }

}

void VkApp::submitFrame()
{
    //vkResetFences(m_device, 1, &m_waitFence);

    // Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
    const VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
     
    // The submit info structure specifies a command buffer queue submission batch
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.pNext             = nullptr;
    submitInfo.pWaitDstStageMask = &waitStageMask; //  pipeline stages to wait for
    submitInfo.waitSemaphoreCount   = 1;  
    submitInfo.pWaitSemaphores = &m_readSemaphore;  // waited upon before execution
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &m_writtenSemaphore; // signaled when execution finishes
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &m_commandBuffer;
    if (vkQueueSubmit(m_queue, 1, &submitInfo, m_waitFence) != VK_SUCCESS) {
      throw std::runtime_error("Failed to submit command buffer to the queue!");
    }
    
    // Present frame
    VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &m_writtenSemaphore;;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &m_swapchain;
    presentInfo.pImageIndices      = &m_swapchainIndex;

    if (vkQueuePresentKHR(m_queue, &presentInfo) != VK_SUCCESS) {
      throw std::runtime_error("Failed to present the swapchain image to the screen!");
    }

    printf("Frame submitted and presented successfully.\n");
}


VkShaderModule VkApp::createShaderModule(std::string code)
{
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize                 = code.size();
    createInfo.pCode                    = (uint32_t*) code.data();

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule);

    return shaderModule;

}

VkPipelineShaderStageCreateInfo VkApp::createShaderStageInfo(const std::string&    code,
                                                                   VkShaderStageFlagBits stage,
                                                                   const char* entryPoint)
{
    VkPipelineShaderStageCreateInfo shaderStage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    shaderStage.stage  = stage;
    shaderStage.module = createShaderModule(code);
    shaderStage.pName  = entryPoint;
    return shaderStage;
}

#ifdef GUI
void VkApp::initGUI()
{
    uint subpassID = 0;
    
    // UI
    ImGui::CreateContext();
    ImGuiIO& io    = ImGui::GetIO();
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls

    std::vector<VkDescriptorPoolSize> poolSize{{VK_DESCRIPTOR_TYPE_SAMPLER, 1}, 
                                               {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};
    
    VkDescriptorPoolCreateInfo        poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.maxSets       = 2;
    poolInfo.poolSizeCount = 2;
    poolInfo.pPoolSizes    = poolSize.data();
    vkCreateDescriptorPool(m_device, &poolInfo, nullptr, &m_imguiDescPool);

    // Setup Platform/Renderer back ends
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance                  = m_instance;
    init_info.PhysicalDevice            = m_physicalDevice;
    init_info.Device                    = m_device;
    init_info.QueueFamily               = m_graphicsQueueIndex;
    init_info.Queue                     = m_queue;
    init_info.PipelineCache             = VK_NULL_HANDLE;
    init_info.DescriptorPool            = m_imguiDescPool;
    init_info.Subpass                   = subpassID;
    init_info.MinImageCount             = 2;
    init_info.ImageCount                = m_imageCount;
    init_info.MSAASamples               = VK_SAMPLE_COUNT_1_BIT;
    init_info.CheckVkResultFn           = nullptr;
    init_info.Allocator                 = nullptr;

    ImGui_ImplVulkan_Init(&init_info, m_postRenderPass);

    // Upload Fonts
    VkCommandBuffer cmdbuf = createTempCmdBuffer();
    ImGui_ImplVulkan_CreateFontsTexture(cmdbuf);
    submitTempCmdBuffer(cmdbuf);
    
    ImGui_ImplGlfw_InitForVulkan(app->GLFW_window, true);
}
#endif
