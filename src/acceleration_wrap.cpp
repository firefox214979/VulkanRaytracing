
#include "acceleration_wrap.h"
#include "vkapp.h"
#include <numeric>

//--------------------------------------------------------------------------------------------------
// Initializing the allocator and querying the raytracing properties
//

void RaytracingBuilderKHR::setup(VkApp* _VK, const VkDevice& device, uint32_t queueIndex)
{
    VK = _VK;
    m_device     = device;
    m_queueIndex = queueIndex;
}

//--------------------------------------------------------------------------------------------------
// Destroying all allocations
//
void RaytracingBuilderKHR::destroy()
{
    for(auto& blas : m_blas)  {
        blas.accelBuf.destroy(VK->m_device);
        vkDestroyAccelerationStructureKHR(VK->m_device, blas.accelStr, nullptr); }
    
    m_tlas.accelBuf.destroy(VK->m_device);
    vkDestroyAccelerationStructureKHR(VK->m_device, m_tlas.accelStr, nullptr);

    m_blas.clear();
}

//--------------------------------------------------------------------------------------------------
// Returning the constructed top-level acceleration structure
//
VkAccelerationStructureKHR RaytracingBuilderKHR::getAccelerationStructure() const
{
    return m_tlas.accelStr;
}

//--------------------------------------------------------------------------------------------------
// Return the device address of a BLAS previously created.
//
VkDeviceAddress RaytracingBuilderKHR::getBlasDeviceAddress(uint32_t blasId)
{
    assert(size_t(blasId) < m_blas.size());
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR};
    addressInfo.accelerationStructure = m_blas[blasId].accelStr;
    printf("    vkGetAccelerationStructureDeviceAddressKHR\n");
    return vkGetAccelerationStructureDeviceAddressKHR(m_device, &addressInfo);
}

