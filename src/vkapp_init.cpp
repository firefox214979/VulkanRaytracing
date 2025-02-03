

#include <array>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include <unordered_set>
#include <unordered_map>

#include "vkapp.h"
#include "app.h"
#include "extensions_vk.hpp"

#ifdef GUI
#include "backends/imgui_impl_glfw.h"
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#endif


void VkApp::destroyAllVulkanResources()
{
    vkWaitForFences(m_device, 1, &m_waitFence, VK_TRUE, UINT64_MAX);
    vkResetFences(m_device, 1, &m_waitFence);
    vkDeviceWaitIdle(m_device);
    
    #ifdef GUI
    vkDestroyDescriptorPool(m_device, m_imguiDescPool, nullptr);
    ImGui_ImplVulkan_Shutdown();
    #endif

    vkDestroyPipelineLayout(m_device, m_denoiseCompPipelineLayout, nullptr);
    vkDestroyPipeline(m_device, m_denoisePipeline, nullptr);
    m_denoiseDesc.destroy(m_device);
    m_denoiseBuffer.destroy(m_device);

    m_shaderBindingTableBuff.destroy(m_device);

    m_lightBuff.destroy(m_device);

    m_shaderBindingTableBuff.destroy(m_device);
    printf("Shader binding table buffer destroyed.\n");

    if (m_rtPipelineLayout != VK_NULL_HANDLE)
    {
      vkDestroyPipelineLayout(m_device, m_rtPipelineLayout, nullptr);
      printf("Rt pipeline layout destroyed.\n");
    }

    if (m_rtPipeline != VK_NULL_HANDLE)
    {
      vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
      printf("Rt pipeline destroyed.\n");
    }

    m_rtDesc.destroy(m_device);
    printf("Rt Descriptor destroyed.\n");

    m_rtBuilder.destroy();
    printf("Rt Builder destroyed.\n");

    m_rtColCurrBuffer.destroy(m_device);
    m_rtColPrevBuffer.destroy(m_device);
    m_rtKdCurrBuffer.destroy(m_device);
    m_rtKdPrevBuffer.destroy(m_device);
    m_rtNdCurrBuffer.destroy(m_device);
    m_rtNdPrevBuffer.destroy(m_device);
    printf("Rt Buffers destroyed.\n");

    m_postDesc.destroy(m_device);
    printf("Post descriptor set destroyed.\n");

    if (m_renderTarget.image != VK_NULL_HANDLE) {
      m_renderTarget.destroy(m_device);
      printf("Render target destroyed.\n");
    }

    if (m_scPipelineLayout != VK_NULL_HANDLE) 
    {
      vkDestroyPipelineLayout(m_device, m_scPipelineLayout, nullptr);
      printf("Sc pipeline layout destroyed.\n");
    }

    if (m_scPipeline != VK_NULL_HANDLE) 
    {
      vkDestroyPipeline(m_device, m_scPipeline, nullptr);
      printf("Sc pipeline destroyed.\n");
    }

    m_scDesc.destroy(m_device);
    printf("Sc descriptor set destroyed.\n");

    if (m_scRenderPass != VK_NULL_HANDLE) 
    {
      vkDestroyRenderPass(m_device, m_scRenderPass, nullptr);
      printf("Sc Render pass destroyed.\n");
    }

    if (m_scFramebuffer != VK_NULL_HANDLE) 
    {
      vkDestroyFramebuffer(m_device, m_scFramebuffer, nullptr);
      printf("Sc Framebuffer destroyed.\n");
    }

    if (m_objDescriptionBuff.buffer != VK_NULL_HANDLE) 
    {
      m_objDescriptionBuff.destroy(m_device);
      printf("Object description buffer destroyed.\n");
    }

    if (m_matrixBuff.buffer != VK_NULL_HANDLE) 
    {
      m_matrixBuff.destroy(m_device);
      printf("Matrix buffer destroyed.\n");
    }

    if (m_readSemaphore != VK_NULL_HANDLE) 
    {
      vkDestroySemaphore(m_device, m_readSemaphore, nullptr);
      printf("Read semaphore destroyed.\n");
      m_readSemaphore = VK_NULL_HANDLE;
    }

    if (m_writtenSemaphore != VK_NULL_HANDLE) 
    {
      vkDestroySemaphore(m_device, m_writtenSemaphore, nullptr);
      printf("Written semaphore destroyed.\n");
      m_writtenSemaphore = VK_NULL_HANDLE;
    }

    if (m_waitFence != VK_NULL_HANDLE) 
    {
      vkDestroyFence(m_device, m_waitFence, nullptr);
      printf("Wait fence destroyed.\n");
      m_waitFence = VK_NULL_HANDLE;
    }

    for (auto framebuffer : m_framebuffers) 
    {
      if (framebuffer != VK_NULL_HANDLE) 
      {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
      }
    }
    m_framebuffers.clear();
    printf("All framebuffers destroyed.\n");

    for (auto imageView : m_imageViews) 
    {
      if (imageView != VK_NULL_HANDLE) 
      {
        vkDestroyImageView(m_device, imageView, nullptr);
      }
    }
    m_imageViews.clear();
    printf("All image views destroyed.\n");

    if (m_swapchain != VK_NULL_HANDLE) 
    {
      vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
      printf("Swapchain destroyed.\n");
      m_swapchain = VK_NULL_HANDLE;
    }

    m_depthImage.destroy(m_device);
    printf("Depth image resources destroyed.\n");

    if (m_postRenderPass != VK_NULL_HANDLE) 
    {
      vkDestroyRenderPass(m_device, m_postRenderPass, nullptr);
      printf("Post render pass destroyed.\n");
      m_postRenderPass = VK_NULL_HANDLE;
    }

    if (m_postPipelineLayout != VK_NULL_HANDLE) 
    {
      vkDestroyPipelineLayout(m_device, m_postPipelineLayout, nullptr);
      printf("Post pipeline layout destroyed.\n");
      m_postPipelineLayout = VK_NULL_HANDLE;
    }

    if (m_postPipeline != VK_NULL_HANDLE) 
    {
      vkDestroyPipeline(m_device, m_postPipeline, nullptr);
      printf("Post pipeline destroyed.\n");
      m_postPipeline = VK_NULL_HANDLE;
    }

    if (m_cmdPool != VK_NULL_HANDLE) 
    {
      vkDestroyCommandPool(m_device, m_cmdPool, nullptr);
      printf("Command pool destroyed.\n");
      m_cmdPool = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE) 
    {
      vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
      printf("Surface destroyed.\n");
      m_surface = VK_NULL_HANDLE;
    }

    if (m_device != VK_NULL_HANDLE) 
    {
      vkDestroyDevice(m_device, nullptr);
      printf("Device destroyed.\n");
      m_device = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) 
    {
      vkDestroyInstance(m_instance, nullptr);
      printf("Instance destroyed.\n");
      m_instance = VK_NULL_HANDLE;
    }

    printf("All Vulkan resources destroyed.\n");
}

