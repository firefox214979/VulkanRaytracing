//////////////////////////////////////////////////////////////////////
// Uses the ASSIMP library to read mesh models in of 30+ file types
// into a structure suitable for the raytracer.
////////////////////////////////////////////////////////////////////////

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <math.h>

#include <filesystem>
namespace fs = std::filesystem;

#include "vkapp.h"

#include <assimp/Importer.hpp>
#include <assimp/version.h>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#define GLM_FORCE_RADIANS
#define GLM_SWIZZLE
#include <glm/glm.hpp>
using namespace glm;

#define STBI_FAILURE_USERMSG
#include "stb_image.h"

#include "app.h"
#include "shaders/shared_structs.h"

// Local objects and procedures defined and used here:
struct ModelData
{
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<Material> materials;
    std::vector<int32_t>     matIndx;
    std::vector<std::string> textures;

    bool readAssimpFile(const std::string& path, const glm::mat4& M);
};

void recurseModelNodes(ModelData* meshdata,
                       const  aiScene* aiscene,
                       const  aiNode* node,
                       const aiMatrix4x4& parentTr,
                       const int level=0);


// Returns an address (as VkDeviceAddress=uint64_t) of a buffer on the GPU.
VkDeviceAddress getBufferDeviceAddress(VkDevice device, VkBuffer buffer) {
    VkBufferDeviceAddressInfo info = {VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    info.buffer                    = buffer;
    return vkGetBufferDeviceAddress(device, &info);
}

void VkApp::loadModel()
{
#ifdef SAN_MIGUEL
    // Download from  https://casual-effects.com/data/index.html
    std::string modelFile = "models/San_Miguel/san-miguel.obj";
    app->myCamera.reset(glm::vec3(6.026, 1.348, 7.284), 1.5,  55.99, -7.06,  0.57, 0.1, 1000.0);
    scLightAmb = vec3(0.2);
    scLightInt = vec3(1.0f);
    scLightPos = vec3(21.0f, 20.4f, 2.3);
#else
    // Included with this framework.
    std::string modelFile = "models/living_room/living_room.obj";
    app->myCamera.reset(glm::vec3(2.28, 1.68, 6.64),    0.7, -20.0,   10.66,  0.57, 0.1, 1000.0);
    scLightAmb = vec3(0.2);
    scLightInt = vec3(1.0f);
    scLightPos = vec3(0.5f, 2.5f, 3.0f);
#endif
    
    if (!loadModel(modelFile, glm::mat4(1.0))) {
        printf("\n\nCannot find model file %s\n\n", modelFile.c_str());
        exit(0); }
}

bool VkApp::loadModel(const std::string& filename, glm::mat4 transform)
{
    ModelData meshdata;
    if (!meshdata.readAssimpFile(filename.c_str(), glm::mat4(1.0))) return false;

    printf("vertices: %zd\n", meshdata.vertices.size());
    printf("indices: %zd (%zd)\n", meshdata.indices.size(), meshdata.indices.size()/3);
    printf("materials: %zd\n", meshdata.materials.size());
    printf("matIndx: %zd\n", meshdata.matIndx.size());
    printf("textures: %zd\n", meshdata.textures.size());
             
#ifdef SAN_MIGUEL
    // The San_Miguel model has no useful lights. This code adds one
    // light to mimic a sky above the courtyard. That is, a rectangle
    // consisting of 4 vertices, and two triangles with a new bright
    // emissive Material.
    
    vec3 T0(0,0,1);
    vec3 T1( 0.866, 0, -0.5);
    vec3 T2(-0.866, 0, -0.5);
    vec3 Z(0,0,0);
    vec3 LC(21.50, 20.39, 2.29);
    int Nv = meshdata.vertices.size();
    int Nm = meshdata.materials.size();
    
    float s = 50;
    vec3 Sky(5,5,5);
    meshdata.vertices.push_back({vec3( 6.5,15, 0), vec3(0,1,0), vec2(0,0)});
    meshdata.vertices.push_back({vec3( 6.5,15,13), vec3(0,1,0), vec2(0,0)});
    meshdata.vertices.push_back({vec3(23.0,15, 0), vec3(0,1,0), vec2(0,0)});
    meshdata.vertices.push_back({vec3(23.0,15,13), vec3(0,1,0), vec2(0,0)});
    meshdata.indices.push_back(Nv+0);
    meshdata.indices.push_back(Nv+1);
    meshdata.indices.push_back(Nv+2);
    meshdata.indices.push_back(Nv+2);
    meshdata.indices.push_back(Nv+1);
    meshdata.indices.push_back(Nv+3);
    meshdata.materials.push_back({Z, Z, Sky, 0.0, -1});
    meshdata.matIndx.push_back(Nm);                       
    meshdata.matIndx.push_back(Nm);                             
#endif
    
    printf("vertices: %zd\n", meshdata.vertices.size());
    printf("indices: %zd (%zd)\n", meshdata.indices.size(), meshdata.indices.size()/3);
    printf("materials: %zd\n", meshdata.materials.size());
    printf("matIndx: %zd\n", meshdata.matIndx.size());
    printf("textures: %zd\n", meshdata.textures.size());
    

    std::vector<Emitter> lightList; 
    // @@ The raytracer will eventually need a list of lights.  By
    // "light" I mean a triangle in the triangle list such that the
    // triangle's associated material type has a non-zero emission.
    // Create such a list into the above lightList vector,
    //
    // Hints:
    // 1: Loop through the indices of the model's triangles with
    //      for (uint i=0;  i<meshdata.matIndx.size();  i++)
    // 2: Get the material type of triangle i with
    //      Material& mat = meshdata.materials[meshdata.matIndx[i]];
    //      where Material type is defined in shaders/shared_structs.h
    // 3: Check if the material specifies a non-zero emission, perhaps with
    //      if (glm::dot(mat.emission,mat.emission) > 0.0f)
    // 4: The triangle at index i has
    //    vertices:
    //      v0 = meshdata.vertices[meshdata.indices[3*i+0]].pos;
    //      v1 = meshdata.vertices[meshdata.indices[3*i+1]].pos;
    //      v2 = meshdata.vertices[meshdata.indices[3*i+2]].pos;
    //    emission:  Use 4*mat.emission to get a scene of reasonable brightness
    // 5: Create an instance of Emitter with those values and push_back onto lightList
    //      Class Emitter is defined in shaders/shared_structs.h
    
#ifdef SAN_MIGUEL
    // The San_Miguel model has many useless lights. The myloadModel
    // procedure adds one useful light.  This keeps just that one
    // useful light and tosses out the rest.
    lightList.erase(lightList.begin(), lightList.end()-1);
#endif
    std::vector<Emitter> emitterList;
    for (uint i = 0; i < meshdata.matIndx.size(); i++) {
      Material& mat = meshdata.materials[meshdata.matIndx[i]];

      if (glm::dot(mat.emission, mat.emission) > 0.0f) {
        Emitter emitter;

        emitter.v0 = meshdata.vertices[meshdata.indices[3 * i + 0]].pos;
        emitter.v1 = meshdata.vertices[meshdata.indices[3 * i + 1]].pos;
        emitter.v2 = meshdata.vertices[meshdata.indices[3 * i + 2]].pos;

        emitter.emission = mat.emission;

        emitter.index = i;

        emitter.normal = normalize(glm::cross(emitter.v1 - emitter.v0, emitter.v2 - emitter.v0));

        emitter.area = glm::length(glm::cross(emitter.v1 - emitter.v0, emitter.v2 - emitter.v0)) * 0.5f;

        emitterList.push_back(emitter);
      }
    }
    // @@ Project 4b is optional, but if you do it, here is where you
    // send the above lightList to the shader.  The project 4b
    // document provides details.
    
    initBufferWrap(m_lightBuff, sizeof(emitterList[0]) * emitterList.size(),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    );

    VkCommandBuffer commandBuffer = createTempCmdBuffer();
    vkCmdUpdateBuffer(
      commandBuffer,
      m_lightBuff.buffer,
      0,
      sizeof(emitterList[0]) * emitterList.size(),
      emitterList.data()
    );
    submitTempCmdBuffer(commandBuffer);

    ObjData object;
    object.nbIndices  = static_cast<uint32_t>(meshdata.indices.size());
    object.nbVertices = static_cast<uint32_t>(meshdata.vertices.size());

    // Create the buffers on Device and copy vertices, indices and materials
    VkCommandBuffer    cmdBuf = createTempCmdBuffer();

    VkBufferUsageFlags flag = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    VkBufferUsageFlags rtFlags = flag
        | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  
    initBufferWrapFromData(object.vertexBuffer, cmdBuf, meshdata.vertices,
                         VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rtFlags);
    initBufferWrapFromData(object.indexBuffer, cmdBuf, meshdata.indices,
                         VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rtFlags);
    initBufferWrapFromData(object.matColorBuffer, cmdBuf, meshdata.materials, flag);
    initBufferWrapFromData(object.matIndexBuffer, cmdBuf, meshdata.matIndx, flag);
    
    NAME(object.vertexBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "object.vertexBuffer");
    NAME(object.vertexBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "object.indexBuffer");
    NAME(object.vertexBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "object.matColorBuffer");
    NAME(object.vertexBuffer.buffer, VK_OBJECT_TYPE_BUFFER, "object.matIndexBuffer");
    
  
    submitTempCmdBuffer(cmdBuf);
    
    // Creates all textures on the GPU
    auto txtOffset = static_cast<uint32_t>(m_objText.size());  // Offset is current size
    for(const auto& texName : meshdata.textures)
        m_objText.push_back(readTextureFile(texName));

    // Assuming one instance of an object with its supplied transform.
    // Could provide multiple transform here to make a vector of instances of this object.
    ObjInst instance;
    instance.transform = transform;
    instance.objIndex  = static_cast<uint32_t>(m_objData.size()); // Index of current object
    m_objInst.push_back(instance);

    // Creating information for device access
    ObjDesc desc;
    desc.txtOffset            = txtOffset;
    desc.vertexAddress        = getBufferDeviceAddress(m_device, object.vertexBuffer.buffer);
    desc.indexAddress         = getBufferDeviceAddress(m_device, object.indexBuffer.buffer);
    desc.materialAddress      = getBufferDeviceAddress(m_device, object.matColorBuffer.buffer);
    desc.materialIndexAddress = getBufferDeviceAddress(m_device, object.matIndexBuffer.buffer);

    m_objData.emplace_back(object);
    m_objDesc.emplace_back(desc);

    // @@ At shutdown:
    //   Destroy all textures with:  for (t:m_objText) t.destroy(m_device); 
    //   Destroy the 4 buffers containing each object's data with:
    //      for (auto& ob : m_objData) {
    //        ob.vertexBuffer.destroy(m_device); 
    //        and similar for ob.indexBuffer, ob.matColorBuffer, ob.matIndexBuffer ... }

    return true;
}

bool ModelData::readAssimpFile(const std::string& path, const mat4& M)
{
    printf("ReadAssimpFile File:  %s \n", path.c_str());
  
    aiMatrix4x4 modelTr(M[0][0], M[1][0], M[2][0], M[3][0],
                        M[0][1], M[1][1], M[2][1], M[3][1],
                        M[0][2], M[1][2], M[2][2], M[3][2],
                        M[0][3], M[1][3], M[2][3], M[3][3]);

    // Does the file exist?
    std::ifstream find_it(path.c_str());
    if (find_it.fail()) {
        std::cerr << "File not found: "  << path << std::endl;
        return false; }

    // Invoke assimp to read the file.
    printf("Assimp %d.%d Reading %s\n", aiGetVersionMajor(), aiGetVersionMinor(), path.c_str());
    Assimp::Importer importer;
    const aiScene* aiscene = importer.ReadFile(path.c_str(),
                                               aiProcess_Triangulate|aiProcess_GenSmoothNormals);
    
    if (!aiscene) {
        printf("... Failed to read.\n");
        exit(-1); }

    if (!aiscene->mRootNode) {
        printf("Scene has no rootnode.\n");
        exit(-1); }

    printf("Assimp mNumMeshes: %d\n", aiscene->mNumMeshes);
    printf("Assimp mNumMaterials: %d\n", aiscene->mNumMaterials);
    printf("Assimp mNumTextures: %d\n", aiscene->mNumTextures);

    for (int i=0;  i<aiscene->mNumMaterials;  i++) {
        aiMaterial* mtl = aiscene->mMaterials[i];
        aiString name;
        mtl->Get(AI_MATKEY_NAME, name);
        aiColor3D emit(0.f,0.f,0.f); 
        aiColor3D diff(0.f,0.f,0.f), spec(0.f,0.f,0.f); 
        float alpha = 20.0;
        bool he = mtl->Get(AI_MATKEY_COLOR_EMISSIVE, emit);
        bool hd = mtl->Get(AI_MATKEY_COLOR_DIFFUSE, diff);
        bool hs = mtl->Get(AI_MATKEY_COLOR_SPECULAR, spec);
        bool ha = mtl->Get(AI_MATKEY_SHININESS, &alpha, NULL);
        aiColor3D trans;
        bool ht = mtl->Get(AI_MATKEY_COLOR_TRANSPARENT, trans);

        Material newmat;
        if (!emit.IsBlack()) { // An emitter
            newmat.diffuse = {1,1,1};  // An emitter needs (1,1,1), else black screen!  WTF???
            newmat.specular = {0,0,0};
            newmat.shininess = 0.0;
            newmat.emission = {emit.r, emit.g, emit.b};
            newmat.textureId = -1; }
        
        else {
            vec3 Kd(0.5f, 0.5f, 0.5f); 
            vec3 Ks(0.03f, 0.03f, 0.03f);
            if (AI_SUCCESS == hd) Kd = vec3(diff.r, diff.g, diff.b);
            if (AI_SUCCESS == hs) Ks = vec3(spec.r, spec.g, spec.b);
            newmat.diffuse = {Kd[0], Kd[1], Kd[2]};
            newmat.specular = {Ks[0], Ks[1], Ks[2]};
            newmat.shininess = alpha; //sqrtf(2.0f/(2.0f+alpha));
            newmat.emission = {0,0,0};
            newmat.textureId = -1;  }
        
        aiString texPath;
        if (AI_SUCCESS == mtl->GetTexture(aiTextureType_DIFFUSE, 0, &texPath)) {
            fs::path fullPath = path;
            fullPath.replace_filename(texPath.C_Str());
            std::cout << "Texture: " << fullPath << std::endl;
            newmat.textureId = textures.size();
            auto xxx = fullPath.u8string();
            textures.push_back(std::string(xxx));
        }
        
        materials.push_back(newmat);
    }
    
    recurseModelNodes(this, aiscene, aiscene->mRootNode, modelTr);

    return true;

}

// Recursively traverses the assimp node hierarchy, accumulating
// modeling transformations, and creating and transforming any meshes
// found.  Meshes comming from assimp can have associated surface
// properties, so each mesh *copies* the current BRDF as a starting
// point and modifies it from the assimp data structure.
void recurseModelNodes(ModelData* meshdata,
                       const aiScene* aiscene,
                       const aiNode* node,
                       const aiMatrix4x4& parentTr,
                       const int level)
{
    // Print line with indentation to show structure of the model node hierarchy.
    //for (int i=0;  i<level;  i++) printf("| ");
    //printf("%s \n", node->mName.data);

    // Accumulating transformations while traversing down the hierarchy.
    aiMatrix4x4 childTr = parentTr*node->mTransformation;
    aiMatrix3x3 normalTr = aiMatrix3x3(childTr); // Really should be inverse-transpose for full generality
     
    // Loop through this node's meshes
    for (unsigned int m=0;  m<node->mNumMeshes; ++m) {
        aiMesh* aimesh = aiscene->mMeshes[node->mMeshes[m]];
        //printf("  %d: %d:%d\n", m, aimesh->mNumVertices, aimesh->mNumFaces);

        // Loop through all vertices and record the
        // vertex/normal/texture/tangent data with the node's model
        // transformation applied.
        uint faceOffset = meshdata->vertices.size();
        for (unsigned int t=0;  t<aimesh->mNumVertices;  ++t) {
            aiVector3D aipnt = childTr*aimesh->mVertices[t];
            aiVector3D ainrm = aimesh->HasNormals() ? normalTr*aimesh->mNormals[t] : aiVector3D(0,0,1);
            aiVector3D aitex = aimesh->HasTextureCoords(0) ? aimesh->mTextureCoords[0][t] : aiVector3D(0,0,0);
            aiVector3D aitan = aimesh->HasTangentsAndBitangents() ? normalTr*aimesh->mTangents[t] :  aiVector3D(1,0,0);


            meshdata->vertices.push_back({{aipnt.x, aipnt.y, aipnt.z},
                                          {ainrm.x, ainrm.y, ainrm.z},
                                          {aitex.x, aitex.y}});
        }
        
        // Loop through all faces, recording indices
        for (unsigned int t=0;  t<aimesh->mNumFaces;  ++t) {
            aiFace* aiface = &aimesh->mFaces[t];
            for (int i=2;  i<aiface->mNumIndices;  i++) {
                meshdata->matIndx.push_back(aimesh->mMaterialIndex);
                meshdata->indices.push_back(aiface->mIndices[0]+faceOffset);
                meshdata->indices.push_back(aiface->mIndices[i-1]+faceOffset);
                meshdata->indices.push_back(aiface->mIndices[i]+faceOffset); } }; }


    // Recurse onto this node's children
    for (unsigned int i=0;  i<node->mNumChildren;  ++i)
        recurseModelNodes(meshdata, aiscene, node->mChildren[i], childTr, level+1);
}

ImageWrap VkApp::readTextureFile(std::string fileName)
{
    for (int i=0;  i<fileName.size();  i++)
        if (fileName[i] == '\\') fileName[i] = '/';
    
    //VkImage& textureImage, VkDeviceMemory& textureImageMemory
    int texWidth, texHeight, texChannels;

    stbi_set_flip_vertically_on_load(true);
    stbi_uc* pixels = stbi_load(fileName.c_str(), &texWidth, &texHeight, &texChannels,
                                STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    BufferWrap staging;
    initBufferWrap(staging, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT
                   | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    void* data;
    vkMapMemory(m_device, staging.memory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(m_device, staging.memory);

    stbi_image_free(pixels);

    uint mipLevels = std::floor(std::log2(std::max(texWidth, texHeight))) + 1;
    
    ImageWrap myImage;
    VkExtent2D texSize{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)};
    initImageWrap(myImage, texSize, VK_FORMAT_R8G8B8A8_UNORM,
                  VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_SAMPLED_BIT
                  | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  VK_IMAGE_ASPECT_COLOR_BIT,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  mipLevels);

    initTextureSampler(myImage);
    
    // Copy staging buffer contents to the image (via a vkCmdCopyBufferToImage)
    VkCommandBuffer commandBuffer = createTempCmdBuffer();

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), 1};

    vkCmdCopyBufferToImage(commandBuffer, staging.buffer, myImage.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    
    submitTempCmdBuffer(commandBuffer);

    // Done with staging buffer
    staging.destroy(m_device);

    generateMipmap(myImage.image, VK_FORMAT_R8G8B8A8_UNORM, texWidth, texHeight, mipLevels);
    
    return myImage;
}

void VkApp::generateMipmap(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels)
{
        // Check if image format supports linear blitting
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(m_physicalDevice, imageFormat, &formatProperties);

        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting!");
        }

        VkCommandBuffer commandBuffer = createTempCmdBuffer();
    
        VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = texWidth;
        int32_t mipHeight = texHeight;

        for (uint32_t i = 1; i < mipLevels; i++) {
            barrier.subresourceRange.baseMipLevel = i - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            VkImageBlit blit{};
            blit.srcOffsets[0] = {0, 0, 0};
            blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = {0, 0, 0};
            blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.dstSubresource.mipLevel = i;
            blit.dstSubresource.baseArrayLayer = 0;
            blit.dstSubresource.layerCount = 1;

            vkCmdBlitImage(commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                1, &blit,
                VK_FILTER_LINEAR);

            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (mipWidth > 1) mipWidth /= 2;
            if (mipHeight > 1) mipHeight /= 2;
        }

        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);

    submitTempCmdBuffer(commandBuffer);
    }