//--------------------------------------------------------------------------------------------------
// Create all the BLAS from the vector of BlasInput
// - There will be one BLAS per input-vector entry
// - There will be as many BLAS as input.size()
// - The resulting BLAS (along with the inputs used to build) are stored in m_blas,
//   and can be referenced by index.
// - if flag has the 'Compact' flag, the BLAS will be compacted
//
void RaytracingBuilderKHR::buildBlas(const std::vector<BlasInput>& input,
                                     VkBuildAccelerationStructureFlagsKHR flags)
{
    auto         nbBlas = static_cast<uint32_t>(input.size());
    VkDeviceSize asTotalSize{0};     // Memory size of all allocated BLAS
    uint32_t     nbCompactions{0};   // Nb of BLAS requesting compaction
    VkDeviceSize maxScratchSize{0};  // Largest scratch size

    // Preparing the information for the acceleration build commands.
    std::vector<BuildAccelerationStructure> buildAs(nbBlas);
    for(uint32_t idx = 0; idx < nbBlas; idx++)
        {
            printf("    For BlasInput %d (of %d).\n", idx, nbBlas);
            // Filling partially the VkAccelerationStructureBuildGeometryInfoKHR for querying the build sizes.
            // Other information will be filled in the createBlas (see #2)
            buildAs[idx].buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildAs[idx].buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
            buildAs[idx].buildInfo.flags         = input[idx].flags | flags;
            buildAs[idx].buildInfo.geometryCount = static_cast<uint32_t>(input[idx].asGeometry.size());
            buildAs[idx].buildInfo.pGeometries   = input[idx].asGeometry.data();

            // Build range information
            buildAs[idx].rangeInfo = input[idx].asBuildOffsetInfo.data();

            // Finding sizes to create acceleration structures and scratch
            std::vector<uint32_t> maxPrimCount(input[idx].asBuildOffsetInfo.size());
            for(auto tt = 0; tt < input[idx].asBuildOffsetInfo.size(); tt++)
                maxPrimCount[tt] = input[idx].asBuildOffsetInfo[tt].primitiveCount; //# of triangles
            printf("      vkGetAccelerationStructureBuildSizesKHR to request needed BLAS size\n");
            vkGetAccelerationStructureBuildSizesKHR(m_device,
                                                    VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                    &buildAs[idx].buildInfo, maxPrimCount.data(),
                                                    &buildAs[idx].sizeInfo);

            // Extra info
            asTotalSize += buildAs[idx].sizeInfo.accelerationStructureSize;
            maxScratchSize = std::max(maxScratchSize, buildAs[idx].sizeInfo.buildScratchSize);
            nbCompactions += hasFlag(buildAs[idx].buildInfo.flags,
                                     VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR);
        }


    // Allocate the scratch buffers holding the temporary data of the acceleration structure builder
    printf("    Create scratch buffer of max size\n");
    VK->initBufferWrap(VK->m_scratch1, maxScratchSize,
                   VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                   | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  
    VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr, VK->m_scratch1.buffer};
    printf("    vkGetBufferDeviceAddress for address of scratch buffer\n");
    VkDeviceAddress           scratchAddress = vkGetBufferDeviceAddress(m_device, &bufferInfo);

    // Allocate a query pool for storing the needed size for every BLAS compaction.
    VkQueryPool queryPool{VK_NULL_HANDLE};
    if(nbCompactions > 0)  // Is compaction requested?
        {
            assert(nbCompactions == nbBlas);  // Don't allow mix of on/off compaction
            VkQueryPoolCreateInfo qpci{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
            qpci.queryCount = nbBlas;
            qpci.queryType  = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR;
            vkCreateQueryPool(m_device, &qpci, nullptr, &queryPool);
        }

    // Batching creation/compaction of BLAS to allow staying in restricted amount of memory
    std::vector<uint32_t> indices;  // Indices of the BLAS to create
    VkDeviceSize          batchSize{0};
    VkDeviceSize          batchLimit{256'000'000};  // 256 MB
    for(uint32_t idx = 0; idx < nbBlas; idx++)
        {
            indices.push_back(idx);
            batchSize += buildAs[idx].sizeInfo.accelerationStructureSize;
            // Over the limit or last BLAS element
            if(batchSize >= batchLimit || idx == nbBlas - 1)
                {
                    VkCommandBuffer cmdBuf = VK->createTempCmdBuffer();
                    cmdCreateBlas(cmdBuf, indices, buildAs, scratchAddress, queryPool);
                    VK->submitTempCmdBuffer(cmdBuf);

                    if (queryPool)
                        {
                            VkCommandBuffer cmdBuf = VK->createTempCmdBuffer();
                            cmdCompactBlas(cmdBuf, indices, buildAs, queryPool);
                            VK->submitTempCmdBuffer(cmdBuf);

                            // Destroy the non-compacted version
                            destroyNonCompacted(indices, buildAs);
                        }
                    // Reset

                    batchSize = 0;
                    indices.clear();
                }
        }

    // Logging reduction
    if(queryPool)
        {
            VkDeviceSize compactSize = std::accumulate(buildAs.begin(), buildAs.end(), 0ULL, [](const auto& a, const auto& b) {
                return a + b.sizeInfo.accelerationStructureSize;
            });
        }

    // Keeping all the created acceleration structures
    for(auto& b : buildAs)
        {
            m_blas.emplace_back(b.as);
        }

    // Clean up
    if (queryPool) {
        printf("  vkDestroyQueryPool\n");
        vkDestroyQueryPool(m_device, queryPool, nullptr); }
}

AccelWrap createAcceleration(VkApp* VK,
                                              VkAccelerationStructureCreateInfoKHR& accelInfo)
{
    AccelWrap accelWrap;
    // Allocating the buffer to hold the acceleration structure
    printf("        create buffer for aceleration struct\n");

    VK->initBufferWrap(accelWrap.accelBuf, accelInfo.size,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
                      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Create the acceleration structure
    accelInfo.buffer = accelWrap.accelBuf.buffer;
    printf("        vkCreateAccelerationStructureKHR (shape and size; no content yet)\n");
    vkCreateAccelerationStructureKHR(VK->m_device, &accelInfo, nullptr, &accelWrap.accelStr);

    return accelWrap;
}

// Creating the bottom level acceleration structure for all indices of `buildAs` vector.
// The array of BuildAccelerationStructure was created in buildBlas and the vector of
// indices limits the number of BLAS to create at once. This limits the amount of
// memory needed when compacting the BLAS.
void RaytracingBuilderKHR::cmdCreateBlas(VkCommandBuffer                          cmdBuf,
                                         std::vector<uint32_t>                    indices,
                                         std::vector<BuildAccelerationStructure>& buildAs,
                                         VkDeviceAddress                          scratchAddress,
                                         VkQueryPool                              queryPool)
{
    printf("    Call cmdCreateBlas\n");
    if(queryPool)  // For querying the compaction size
        vkResetQueryPool(m_device, queryPool, 0, static_cast<uint32_t>(indices.size()));
    uint32_t queryCnt{0};

    for(const auto& idx : indices)
        {
            printf("      For BLAS #%d of %zd\n", idx, indices.size());
            // Actual allocation of buffer and acceleration structure.
            VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            // Will be used to allocate memory.
            createInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
            buildAs[idx].as = createAcceleration(VK, createInfo);
    
            // BuildInfo #2 part
            // Setting where the build lands
            buildAs[idx].buildInfo.dstAccelerationStructure  = buildAs[idx].as.accelStr;
            // All build use the same scratch buffer
            buildAs[idx].buildInfo.scratchData.deviceAddress = scratchAddress;

            // Building the bottom-level-acceleration-structure
            printf("        vkCmdBuildAccelerationStructuresKHR build BLAS\n");
            vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildAs[idx].buildInfo,
                                                &buildAs[idx].rangeInfo);

            // Since the scratch buffer is reused across builds, we
            // need a barrier to ensure one build is finished before
            // starting the next one.
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
            barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
            printf("        vkCmdPipelineBarrier\n");
            vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                                 0, 1, &barrier, 0, nullptr, 0, nullptr);

            if(queryPool)
                {
                    // Add a query to find the 'real' amount of memory needed, use for compaction
                    printf("      vkCmdWriteAccelerationStructuresPropertiesKHR\n");
                    vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuf, 1,
                               &buildAs[idx].buildInfo.dstAccelerationStructure,
                               VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
                               queryPool, queryCnt++);
                }
        }
}