void VkApp::recreateSizedResources(VkExtent2D size)
{
    assert(false && "Not ready for resize events.");
    // Destroy everything related to the window size
    // (RE)Create them all at the new size
}
 
void VkApp::createInstance(bool doApiDump)
{
    uint32_t countGLFWextensions{0};
    const char** reqGLFWextensions = glfwGetRequiredInstanceExtensions(&countGLFWextensions);

    printf("GLFW required extensions:\n");
    for (uint32_t i = 0; i < countGLFWextensions; i++) 
    {
      reqInstanceExtensions.push_back(reqGLFWextensions[i]);
      printf("\t%s\n", reqGLFWextensions[i]);
    }

    if (doApiDump)
        reqInstanceLayers.insert(reqInstanceLayers.begin(), "VK_LAYER_LUNARG_api_dump");
  
    uint32_t count;

    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> availableLayers(count);
    vkEnumerateInstanceLayerProperties(&count, availableLayers.data());

    printf("InstanceLayer count: %d\n", count);
    for (int i = 0; i < availableLayers.size(); i++) 
    {
      printf("\tInstanceLayer: %s\n", availableLayers[i].layerName);
    }

    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, availableExtensions.data());

    printf("InstanceExtensions count: %d\n", count);
    for (int i = 0; i < availableExtensions.size(); i++)
    {
      printf("\tInstanceExtension: %s\n", availableExtensions[i].extensionName);
    }

    VkApplicationInfo applicationInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    applicationInfo.pApplicationName = "rtrt";
    applicationInfo.pEngineName      = "no-engine";
    applicationInfo.apiVersion       = VK_MAKE_VERSION(1, 3, 0);

    VkInstanceCreateInfo instanceCreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.pNext                   = nullptr;
    instanceCreateInfo.pApplicationInfo        = &applicationInfo;
    
    instanceCreateInfo.enabledExtensionCount   = reqInstanceExtensions.size();
    instanceCreateInfo.ppEnabledExtensionNames = reqInstanceExtensions.data();
    
    instanceCreateInfo.enabledLayerCount       = reqInstanceLayers.size();
    instanceCreateInfo.ppEnabledLayerNames     = reqInstanceLayers.data();

    vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance);

    if (vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance) != VK_SUCCESS) 
    {
      throw std::runtime_error("vkCreateInstance failed.");
    }
    else 
    {
      printf("Vulkan instance created successfully!\n");
    }

    printf("Vulkan instance creation complete.\n");
}

