

#include <array>
#include <iostream>     // std::cout
#include <fstream>      // std::ifstream

#include <unordered_set>
#include <unordered_map>

#include "vkapp.h"
#include "app.h"
#include "extensions_vk.hpp"

void VkApp::createDepthResource() 
{
    // Note m_depthImage is type ImageWrap; a tiny wrapper around
    // several related Vulkan objects.
    initImageWrap(m_depthImage, m_windowSize,
                  VK_FORMAT_X8_D24_UNORM_PACK32,
                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  VK_IMAGE_ASPECT_DEPTH_BIT,
                  VK_IMAGE_LAYOUT_UNDEFINED,
                  1);

}


void VkApp::createPostRenderPass()
{  
    std::array<VkAttachmentDescription, 2> attachments{};
    // Color attachment
    attachments[0].format      = VK_FORMAT_B8G8R8A8_UNORM;
    attachments[0].loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    attachments[0].samples     = VK_SAMPLE_COUNT_1_BIT;

    // Depth attachment
    attachments[1].format        = VK_FORMAT_X8_D24_UNORM_PACK32;
    attachments[1].loadOp        = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].finalLayout   = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    attachments[1].samples       = VK_SAMPLE_COUNT_1_BIT;

    const VkAttachmentReference colorReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    const VkAttachmentReference depthReference{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};


    std::array<VkSubpassDependency, 1> subpassDependencies{};
    // Transition from final to initial (VK_SUBPASS_EXTERNAL refers to all commands executed outside of the actual renderpass)
    subpassDependencies[0].srcSubpass      = VK_SUBPASS_EXTERNAL;
    subpassDependencies[0].dstSubpass      = 0;
    subpassDependencies[0].srcStageMask    = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    subpassDependencies[0].dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpassDependencies[0].srcAccessMask   = VK_ACCESS_MEMORY_READ_BIT;
    subpassDependencies[0].dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
        | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    subpassDependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    VkSubpassDescription subpassDescription{};
    subpassDescription.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.colorAttachmentCount    = 1;
    subpassDescription.pColorAttachments       = &colorReference;
    subpassDescription.pDepthStencilAttachment = &depthReference;

    VkRenderPassCreateInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments    = attachments.data();
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpassDescription;
    renderPassInfo.dependencyCount = static_cast<uint32_t>(subpassDependencies.size());
    renderPassInfo.pDependencies   = subpassDependencies.data();

    vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_postRenderPass);
}

// A VkFrameBuffer wraps several images into a render target --
// usually a color buffer and a depth buffer.
void VkApp::createPostFrameBuffers()
{
    std::array<VkImageView, 2> fbattachments{};
    
    // Create frame buffers for every swap chain image
    VkFramebufferCreateInfo createInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    createInfo.renderPass      = m_postRenderPass;
    createInfo.width           = m_windowSize.width;
    createInfo.height          = m_windowSize.height;
    createInfo.layers          = 1;
    createInfo.attachmentCount = 2;
    createInfo.pAttachments    = fbattachments.data();

    // Each of the three swapchain images gets an associated frame
    // buffer, all sharing one depth buffer.
    m_framebuffers.resize(m_imageCount);
    for(uint32_t i = 0; i < m_imageCount; i++) 
    {
        fbattachments[0] = m_imageViews[i];         // A color attachment from the swap chain
        fbattachments[1] = m_depthImage.imageView;  // A depth attachment
        if (vkCreateFramebuffer(m_device, &createInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) 
        {
          throw std::runtime_error("Failed to create framebuffer for swapchain image " + std::to_string(i));
        }
    }

    printf("Successfully created %d framebuffers.\n", m_imageCount);
}


void VkApp::createPostPipeline()
{

    // Creating the pipeline layout
    VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};


    createInfo.setLayoutCount         = 1;
    createInfo.pSetLayouts            = &m_postDesc.descSetLayout;
    //createInfo.setLayoutCount         = 0;
    //createInfo.pSetLayouts            = nullptr;
    
    createInfo.pushConstantRangeCount = 0;
    createInfo.pPushConstantRanges    = nullptr;
    
    vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_postPipelineLayout);

    ////////////////////////////////////////////
    // Create the shaders
    ////////////////////////////////////////////
    VkShaderModule vertShaderModule = createShaderModule(loadFile("spv/post.vert.spv"));
    VkShaderModule fragShaderModule = createShaderModule(loadFile("spv/post.frag.spv"));

    VkPipelineShaderStageCreateInfo
        vertShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo
        fragShaderStageInfo{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo
        vertexInputInfo{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    // No geometry in this pipeline's draw.
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
        
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo
        inputAssembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float) m_windowSize.width;
    viewport.height = (float) m_windowSize.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_windowSize;

    VkPipelineViewportStateCreateInfo
        viewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo
        rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE; //??
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo
        multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineDepthStencilStateCreateInfo
        depthStencil{VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;// BEWARE!!  NECESSARY!!
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo
        colorBlending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.logicOp = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkGraphicsPipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = shaderStages;
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = m_postPipelineLayout;
    pipelineInfo.renderPass = m_postRenderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                              &m_postPipeline);
 
    // The pipeline has fully compiled copies of the shaders, so these
    // intermediate (SPV) versions can be destroyed.
       vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
       vkDestroyShaderModule(m_device, vertShaderModule, nullptr);

}

// Read a file's bytes into a string
std::string VkApp::loadFile(const std::string& filename)
{
    std::string   result;
    std::ifstream stream(filename, std::ios::ate | std::ios::binary);  //ate: Open at file end

    if(!stream.is_open())
        throw std::runtime_error("Can not open a shader file.\n");

    result.reserve(stream.tellg()); // tellg() is last char position in file (i.e.,  length)

    // Seek back to the beginning of the file and read it's contents
    stream.seekg(0, std::ios::beg);
    result.assign((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    return result;
}

//-------------------------------------------------------------------------------------------------
// Post processing pass: tone mapper, UI
void VkApp::postProcess()
{
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color        = {{1,1,1,1}};
    clearValues[1].depthStencil = {1.0f, 0};
            
    VkRenderPassBeginInfo beginInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    beginInfo.clearValueCount = 2;
    beginInfo.pClearValues    = clearValues.data();
    beginInfo.renderPass      = m_postRenderPass;
    beginInfo.framebuffer     = m_framebuffers[m_swapchainIndex];
    beginInfo.renderArea      = {{0, 0}, m_windowSize};
    
    vkCmdBeginRenderPass(m_commandBuffer, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    {   // extra indent for renderpass commands
        
        auto aspectRatio = static_cast<float>(m_windowSize.width)
            / static_cast<float>(m_windowSize.height);
        vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_postPipeline);
        vkCmdBindDescriptorSets(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                m_postPipelineLayout, 0, 1, &m_postDesc.descSet, 0, nullptr);

        // Weird! This draws 3 vertices but with no vertices/triangles buffers bound in.
        // Hint: The vertex shader fabricates vertices from gl_VertexIndex
        vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);

        #ifdef GUI
        // Important: This is LAST -- so ImGui can overwrite all screen contents.
        ImGui::Render();  // Rendering UI
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), m_commandBuffer);
        #endif
    }
    vkCmdEndRenderPass(m_commandBuffer);
}