//--------------------------------------------------------------------------------------------------
// Create and replace a new acceleration structure and buffer based on the size retrieved by the
// Query.
void RaytracingBuilderKHR::cmdCompactBlas(VkCommandBuffer                          cmdBuf,
                                          std::vector<uint32_t>                    indices,
                                          std::vector<BuildAccelerationStructure>& buildAs,
                                          VkQueryPool                              queryPool)
{
    printf("  cmdCompactBlas\n");
    uint32_t queryCtn{0};

    // Get the compacted size result back
    std::vector<VkDeviceSize> compactSizes(static_cast<uint32_t>(indices.size()));
    vkGetQueryPoolResults(m_device, queryPool, 0, (uint32_t)compactSizes.size(), compactSizes.size() * sizeof(VkDeviceSize),
                          compactSizes.data(), sizeof(VkDeviceSize), VK_QUERY_RESULT_WAIT_BIT);

    for(auto idx : indices)
        {
            buildAs[idx].cleanupAS = buildAs[idx].as.accelStr; // previous AS to destroy
            buildAs[idx].sizeInfo.accelerationStructureSize = compactSizes[queryCtn++];  // new reduced size

            // Creating a compact version of the AS
            VkAccelerationStructureCreateInfoKHR asCreateInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            asCreateInfo.size = buildAs[idx].sizeInfo.accelerationStructureSize;
            asCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
            buildAs[idx].as = createAcceleration(VK, asCreateInfo);

            // Copy the original BLAS to a compact version
            VkCopyAccelerationStructureInfoKHR copyInfo{VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR};
            copyInfo.src  = buildAs[idx].buildInfo.dstAccelerationStructure;
            copyInfo.dst  = buildAs[idx].as.accelStr;
            copyInfo.mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR;
            printf("  vkCmdCopyAccelerationStructureKHR for BLAS commodification\n");
            vkCmdCopyAccelerationStructureKHR(cmdBuf, &copyInfo);
        }
}