void VkApp::createPhysicalDevice()
{
  uint32_t physicalDevicesCount = 0;
  vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, nullptr);
  if (physicalDevicesCount == 0) 
  {
    throw std::runtime_error("Failed to find GPUs with Vulkan support!");
  }

  std::vector<VkPhysicalDevice> physicalDevices(physicalDevicesCount);
  vkEnumeratePhysicalDevices(m_instance, &physicalDevicesCount, physicalDevices.data());

  std::vector<uint32_t> compatibleDevices;  

  printf("%d devices found.\n", physicalDevicesCount);
  int i = 0;

  for (auto physicalDevice : physicalDevices) 
  {
    VkPhysicalDeviceProperties GPUproperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &GPUproperties);

    uint32_t extCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
    std::vector<VkExtensionProperties> extensionProperties(extCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, extensionProperties.data());

    std::unordered_set<std::string> availableExtensions;
    for (const auto& ext : extensionProperties) 
    {
      availableExtensions.insert(ext.extensionName);
    }

    bool extensionsSupported = true;
    for (const auto& requiredExt : reqDeviceExtensions) 
    {
      if (availableExtensions.find(requiredExt) == availableExtensions.end()) 
      {
        extensionsSupported = false;
        break;
      }
    }

    if (GPUproperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && extensionsSupported) 
    {
      compatibleDevices.push_back(i);
      printf("Compatible GPU found: %s\n", GPUproperties.deviceName);
      m_physicalDevice = physicalDevice;  
    }
    else 
    {
      printf("Incompatible GPU: %s (Reason: %s)\n",
        GPUproperties.deviceName,
        (GPUproperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? "Not a discrete GPU" : "Missing required extensions");
    }
    i++;
  }

  if (compatibleDevices.empty()) 
  {
    throw std::runtime_error("Failed to find a compatible GPU!");
  }

  VkPhysicalDeviceProperties selectedGPUProperties;
  vkGetPhysicalDeviceProperties(m_physicalDevice, &selectedGPUProperties);
  printf("Physical device selected: %s\n", selectedGPUProperties.deviceName);
}


void VkApp::chooseQueueIndex()
{
    VkQueueFlags requiredQueueFlags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT
                                      | VK_QUEUE_TRANSFER_BIT;

    uint32_t mpCount;
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &mpCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueProperties(mpCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &mpCount, queueProperties.data());

    printf("Number of queue families: %d\n", mpCount);
    for (uint32_t i = 0; i < mpCount; ++i) 
    {
      printf("Queue Family %d: Flags = %d, Queue Count = %d\n", i, queueProperties[i].queueFlags, queueProperties[i].queueCount);
      if ((queueProperties[i].queueFlags & requiredQueueFlags) == requiredQueueFlags) 
      {
        m_graphicsQueueIndex = i;
        printf("Selected queue family index: %d\n", i);
        return;  
      }
    }
    throw std::runtime_error("Failed to find a queue family that supports graphics, compute, and transfer operations!");
}


