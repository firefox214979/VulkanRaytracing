#include "buffer_wrap.h"
#include "vkapp.h"

void VkApp::initBufferWrap(BufferWrap& wrap, VkDeviceSize size, VkBufferUsageFlags usage,
                           VkMemoryPropertyFlags properties)
{
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(m_device, &bufferInfo, nullptr, &wrap.buffer);

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(m_device, wrap.buffer, &memRequirements);

    VkMemoryAllocateFlagsInfo memFlags = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO, nullptr,
        VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT, 0};

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.pNext = &memFlags;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_physicalDevice, memRequirements.memoryTypeBits, properties);

    vkAllocateMemory(m_device, &allocInfo, nullptr, &wrap.memory);
    vkBindBufferMemory(m_device, wrap.buffer, wrap.memory, 0);

    // @@ Verify success of vkAllocateMemory and vkBindBufferMemory.
}


// This creates a VkImage, an associated VkDeviceMemory object, and an
// VkImageView.  The VkSampler is left empty to be created elsewhere
// if needed.
void VkApp::initImageWrap(ImageWrap& wrap,
                          VkExtent2D& size,
                          VkFormat format,
                          VkImageUsageFlags usage,
                          VkMemoryPropertyFlags properties,
                          VkImageAspectFlagBits aspect,
                          VkImageLayout layout,
                          uint mipLevels)
{
    // Create the VkImage
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = size.width;
    imageInfo.extent.height = size.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = mipLevels;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateImage(m_device, &imageInfo, nullptr, &wrap.image);

    // Create and bind the associated VkDeviceMemory
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(m_device, wrap.image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(m_physicalDevice, 
                                               memRequirements.memoryTypeBits, properties);

    vkAllocateMemory(m_device, &allocInfo, nullptr, &wrap.memory);
    
    vkBindImageMemory(m_device, wrap.image, wrap.memory, 0);

    // Create the associated VkImageView
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = wrap.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspect;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    
    vkCreateImageView(m_device, &viewInfo, nullptr, &wrap.imageView);

    // Do not create a sampler; a different call will do that
    wrap.sampler = VK_NULL_HANDLE;

    // Transition the image to the desired layout
    if (layout != VK_IMAGE_LAYOUT_UNDEFINED) {

        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = wrap.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        
        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        VkCommandBuffer commandBuffer = createTempCmdBuffer();

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0,
                             0, nullptr,    0, nullptr,    1, &barrier);
        
        submitTempCmdBuffer(commandBuffer);
    }

    // @@ Verify success for vkCreateImage,  vkAllocateMemory, vkCreateImageView
}

void VkApp::initTextureSampler(ImageWrap& wrap)
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    vkCreateSampler(m_device, &samplerInfo, nullptr, &wrap.sampler);
    
    // @@ Verify success for vkCreateSampler
}


void VkApp::initBufferWrapFromData(BufferWrap& wrap,
                                   const VkCommandBuffer& cmdBuf,
                                   const VkDeviceSize&    size,
                                   const void*            data,
                                   VkBufferUsageFlags     usage)
{
    BufferWrap staging;
    initBufferWrap(staging, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* dest;
    vkMapMemory(m_device, staging.memory, 0, size, 0, &dest);
    memcpy(dest, data, size);
    vkUnmapMemory(m_device, staging.memory);

    
    initBufferWrap(wrap, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    copyBuffer(staging.buffer, wrap.buffer, size);

    staging.destroy(m_device);
}

void VkApp::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
{
    VkCommandBuffer commandBuffer =  createTempCmdBuffer();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    submitTempCmdBuffer(commandBuffer);
}

// Gets a list of memory types supported by the GPU, and search
// through that list for one that matches the requested properties
// flag.  The (only?) two types requested here are:
//
// (1) VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT: For the bulk of the memory
// used by the GPU to store things internally.
//
// (2) VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT:
// for memory visible to the CPU  for CPU to GPU copy operations.
uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i))
            && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i; } }

    throw std::runtime_error("failed to find suitable memory type!");
}