//--------------------------------------------------------------------------------------------------
// Destroy all the non-compacted acceleration structures
//
void RaytracingBuilderKHR::destroyNonCompacted(std::vector<uint32_t> indices, std::vector<BuildAccelerationStructure>& buildAs)
{
    printf("  RaytracingBuilderKHR::destroyNonCompacted\n");
    for(auto& i : indices)
        {
            vkDestroyAccelerationStructureKHR(VK->m_device, buildAs[i].cleanupAS, nullptr);
        }
}

//--------------------------------------------------------------------------------------------------
// Low level of Tlas creation 
//
void RaytracingBuilderKHR::cmdCreateTlas(VkCommandBuffer                      cmdBuf,
                                         uint32_t                             countInstance,
                                         VkDeviceAddress                      instBufferAddr,
                                         VkBuildAccelerationStructureFlagsKHR flags,
                                         bool                                 update,
                                         bool                                 motion)
{
    // Wraps a device pointer to the above uploaded instances.
    VkAccelerationStructureGeometryInstancesDataKHR instancesVk{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR};
    instancesVk.data.deviceAddress = instBufferAddr;

    // Put the above into a VkAccelerationStructureGeometryKHR. We need to put the instances struct in a union and label it as instance data.
    VkAccelerationStructureGeometryKHR topASGeometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    topASGeometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    topASGeometry.geometry.instances = instancesVk;

    // Find sizes
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfo.flags         = flags;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &topASGeometry;
    buildInfo.mode = update ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.srcAccelerationStructure = VK_NULL_HANDLE;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    printf("      vkGetAccelerationStructureBuildSizesKHR to request needed TLAS sizes\n");
    vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo,
                                            &countInstance, &sizeInfo);

    // Create TLAS
    if(update == false)
        {

            printf("      Create acceleration structure of size: %ld\n", sizeInfo.accelerationStructureSize);
            VkAccelerationStructureCreateInfoKHR createInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR};
            createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
            createInfo.size = sizeInfo.accelerationStructureSize;
            m_tlas = createAcceleration(VK, createInfo);
        }

    // Allocate the scratch memory
    printf("      Create a scratch buffer of size: %ld\n", sizeInfo.buildScratchSize);
    VK->initBufferWrap(VK->m_scratch2, sizeInfo.buildScratchSize,
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                       | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr, VK->m_scratch2.buffer};
    printf("      vkGetBufferDeviceAddress of the scratch buffer\n");
    VkDeviceAddress scratchAddress = vkGetBufferDeviceAddress(m_device, &bufferInfo);

    // Update build information
    buildInfo.srcAccelerationStructure  = update ? m_tlas.accelStr : VK_NULL_HANDLE;
    buildInfo.dstAccelerationStructure  = m_tlas.accelStr;
    buildInfo.scratchData.deviceAddress = scratchAddress;

    // Build Offsets info: n instances
    VkAccelerationStructureBuildRangeInfoKHR        buildOffsetInfo{countInstance, 0, 0, 0};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildOffsetInfo = &buildOffsetInfo;

    // Build the TLAS
    printf("      vkCmdBuildAccelerationStructuresKHR to build the TLAS\n");
    vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfo, &pBuildOffsetInfo);
    //scratch.destroy(VK->m_device);
}