void VkApp::createDevice()
{
    // =============
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR};
    rtPipelineFeature.pNext = nullptr;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR};
    accelFeature.pNext = &rtPipelineFeature;

    VkPhysicalDeviceVulkan13Features features13{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    features13.pNext = &accelFeature;

    VkPhysicalDeviceVulkan12Features features12{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    features12.pNext = &features13;

    VkPhysicalDeviceVulkan11Features features11{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
    features11.pNext = &features12;

    VkPhysicalDeviceFeatures2 features2{
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &features11;
    // =============
    
    // Ask Vulkan to fill in all structures on the pNext chain
    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &features2);

    float priority = 1.0;
    VkDeviceQueueCreateInfo queueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queueInfo.queueFamilyIndex = m_graphicsQueueIndex;
    queueInfo.queueCount       = 1;
    queueInfo.pQueuePriorities = &priority;
    
    VkDeviceCreateInfo deviceCreateInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    deviceCreateInfo.pNext            = &features2; // This is the whole pNext chain
  
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos    = &queueInfo;
    
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(reqDeviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = reqDeviceExtensions.data();

    if (vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to create logical Vulkan device!");
    }

    printf("Vulkan device created successfully!\n");
}

void VkApp::getCommandQueue()
{
    vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_queue);
    // Returns void -- nothing to verify
    // Nothing to destroy -- the queue is owned by the device.
}

// Create a command pool, used to allocate command buffers, which in
// turn are use to gather and send commands to the GPU.  The flag
// makes it possible to reuse command buffers.  The queue index
// determines which queue the command buffers can be submitted to.
// Use the command pool to also create a command buffer.
void VkApp::createCommandPool()
{
    VkCommandPoolCreateInfo poolCreateInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolCreateInfo.queueFamilyIndex = m_graphicsQueueIndex;
    if (vkCreateCommandPool(m_device, &poolCreateInfo, nullptr, &m_cmdPool) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to create command pool!");
    }
    printf("Command pool created successfully.\n");
    
    VkCommandBufferAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool        = m_cmdPool;
    allocateInfo.commandBufferCount = 1;
    allocateInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    if (vkAllocateCommandBuffers(m_device, &allocateInfo, &m_commandBuffer) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to allocate command buffer!");
    }
    printf("Command buffer allocated successfully.\n");
}
 
// Calling load_VK_EXTENSIONS from extensions_vk.cpp.  A Python script
// from NVIDIA created extensions_vk.cpp from the current Vulkan spec
// for the purpose of loading the symbols for all registered
// extension.  This be (indistinguishable from) magic.
void VkApp::loadExtensions()
{
    load_VK_EXTENSIONS(m_instance, vkGetInstanceProcAddr, m_device, vkGetDeviceProcAddr);
}

//  VkSurface is Vulkan's name for the screen.  Since GLFW creates and
//  manages the window, it creates the VkSurface at our request.
void VkApp::getSurface()
{
  VkBool32 isSupported;  

  if (glfwCreateWindowSurface(m_instance, app->GLFW_window, nullptr, &m_surface) != VK_SUCCESS) 
  {
    throw std::runtime_error("Failed to create window surface!");
  }
  printf("GLFW window surface created successfully.\n");

  if (vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueIndex, m_surface, &isSupported) != VK_SUCCESS) 
  {
    throw std::runtime_error("Failed to query physical device surface support!");
  }
  printf("Surface support query successful.\n");

  if (isSupported != VK_TRUE) 
  {
    throw std::runtime_error("Selected GPU does not support presenting to the specified surface!");
  }
  printf("Selected GPU supports presenting to the surface.\n");
}


// 
void VkApp::createSwapchain()
{
    VkSwapchainKHR oldSwapchain = m_swapchain;

    vkDeviceWaitIdle(m_device);  // Probably unnecessary

    // Get the surface's capabilities
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities);

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

    printf("Available present modes:\n");
    for (const auto& mode : presentModes) 
    {
      printf("\tPresent Mode: %d\n", mode);
    }

    // Choose VK_PRESENT_MODE_FIFO_KHR as a default (this must be supported)
    VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Support is required.
    for (const auto& mode : presentModes) 
    {
      if (mode == VK_PRESENT_MODE_MAILBOX_KHR) 
      {
        swapchainPresentMode = mode;  
        break;
      }
    }
    printf("Selected present mode: %d\n", swapchainPresentMode);
  
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &formatCount, formats.data());

    printf("Available surface formats:\n");
    for (const auto& format : formats) 
    {
      printf("\tFormat: %d, Color Space: %d\n", format.format, format.colorSpace);
    }

    VkFormat surfaceFormat       = VK_FORMAT_UNDEFINED;               // Temporary value.
    VkColorSpaceKHR surfaceColor = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR; // Temporary value
    for (const auto& format : formats) 
    {
      if (format.format == VK_FORMAT_B8G8R8A8_UNORM) 
      {
        surfaceFormat = format.format;
        surfaceColor = format.colorSpace;
        break;
      }
    }
    printf("Selected surface format: %d, Selected color space: %d\n", surfaceFormat, surfaceColor);

    
    // Get the swap chain extent
    VkExtent2D swapchainExtent = capabilities.currentExtent;
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) 
    {
        swapchainExtent = capabilities.currentExtent; 
    }
    else 
    {
        // Does this case ever happen?
        int width, height;
        glfwGetFramebufferSize(app->GLFW_window, &width, &height);

        swapchainExtent = VkExtent2D{static_cast<uint32_t>(width), static_cast<uint32_t>(height)};

        swapchainExtent.width = std::clamp(swapchainExtent.width,
                                           capabilities.minImageExtent.width,
                                           capabilities.maxImageExtent.width);
        swapchainExtent.height = std::clamp(swapchainExtent.height,
                                            capabilities.minImageExtent.height,
                                            capabilities.maxImageExtent.height); 
    }

    // Test against valid size, typically hit when windows are minimized.
    // The app must prevent triggering this code in such a case
    assert(swapchainExtent.width && swapchainExtent.height);
    // If this assert fires, we have some work to do to better deal
    // with the situation.

    // Choose the number of swap chain images, within the bounds supported.
    uint imageCount = capabilities.minImageCount + 1; // Recommendation: minImageCount+1
    if (capabilities.maxImageCount > 0
        && imageCount > capabilities.maxImageCount) 
    {
            imageCount = capabilities.maxImageCount; 
    }
    
    assert (imageCount == 3);
    // If this triggers, disable the assert, BUT help me understand
    // the situation that caused it.  

    // Create the swap chain
    VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                 | VK_IMAGE_USAGE_STORAGE_BIT
                                 | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    
    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    createInfo.surface                  = m_surface;
    createInfo.minImageCount            = imageCount;
    createInfo.imageFormat              = surfaceFormat;
    createInfo.imageColorSpace          = surfaceColor;
    createInfo.imageExtent              = swapchainExtent;
    createInfo.imageUsage               = imageUsage;
    createInfo.preTransform             = capabilities.currentTransform;
    createInfo.compositeAlpha           = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.imageArrayLayers         = 1;
    createInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount    = 1;
    createInfo.pQueueFamilyIndices      = &m_graphicsQueueIndex;
    createInfo.presentMode              = swapchainPresentMode;
    createInfo.oldSwapchain             = oldSwapchain;
    createInfo.clipped                  = true;

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) 
    {
        throw std::runtime_error("Failed to create swap chain!");
    }
    
    if (vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to get swapchain image count!");
    }
    m_swapchainImages.resize(m_imageCount);
    if (vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data()) != VK_SUCCESS) 
    {
      throw std::runtime_error("Failed to get swapchain images!");
    }
    printf("Successfully retrieved %d swapchain images.\n", m_imageCount);
    
    m_barriers.resize(m_imageCount);
    m_imageViews.resize(m_imageCount);

    // Create an VkImageView for each swap chain image.
    for (uint i=0;  i<m_imageCount;  i++) 
    {
        VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
            createInfo.image = m_swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = surfaceFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            vkCreateImageView(m_device, &createInfo, nullptr, &m_imageViews[i]); 
    }

    // Create three VkImageMemoryBarrier structures (one for each swap
    // chain image) and specify the desired
    // layout (VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) for each.
    for (uint i=0;  i<m_imageCount;  i++) 
    {
        VkImageSubresourceRange range = {0};
        range.aspectMask              = VK_IMAGE_ASPECT_COLOR_BIT;
        range.baseMipLevel            = 0;
        range.levelCount              = VK_REMAINING_MIP_LEVELS;
        range.baseArrayLayer          = 0;
        range.layerCount              = VK_REMAINING_ARRAY_LAYERS;
        
        VkImageMemoryBarrier memBarrier = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        memBarrier.dstAccessMask        = 0;
        memBarrier.srcAccessMask        = 0;
        memBarrier.oldLayout            = VK_IMAGE_LAYOUT_UNDEFINED;
        memBarrier.newLayout            = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        memBarrier.image                = m_swapchainImages[i];
        memBarrier.subresourceRange     = range;
        m_barriers[i] = memBarrier;
    }

    // Create a temporary command buffer. submit the layout conversion
    // command, submit and destroy the command buffer.
    VkCommandBuffer cmd = createTempCmdBuffer();
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, m_imageCount, m_barriers.data());
    submitTempCmdBuffer(cmd);

    // Create the three synchronization objects.  These are not
    // technically part of the swap chain, but they are used
    // exclusively for synchronizing the swap chain, so I include them
    // here.
    VkFenceCreateInfo fenceCreateInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_waitFence);
    NAME(m_waitFence, VK_OBJECT_TYPE_FENCE, "m_waitFence");
    
    VkSemaphoreCreateInfo semCreateInfo = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    vkCreateSemaphore(m_device, &semCreateInfo, nullptr, &m_readSemaphore);
    vkCreateSemaphore(m_device, &semCreateInfo, nullptr, &m_writtenSemaphore);
    NAME(m_readSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "m_readSemaphore");
    NAME(m_writtenSemaphore, VK_OBJECT_TYPE_SEMAPHORE, "m_writtenSemaphore");
        
    m_windowSize = swapchainExtent;
    
}

