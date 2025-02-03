#pragma once

#include <vulkan/vulkan_core.h>
#include <iostream>

class VkApp;

struct BufferWrap
{
    VkBuffer buffer;
    VkDeviceMemory memory;

    BufferWrap() : buffer(VK_NULL_HANDLE),  memory(VK_NULL_HANDLE)
    {};
    
    void destroy(VkDevice& device)
    {
        vkDestroyBuffer(device, buffer, nullptr);
        vkFreeMemory(device, memory, nullptr);
    }
};

struct ImageWrap
{
    VkImage          image{};
    VkDeviceMemory   memory{};
    VkImageView      imageView{};
    VkSampler        sampler{};
    
    ImageWrap() : image(VK_NULL_HANDLE),  memory(VK_NULL_HANDLE),
                  imageView(VK_NULL_HANDLE), sampler(VK_NULL_HANDLE)
    {};
    
    void destroy(VkDevice device)
    {
        vkDestroyImage(device, image, nullptr);
        vkFreeMemory(device, memory, nullptr);
        vkDestroyImageView(device, imageView, nullptr);
        vkDestroySampler(device, sampler, nullptr);
    }
    
    VkDescriptorImageInfo Descriptor(VkImageLayout layout=VK_IMAGE_LAYOUT_GENERAL) const 
    {
        return VkDescriptorImageInfo({sampler, imageView, layout});
    }
};