//--------------------------------------------------------------------------------------------------
// Refit BLAS number blasIdx from updated buffer contents.
//
void RaytracingBuilderKHR::updateBlas(uint32_t blasIdx, BlasInput& blas, VkBuildAccelerationStructureFlagsKHR flags)
{
    assert (false && "Not used; Not maintained;  Probably leaks a VkDeviceMemory");
    printf("  updateBlas\n");
    assert(size_t(blasIdx) < m_blas.size());

    // Preparing all build information, acceleration is filled later
    VkAccelerationStructureBuildGeometryInfoKHR buildInfos{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR};
    buildInfos.flags                    = flags;
    buildInfos.geometryCount            = (uint32_t)blas.asGeometry.size();
    buildInfos.pGeometries              = blas.asGeometry.data();
    buildInfos.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;  // UPDATE
    buildInfos.type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfos.srcAccelerationStructure = m_blas[blasIdx].accelStr;  // UPDATE
    buildInfos.dstAccelerationStructure = m_blas[blasIdx].accelStr;

    // Find size to build on the device
    std::vector<uint32_t> maxPrimCount(blas.asBuildOffsetInfo.size());
    for(auto tt = 0; tt < blas.asBuildOffsetInfo.size(); tt++)
        maxPrimCount[tt] = blas.asBuildOffsetInfo[tt].primitiveCount;  // Number of primitives/triangles
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfos,
                                            maxPrimCount.data(), &sizeInfo);

    // Allocate the scratch buffer and setting the scratch info
    BufferWrap scratch;
    VK->initBufferWrap(scratch, sizeInfo.buildScratchSize,
                       VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                       | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    bufferInfo.buffer = scratch.buffer;
    buildInfos.scratchData.deviceAddress = vkGetBufferDeviceAddress(m_device, &bufferInfo);

    std::vector<const VkAccelerationStructureBuildRangeInfoKHR*> pBuildOffset(blas.asBuildOffsetInfo.size());
    for(size_t i = 0; i < blas.asBuildOffsetInfo.size(); i++)
        pBuildOffset[i] = &blas.asBuildOffsetInfo[i];

    VkCommandBuffer cmdBuf = VK->createTempCmdBuffer();

    // Update the instance buffer on the device side and build the TLAS
    // Update the acceleration structure. Note the VK_TRUE parameter to trigger the update,
    // and the existing BLAS being passed and updated in place
    printf("  vkCmdBuildAccelerationStructuresKHR for BLAS update\n");
    vkCmdBuildAccelerationStructuresKHR(cmdBuf, 1, &buildInfos, pBuildOffset.data());

    VK->submitTempCmdBuffer(cmdBuf);
}
    
void RaytracingBuilderKHR::buildTlas(
                                 const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                                 VkBuildAccelerationStructureFlagsKHR flags,
                                 bool update, bool motion)
{
    // Cannot call buildTlas twice except to update.
    //assert(m_tlas.accelStr == VK_NULL_HANDLE || update);
    uint32_t countInstance = static_cast<uint32_t>(instances.size());

    // Command buffer to create the TLAS
    VkCommandBuffer    cmdBuf = VK->createTempCmdBuffer();

    // Create a buffer holding the actual instance data (matrices++) for use by the AS builder
    printf("    Create a buffer (staged) for the instances\n");
    BufferWrap instancesBuffer;
    VK->initBufferWrapFromData(instancesBuffer, cmdBuf, instances,
                           VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
                           | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR);
    VkBufferDeviceAddressInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr,
        instancesBuffer.buffer};
    printf("    vkGetBufferDeviceAddress of that instance buffer\n");
    VkDeviceAddress           instBufferAddr = vkGetBufferDeviceAddress(m_device, &bufferInfo);
    
    // Make sure the copy of the instance buffer are copied before triggering the acceleration structure build
    VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    printf("    vkCmdPipelineBarrier ensuring instance buffer is completely filled in\n");
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    // Creating the TLAS
    printf("    Call cmdCreateTlas\n");
    cmdCreateTlas(cmdBuf, countInstance, instBufferAddr, flags, update, motion);

    // Finalizing and destroying temporary data
    VK->submitTempCmdBuffer(cmdBuf);
    
    instancesBuffer.destroy(VK->m_device);
 }


