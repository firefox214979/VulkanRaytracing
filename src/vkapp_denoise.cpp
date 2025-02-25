#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <math.h>

#include "vkapp.h"
#define SHADOW
#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#include "app.h"
#include "shaders/shared_structs.h"

#define GROUP_SIZE 128


void VkApp::createDenoiseBuffer()
{
  VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
  VkImageUsageFlags flags = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  VkMemoryPropertyFlags mem = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
  VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_COLOR_BIT;
  VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL;

  initImageWrap(m_denoiseBuffer, m_windowSize, format, flags, mem, aspect, layout);
}

void VkApp::createDenoiseDescriptorSet()
{
  m_denoiseDesc.setBindings(m_device, {
          {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
          {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
          {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},
          {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}
    });

  m_denoiseDesc.write(m_device, 0, m_renderTarget.Descriptor());   // The input image
  m_denoiseDesc.write(m_device, 1, m_denoiseBuffer.Descriptor());   // The output image
  m_denoiseDesc.write(m_device, 2, m_rtKdCurrBuffer.Descriptor());  // The color buffer
  m_denoiseDesc.write(m_device, 3, m_rtNdCurrBuffer.Descriptor());  // The normal:depth buffer
}

void VkApp::createDenoiseCompPipeline()
{
  // pushing time
  VkPushConstantRange pc_info = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise) };
  VkPipelineLayoutCreateInfo plCreateInfo{ VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
  plCreateInfo.setLayoutCount = 1;
  plCreateInfo.pSetLayouts = &m_denoiseDesc.descSetLayout;
  plCreateInfo.pushConstantRangeCount = 1;
  plCreateInfo.pPushConstantRanges = &pc_info;
  vkCreatePipelineLayout(m_device, &plCreateInfo, nullptr, &m_denoiseCompPipelineLayout);

  VkComputePipelineCreateInfo cpCreateInfo{ VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO };
  cpCreateInfo.layout = m_denoiseCompPipelineLayout;

  cpCreateInfo.stage = createShaderStageInfo(loadFile("spv/denoise.comp.spv"),
    VK_SHADER_STAGE_COMPUTE_BIT);
  vkCreateComputePipelines(m_device, {}, 1, &cpCreateInfo, nullptr, &m_denoisePipeline);
  vkDestroyShaderModule(m_device, cpCreateInfo.stage.module, nullptr);

}

void VkApp::denoise()
{
  m_pcDenoise.normFactor = 0.003;
  m_pcDenoise.depthFactor = 0.007;

  // Wait for RT to finish
  VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
  VkImageMemoryBarrier    imgMemBarrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
  imgMemBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
  imgMemBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  imgMemBarrier.image = m_renderTarget.image;
  imgMemBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgMemBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
  imgMemBarrier.subresourceRange = range;

  vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    VK_DEPENDENCY_DEVICE_GROUP_BIT, 0, nullptr, 0, nullptr, 1, &imgMemBarrier);

  int stepwidth = 1;
  for (int a = 0; a < m_num_atrous_iterations; a++) {

    // Tell the A-Trous algorithm its "hole" size
    m_pcDenoise.stepwidth = stepwidth;
    stepwidth *= 2;

    // Select the compute shader, and its descriptor set and push constant
    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_denoisePipeline);
    vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
      m_denoiseCompPipelineLayout, 0, 1,
      &m_denoiseDesc.descSet, 0, nullptr);
    vkCmdPushConstants(m_commandBuffer, m_denoiseCompPipelineLayout,
      VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PushConstantDenoise),
      &m_pcDenoise);

    // Dispatch the shader in batches of 128x1 (WHY???)
    // This MUST match the shaders's line:
    //    layout(local_size_x=GROUP_SIZE, local_size_y=1, local_size_z=1) in;
    vkCmdDispatch(m_commandBuffer,
      (m_windowSize.width + GROUP_SIZE - 1) / GROUP_SIZE,
      m_windowSize.height, 1);

    // Wait until denoise shader is done writing to m_denoiseBuffer
    imgMemBarrier.image = m_denoiseBuffer.image;
    vkCmdPipelineBarrier(m_commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      VK_DEPENDENCY_DEVICE_GROUP_BIT,
      0, nullptr, 0, nullptr, 1, &imgMemBarrier);

    CmdCopyImage(m_denoiseBuffer, m_renderTarget);
  }
}
