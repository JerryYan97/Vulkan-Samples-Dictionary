//
// Created by Jerry on 11/28/2021.
//
#include <iostream>
#include <fstream>
#include "vulkan/vulkan.hpp"

// NOTE: You can use glslc in Vulkan SDK to generate .spv SPIRV shader files from .comp/.vert/.frag .etc glsl or hlsl
//       files. This tool should be attached to your command line tool as soon as you install the Vulkan SDK.

// TODO: May need Boost to find the main.cpp path.
uint32_t findMemoryType( vk::PhysicalDeviceMemoryProperties const & memoryProperties,
                         uint32_t                                   typeBits,
                         vk::MemoryPropertyFlags                    requirementsMask );

int main()
{
    // Initialize matrix data
    float mat1[9] =
    {
            1.f, 2.f, 3.f,
            4.f, 5.f, 6.f,
            7.f, 8.f, 9.f
    };

    float mat2[9] =
    {
            1.f, 1.f, 1.f,
            2.f, 3.f, 4.f,
            1.f, 3.f, 5.f
    };

    float matRes[9];

    /*
       Vulkan Device Creation
    */

    // Initialize vulkan
    // Initialize the vk::ApplicationInfo structure
    vk::ApplicationInfo appInfo(
                                "Matrix Multiply",
                                1,
                                "Vulkan.hpp",
                                1,
                                VK_API_VERSION_1_1);

    // Initialize the vk::InstanceCreateInfo
    vk::InstanceCreateInfo instanceCreateInfo({}, &appInfo);

    // Create an Instance
    vk::Instance instance = vk::createInstance(instanceCreateInfo);

    // Enumerate the physicalDevice and print the first physical device's name
    vk::PhysicalDevice physicalDevice = instance.enumeratePhysicalDevices().front();
    vk::PhysicalDeviceProperties physicalDeviceProperties;
    physicalDevice.getProperties(&physicalDeviceProperties);
    std::cout << "Device name:" << physicalDeviceProperties.deviceName << std::endl;

    // Get the QueueFamilyProperties of the first PhysicalDevice
    std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

    // Find the first index into queueFamilyProperties which supports graphics
    size_t computeQueueFamilyIndex = ~0;
    for (int i = 0; i < queueFamilyProperties.size(); ++i)
    {
        if (queueFamilyProperties[i].queueFlags & vk::QueueFlagBits::eCompute)
        {
            computeQueueFamilyIndex = i;
            break;
        }
    }

    // Create a Device
    float queuePriority = 0.f;
    vk::DeviceQueueCreateInfo deviceQueueCreateInfo(
            vk::DeviceQueueCreateFlags(),
            static_cast<uint32_t>(computeQueueFamilyIndex),
            1,
            &queuePriority
    );
    vk::Device device = physicalDevice.createDevice(
            vk::DeviceCreateInfo(
                    vk::DeviceCreateFlags(),
                    deviceQueueCreateInfo
            )
    );

    /*
        Prepare storage buffers
    */
    // Initialize buffer data and memory backend for matrices
    vk::Buffer uniformDataBuffer1 = device.createBuffer(
            vk::BufferCreateInfo(
                    vk::BufferCreateFlags(),
                    9 * sizeof(float),
                    vk::BufferUsageFlagBits::eUniformBuffer)
    );

    vk::Buffer uniformDataBuffer2 = device.createBuffer(
        vk::BufferCreateInfo(
            vk::BufferCreateFlags(),
            9 * sizeof(float),
            vk::BufferUsageFlagBits::eUniformBuffer)
    );

    vk::Buffer storageDataBuffer = device.createBuffer(
        vk::BufferCreateInfo(
            vk::BufferCreateFlags(),
            9 * sizeof(float),
            vk::BufferUsageFlagBits::eStorageBuffer)
    );

    vk::MemoryRequirements memory1Requirements = device.getBufferMemoryRequirements(uniformDataBuffer1);
    vk::MemoryRequirements memory2Requirements = device.getBufferMemoryRequirements(uniformDataBuffer2);
    vk::MemoryRequirements memoryOutRequirements = device.getBufferMemoryRequirements(storageDataBuffer);

    vk::MemoryPropertyFlags requiredMemFlags = vk::MemoryPropertyFlagBits::eDeviceLocal |
                                               vk::MemoryPropertyFlagBits::eHostVisible |
                                               vk::MemoryPropertyFlagBits::eHostCoherent;
    vk::PhysicalDeviceMemoryProperties deviceMemProperties = physicalDevice.getMemoryProperties();
    uint32_t mem1TypeIndex = findMemoryType(deviceMemProperties, memory1Requirements.memoryTypeBits, requiredMemFlags);
    uint32_t mem2TypeIndex = findMemoryType(deviceMemProperties, memory2Requirements.memoryTypeBits, requiredMemFlags);
    uint32_t memOutTypeIndex = findMemoryType(deviceMemProperties, memoryOutRequirements.memoryTypeBits, requiredMemFlags);

    vk::DeviceMemory uniformDataMemory1 = device.allocateMemory(
            vk::MemoryAllocateInfo(memory1Requirements.size, mem1TypeIndex)
    );

    vk::DeviceMemory uniformDataMemory2 = device.allocateMemory(
            vk::MemoryAllocateInfo(memory2Requirements.size, mem2TypeIndex)
    );

    vk::DeviceMemory storageDataMemoryOut = device.allocateMemory(
        vk::MemoryAllocateInfo(memoryOutRequirements.size, memOutTypeIndex)
    );

    uint8_t* pDevMem1Data = static_cast<uint8_t*>(device.mapMemory(uniformDataMemory1, 0, memory1Requirements.size));
    uint8_t* pDevMem2Data = static_cast<uint8_t*>(device.mapMemory(uniformDataMemory2, 0, memory2Requirements.size));

    memcpy(pDevMem1Data, mat1, 9 * sizeof(float));
    memcpy(pDevMem2Data, mat2, 9 * sizeof(float));

    device.unmapMemory(uniformDataMemory1);
    device.unmapMemory(uniformDataMemory2);

    device.bindBufferMemory(uniformDataBuffer1, uniformDataMemory1, 0);
    device.bindBufferMemory(uniformDataBuffer2, uniformDataMemory2, 0);
    device.bindBufferMemory(storageDataBuffer, storageDataMemoryOut, 0);

    /*
        Prepare Compute Pipeline
    */
    // Create Compute pipeline
    vk::DescriptorSetLayoutBinding myDescriptorSetLayoutBindings[] =
    {
            {0, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
            {1, vk::DescriptorType::eUniformBuffer, 1, vk::ShaderStageFlagBits::eCompute},
            {2, vk::DescriptorType::eStorageBuffer, 1, vk::ShaderStageFlagBits::eCompute}
    };

    // Create a DescriptorSetLayout using that bindings
    vk::DescriptorSetLayout descriptorSetLayout = device.createDescriptorSetLayout(
            vk::DescriptorSetLayoutCreateInfo(vk::DescriptorSetLayoutCreateFlags(), 3, myDescriptorSetLayoutBindings)
    );

    // Create a pipelineLayout using that DescriptorSetLayout
    vk::PipelineLayout pipelineLayout = device.createPipelineLayout(
        vk::PipelineLayoutCreateInfo(vk::PipelineLayoutCreateFlags(), descriptorSetLayout)
    );

    /* Initialize Descriptor Set */
    // Create a descriptor pool
    vk::DescriptorPoolSize poolSizes[] =
    {
            vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer, 2),
            vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer, 1)
    };

    vk::DescriptorPool descriptorPool = device.createDescriptorPool(
            vk::DescriptorPoolCreateInfo(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, 1, 3, poolSizes)
    );

    // Allocate a descriptor set
    vk::DescriptorSet descriptorSet = device.allocateDescriptorSets(
        vk::DescriptorSetAllocateInfo(descriptorPool, 1, &descriptorSetLayout)
    ).front();

    // Initialize a descriptor set
    vk::DescriptorBufferInfo descriptorBufferInfos[] =
    {
            vk::DescriptorBufferInfo(uniformDataBuffer1, 0, 9 * sizeof(float)),
            vk::DescriptorBufferInfo(uniformDataBuffer2, 0, 9 * sizeof(float)),
            vk::DescriptorBufferInfo(storageDataBuffer, 0, 9 * sizeof(float))
    };
    vk::WriteDescriptorSet descriptorSetWrites[] =
    {
            vk::WriteDescriptorSet(descriptorSet, 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfos),
            vk::WriteDescriptorSet(descriptorSet, 1, 1, 1, vk::DescriptorType::eUniformBuffer, nullptr, descriptorBufferInfos),
            vk::WriteDescriptorSet(descriptorSet, 2, 2, 1, vk::DescriptorType::eStorageBuffer, nullptr, descriptorBufferInfos)
    };
    device.updateDescriptorSets(3, descriptorSetWrites, 0, nullptr);

    // Create Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                         the CMakeLists.txt file in the same level of repository.
    std::cout << SOURCE_PATH << std::endl;
    std::string shaderPath = std::string(SOURCE_PATH) + std::string("/MatrixMul.comp.spv");
    std::ifstream inputShader(shaderPath.c_str(), std::ios::binary | std::ios::in | std::ios::ate);
    std::vector<unsigned char> buffer(std::istreambuf_iterator<char>(inputShader), {});
    inputShader.close();
    vk::ShaderModuleCreateInfo shaderModuleCreateInfo(
            vk::ShaderModuleCreateFlags(), buffer.size(), (uint32_t*)buffer.data()
    );
    vk::ShaderModule shaderModule;
    device.createShaderModule(&shaderModuleCreateInfo, nullptr, &shaderModule);

    // Create Compute Shader Pipeline
    vk::PipelineShaderStageCreateInfo pipelineShaderStageCreateInfo(
            vk::PipelineShaderStageCreateFlags(), vk::ShaderStageFlagBits::eCompute, shaderModule, "main"
    );
    vk::ComputePipelineCreateInfo computePipelineCreateInfo(
            vk::PipelineCreateFlags(),
            pipelineShaderStageCreateInfo,
            pipelineLayout
    );
    vk::Pipeline computePipeline;
    device.createComputePipelines(VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline);

    /*
        Command buffer creation (for compute work submission)
    */
    // Create a CommandPool to allocate a CommandBuffer from
    vk::CommandPool commandPool = device.createCommandPool(
            vk::CommandPoolCreateInfo(vk::CommandPoolCreateFlags(), computeQueueFamilyIndex)
    );

    // Allocate a CommandBuffer from the CommandPool
    vk::CommandBuffer commandBuffer = device.allocateCommandBuffers(
            vk::CommandBufferAllocateInfo(commandPool,
                                          vk::CommandBufferLevel::ePrimary,
                                          1)
    ).front();

    // Get a compute queue from the device
    vk::Queue computeQueue = device.getQueue(computeQueueFamilyIndex, 0);

    commandBuffer.begin( vk::CommandBufferBeginInfo( vk::CommandBufferUsageFlags() ) );

    // Barrier to ensure that input buffer transfer is finished before compute shader reads from it
    vk::BufferMemoryBarrier inputBufferBarriers[] =
    {
        vk::BufferMemoryBarrier(vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eHostRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, uniformDataBuffer1, 0, VK_WHOLE_SIZE),
        vk::BufferMemoryBarrier(vk::AccessFlagBits::eHostWrite, vk::AccessFlagBits::eHostRead, VK_QUEUE_FAMILY_IGNORED, VK_QUEUE_FAMILY_IGNORED, uniformDataBuffer2, 0, VK_WHOLE_SIZE)
    };

    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost,
                                  vk::PipelineStageFlagBits::eComputeShader,
                                  vk::DependencyFlags(),
                                  0,
                                  nullptr,
                                  0,
                                  inputBufferBarriers,
                                  0,
                                  nullptr
                                  );

    commandBuffer.bindPipeline(vk::PipelineBindPoint::eCompute, computePipeline);
    commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    commandBuffer.dispatch(1, 1, 1);

    // Barrier to ensure that shader writes are finished before buffer is read back from GPU
    vk::BufferMemoryBarrier bufferOutBarrier(
            vk::AccessFlagBits::eHostRead,
            vk::AccessFlagBits::eShaderWrite,
            VK_QUEUE_FAMILY_IGNORED,
            VK_QUEUE_FAMILY_IGNORED,
            storageDataBuffer,
            0,
            VK_WHOLE_SIZE
    );
    commandBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                  vk::PipelineStageFlagBits::eTransfer,
                                  vk::DependencyFlags(),
                                  0, nullptr,
                                  1, &bufferOutBarrier,
                                  0, nullptr);
    commandBuffer.end();

    vk::FenceCreateInfo fenceCreateInfo((vk::FenceCreateFlags()));
    vk::Fence fence;
    device.createFence(&fenceCreateInfo, nullptr, &fence);
    vk::SubmitInfo submitInfo;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    // Submit compute work
    computeQueue.submit(1, &submitInfo, fence);
    device.waitForFences(1, &fence, VK_TRUE, UINT64_MAX);

    // Make device writes visible to the host
    void* mapped;
    device.mapMemory(storageDataMemoryOut, 0, VK_WHOLE_SIZE, vk::MemoryMapFlags(), &mapped);
    vk::MappedMemoryRange mappedRange(storageDataMemoryOut, 0, VK_WHOLE_SIZE);
    device.invalidateMappedMemoryRanges(1, &mappedRange);

    // Copy to output
    memcpy(matRes, mapped, 9 * sizeof(float));
    device.unmapMemory(storageDataMemoryOut);

    std::cout << "Hello World." << std::endl;

    //
    device.destroyShaderModule(shaderModule);
    device.destroyPipelineLayout(pipelineLayout);
    device.destroyDescriptorSetLayout(descriptorSetLayout);
    device.freeDescriptorSets(descriptorPool, descriptorSet);
    device.destroyDescriptorPool(descriptorPool);

    // Free memory and destroy buffer
    device.freeMemory(uniformDataMemory1);
    device.freeMemory(uniformDataMemory2);
    device.freeMemory(storageDataMemoryOut);
    device.destroyBuffer(uniformDataBuffer1);
    device.destroyBuffer(uniformDataBuffer2);
    device.destroyBuffer(storageDataBuffer);

    // Freeing the commandBuffer
    device.freeCommandBuffers(commandPool, commandBuffer);

    // Destroy the commandPool
    device.destroyCommandPool(commandPool);

    // Destroy the device
    device.destroy();

    // Destroy the Instance
    instance.destroy();
}

void readShaderFile(const std::string& iPath)
{

    return;
}

uint32_t findMemoryType( vk::PhysicalDeviceMemoryProperties const & memoryProperties,
                         uint32_t                                   typeBits,
                         vk::MemoryPropertyFlags                    requirementsMask )
{
    uint32_t typeIndex = ~0;
    for ( uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++ )
    {
        if ( ( typeBits & 1 ) &&
             ( ( memoryProperties.memoryTypes[i].propertyFlags & requirementsMask ) == requirementsMask ) )
        {
            typeIndex = i;
            break;
        }
        typeBits >>= 1;
    }
    assert(typeIndex != uint32_t( ~0 ));
    return typeIndex;
}