//-------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------
// Convert an OBJ model into the ray tracing geometry used to build the BLAS
//
BlasInput VkApp::objectToVkGeometryKHR(const ObjData& model)
{
    // BLAS builder requires raw device addresses.
    VkBufferDeviceAddressInfo _b1{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr, model.vertexBuffer.buffer};
    printf("      Get vkGetBufferDeviceAddress of object's vertex buffer\n");
    VkDeviceAddress vertexAddress = vkGetBufferDeviceAddress(m_device, &_b1);

    
    VkBufferDeviceAddressInfo _b2{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        nullptr, model.indexBuffer.buffer};
    printf("      Get vkGetBufferDeviceAddress of object's index buffer\n");
    VkDeviceAddress indexAddress  = vkGetBufferDeviceAddress(m_device, &_b2);

    uint32_t maxPrimitiveCount = model.nbIndices / 3;

    // Describe buffer as array of Vertex.
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
    triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // vec3 vertex position data.
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride             = sizeof(Vertex);
    // Describe index data (32-bit unsigned int)
    triangles.indexType               = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = indexAddress;
    triangles.maxVertex = model.nbVertices;

    // Identify the above data as containing opaque triangles.
    VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
    asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
    asGeom.geometry.triangles = triangles;

    // The entire array will be used to build the BLAS.
    VkAccelerationStructureBuildRangeInfoKHR offset;
    offset.firstVertex     = 0;
    offset.primitiveCount  = maxPrimitiveCount;
    offset.primitiveOffset = 0;
    offset.transformOffset = 0;

    // Our blas is made from only one geometry, but could be made of many geometries
    BlasInput input;
    input.asGeometry.emplace_back(asGeom);
    input.asBuildOffsetInfo.emplace_back(offset);

    printf("      Return a BlasInput containing that geometry (+).\n");
    return input;
}

void VkApp::createRtAccelerationStructure()
{
    printf("\nVkApp::createRtAccelerationStructure\n");
    // BLAS - Storing each primitive in a geometry
    std::vector<BlasInput> allBlas;
    allBlas.reserve(m_objData.size());
    printf("\n  Build vector<BlasInput> for list of objects (of length %ld).\n", m_objData.size());
    for (const auto& obj : m_objData)  {
        printf("    Call VkApp::objectToVkGeometryKHR to return a BlasInput entry.\n");
        BlasInput blas = objectToVkGeometryKHR(obj);
        allBlas.emplace_back(blas); }

    printf("\n  Call buildBlas to build vector<AccelWrap> m_blas\n");
    printf("                    from vector<BlasInput>\n");
    m_rtBuilder.buildBlas(allBlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    // TLAS
    printf("\n  Create vector<VkAccelerationStructureInstanceKHR> tlas to hold all BLASes\n");
    std::vector<VkAccelerationStructureInstanceKHR> tlas;
    tlas.reserve(m_objInst.size());
    for(const ObjInst& inst : m_objInst) {
        printf("  For each object\n");
        VkAccelerationStructureInstanceKHR _i{};
        _i.transform = toTransformMatrixKHR(inst.transform);  // Position of the instance
        _i.instanceCustomIndex = inst.objIndex; 
        _i.accelerationStructureReference = m_rtBuilder.getBlasDeviceAddress(inst.objIndex);
        _i.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        _i.mask  = 0xFF;       //  Only be hit if rayMask & instance.mask != 0
        _i.instanceShaderBindingTableRecordOffset = 0; // Use the same hit group for all objects
        printf("    append object's BLAS-address and transformation to the tlas vector\n");
        tlas.emplace_back(_i);
    }
    
    printf("\n  Call buildTlas with a list of BLAS instances\n");
    m_rtBuilder.buildTlas(tlas, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                          false, false);
    m_scratch1.destroy(m_device);
    m_scratch2.destroy(m_device);
    printf("\nEnd of VkApp::createRtAccelerationStructure\n\n");

    
}


