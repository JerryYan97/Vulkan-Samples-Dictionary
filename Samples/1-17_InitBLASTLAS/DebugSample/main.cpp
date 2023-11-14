#include <iostream>
#include <vector>
#include <unordered_set>
#include <fstream>
#include <cassert>
#include "vulkan/vulkan.h"

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

// Features: VulkanRT, Vulkan HLSL, Vulkan Dynamic Rendering.
// Reference: 2023 SIGGRAPH Course - Real-Time Ray-Tracing with Vulkan for the Impatient.
//            https://github.com/SaschaWillems/Vulkan/blob/master/examples/raytracingbasic/raytracingbasic.cpp

#define STR(r)    \
	case r:       \
		return #r \

const std::string to_string(VkResult result)
{
    switch (result) {
        STR(VK_NOT_READY);
        STR(VK_TIMEOUT);
        STR(VK_EVENT_SET);
        STR(VK_EVENT_RESET);
        STR(VK_INCOMPLETE);
        STR(VK_ERROR_OUT_OF_HOST_MEMORY);
        STR(VK_ERROR_OUT_OF_DEVICE_MEMORY);
        STR(VK_ERROR_INITIALIZATION_FAILED);
        STR(VK_ERROR_DEVICE_LOST);
        STR(VK_ERROR_MEMORY_MAP_FAILED);
        STR(VK_ERROR_LAYER_NOT_PRESENT);
        STR(VK_ERROR_EXTENSION_NOT_PRESENT);
        STR(VK_ERROR_FEATURE_NOT_PRESENT);
        STR(VK_ERROR_INCOMPATIBLE_DRIVER);
        STR(VK_ERROR_TOO_MANY_OBJECTS);
        STR(VK_ERROR_FORMAT_NOT_SUPPORTED);
        STR(VK_ERROR_FRAGMENTED_POOL);
        STR(VK_ERROR_UNKNOWN);
        STR(VK_ERROR_OUT_OF_POOL_MEMORY);
        STR(VK_ERROR_INVALID_EXTERNAL_HANDLE);
        STR(VK_ERROR_FRAGMENTATION);
        STR(VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS);
        STR(VK_ERROR_SURFACE_LOST_KHR);
        STR(VK_ERROR_NATIVE_WINDOW_IN_USE_KHR);
        STR(VK_SUBOPTIMAL_KHR);
        STR(VK_ERROR_OUT_OF_DATE_KHR);
        STR(VK_ERROR_INCOMPATIBLE_DISPLAY_KHR);
        STR(VK_ERROR_VALIDATION_FAILED_EXT);
        STR(VK_ERROR_INVALID_SHADER_NV);
        STR(VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT);
        STR(VK_ERROR_NOT_PERMITTED_EXT);
        STR(VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT);
        STR(VK_THREAD_IDLE_KHR);
        STR(VK_THREAD_DONE_KHR);
        STR(VK_OPERATION_DEFERRED_KHR);
        STR(VK_OPERATION_NOT_DEFERRED_KHR);
        STR(VK_PIPELINE_COMPILE_REQUIRED_EXT);
        default:
            return "UNKNOWN_ERROR";
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_utils_messenger_callback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_type,
        const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
        void *user_data)
{
    if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        std::cout << "Callback Warning: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl;
    }
    else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        std::cerr << "Callback Error: " << callback_data->messageIdNumber << ":" << callback_data->pMessageIdName << ":" <<  callback_data->pMessage << std::endl;
    }
    return VK_FALSE;
}

#define VK_CHECK(res) if(res){std::cout << "Error at line:" << __LINE__ << ", Error name:" << to_string(res) << ".\n"; exit(1);}

int main()
{
    // Verify that the debug extension for the callback messenger is supported.
    uint32_t propNum;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, nullptr));
    assert(propNum >= 1);
    std::vector<VkExtensionProperties> props(propNum);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &propNum, props.data()));
    for (int i = 0; i < props.size(); ++i)
    {
        if(strcmp(props[i].extensionName, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0)
        {
            break;
        }
        if(i == propNum - 1)
        {
            std::cout << "Something went very wrong, cannot find " << VK_EXT_DEBUG_UTILS_EXTENSION_NAME << " extension"
                      << std::endl;
            exit(1);
        }
    }

    // Create the debug callback messenger info for instance create and destroy check.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debug_utils_messenger_callback;
    }

    // Verify that the validation layer for Khronos validation is supported
    uint32_t layerNum;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerNum, nullptr));
    assert(layerNum >= 1);
    std::vector<VkLayerProperties> layers(layerNum);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerNum, layers.data()));
    for (int i = 0; i < layerNum; ++i)
    {
        if(strcmp("VK_LAYER_KHRONOS_validation", layers[i].layerName) == 0)
        {
            break;
        }
        if(i == layerNum - 1)
        {
            std::cout << "Something went very wrong, cannot find VK_LAYER_KHRONOS_validation extension" << std::endl;
            exit(1);
        }
    }

    // Initialize instance and application
    VkApplicationInfo appInfo{}; // TIPS: You can delete this bracket to see what happens.
    {
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "DumpTriRT";
        appInfo.applicationVersion = 1;
        appInfo.pEngineName = "VulkanDict";
        appInfo.engineVersion = 1;
        appInfo.apiVersion = VK_API_VERSION_1_3;
    }
    
    const std::vector<const char*> requiredInstExtensions = {
        VK_EXT_DEBUG_UTILS_EXTENSION_NAME, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    const char* validationLayerName = "VK_LAYER_KHRONOS_validation";
    VkInstanceCreateInfo instanceCreateInfo{};
    {
        instanceCreateInfo.sType = VkStructureType::VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pNext = &debugCreateInfo;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = requiredInstExtensions.size();
        instanceCreateInfo.ppEnabledExtensionNames = requiredInstExtensions.data();
        instanceCreateInfo.enabledLayerCount = 1;
        instanceCreateInfo.ppEnabledLayerNames = &validationLayerName;
    }
    VkInstance instance;
    VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &instance));

    // Create debug messenger
    VkDebugUtilsMessengerEXT debugMessenger;
    auto fpVkCreateDebugUtilsMessengerExt = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if(fpVkCreateDebugUtilsMessengerExt == nullptr)
    {
        exit(1);
    }
    VK_CHECK(fpVkCreateDebugUtilsMessengerExt(instance, &debugCreateInfo, nullptr, &debugMessenger));

    // Enumerate the physicalDevices.
    // Write down devices that support VulkanRT:
    // - VK_KHR_acceleration_structure
    // - VK_KHR_deferred_host_operations
    // - VK_KHR_ray_query
    // - VK_KHR_ray_tracing_maintenance1
    // - VK_KHR_ray_tracing_pipeline
    // Select the first one that fulfills the requirements and display the name of it.
    const std::vector<const char*> requiredDeviceExtensions = {
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
        VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME,
        VK_KHR_MAINTENANCE3_EXTENSION_NAME,
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME
    };

    VkPhysicalDevice physicalDevice;
    uint32_t phyDeviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, nullptr));
    assert(phyDeviceCount >= 1);
    std::vector<VkPhysicalDevice> phyDeviceVec(phyDeviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &phyDeviceCount, phyDeviceVec.data()));

    for (VkPhysicalDevice phyDevice : phyDeviceVec)
    {
        VkPhysicalDeviceProperties physicalDevProperties;
        vkGetPhysicalDeviceProperties(phyDevice, &physicalDevProperties);
        std::cout << "Device name:" << physicalDevProperties.deviceName << std::endl;

        // Note - if you don't include VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME in
        // ppEnabledExtensionNames when you create instance, there will be no rayTracingPipelines
        
        // Check if ray query is supported.
        VkPhysicalDeviceRayQueryFeaturesKHR phyDevRayQueryFeatures = {};
        {
            phyDevRayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
        }

        // Check if Acceleration structure is supported. (Whether the native acceleration structure is supported.)
        VkPhysicalDeviceAccelerationStructureFeaturesKHR phyDevAccStructFeatures = {};
        {
            phyDevAccStructFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
            phyDevAccStructFeatures.pNext = &phyDevRayQueryFeatures;
        }

        // Check if ray tracing extension is supported
        VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtPipelineFeatures = {};
        {
            rtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            rtPipelineFeatures.pNext = &phyDevAccStructFeatures;
        }

        VkPhysicalDeviceFeatures2 phyDevFeatures2 = {};
        {
            phyDevFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            phyDevFeatures2.pNext = &rtPipelineFeatures;
        }

        vkGetPhysicalDeviceFeatures2(phyDevice, &phyDevFeatures2);

        std::cout << "Support raytracing pipeline: " << rtPipelineFeatures.rayTracingPipeline << std::endl;

        if (rtPipelineFeatures.rayTracingPipeline)
        {
            physicalDevice = phyDevice;

            // Accerlation structure properties.
            VkPhysicalDeviceAccelerationStructurePropertiesKHR phyDevAccStructProperties = {};
            {
                phyDevAccStructProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
            }

            // Ray tracing properties.
            VkPhysicalDeviceRayTracingPipelinePropertiesKHR phyDevRtPipelineProperties = {};
            {
                phyDevRtPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
                phyDevRtPipelineProperties.pNext = &phyDevAccStructProperties;
            }

            VkPhysicalDeviceProperties2 phyDevRtProperties2 = {};
            {
                phyDevRtProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
                phyDevRtProperties2.pNext = &phyDevRtPipelineProperties;
            }

            vkGetPhysicalDeviceProperties2(phyDevice, &phyDevRtProperties2);

            std::cout << "Max recursion depth: " << phyDevRtPipelineProperties.maxRayRecursionDepth << std::endl;

            std::cout << "Has acceleration structure: " << phyDevAccStructFeatures.accelerationStructure << std::endl;

            std::cout << "Max acceleration structure geometry count: " << phyDevAccStructProperties.maxGeometryCount << std::endl;

            std::cout << "Has ray query: " << phyDevRayQueryFeatures.rayQuery << std::endl;
        }

        // Choose the device if the physical device supports all extensions
        std::unordered_set<std::string> requiredDeviceExtensionsCheckList(requiredDeviceExtensions.begin(), requiredDeviceExtensions.end());

        std::vector<VkExtensionProperties> phyDevExtensionProperties;
        uint32_t propertiesCnt;
        vkEnumerateDeviceExtensionProperties(phyDevice, NULL, &propertiesCnt, nullptr);
        phyDevExtensionProperties.resize(propertiesCnt);
        vkEnumerateDeviceExtensionProperties(phyDevice, NULL, &propertiesCnt, phyDevExtensionProperties.data());

        for (const VkExtensionProperties& prop : phyDevExtensionProperties)
        {
            requiredDeviceExtensionsCheckList.erase(prop.extensionName);
            if (requiredDeviceExtensionsCheckList.size() == 0)
            {
                std::cout << "Choose this physical device for ray tracing." << std::endl;
                break;
            }
        }

        // Only select the first supported physical device.
        if (requiredDeviceExtensionsCheckList.size() == 0)
        {
            break;
        }
    }

    // Find a queue that supports both compute and graphics.
    uint32_t queueFamilyPropCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, nullptr);
    assert(queueFamilyPropCount >= 1);
    std::vector<VkQueueFamilyProperties> queueFamilyProps(queueFamilyPropCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyPropCount, queueFamilyProps.data());
    
    uint32_t queueId = ~0;
    // for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProps)
    for(uint32_t i = 0; i < queueFamilyProps.size(); i++)
    {
        const VkQueueFamilyProperties& queueFamilyProperty = queueFamilyProps[i];
        bool supportsGraphics = (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) > 0;
        bool supportsCompute = (queueFamilyProperty.queueFlags & VK_QUEUE_COMPUTE_BIT) > 0;

        if (supportsGraphics && supportsCompute)
        {
            queueId = i;
            break;
        }
    }

    if (queueId == ~0)
    {
        std::cerr << "Unable to find a queue that supports both compute and graphic family" << std::endl;
    }

    // Create the logical device
    float queuePriority = 1.f;
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    {
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueId;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
    }

    // Setup required rt features
    VkPhysicalDeviceBufferDeviceAddressFeatures phyDeviceBufferDeviceAddrFeatures = {};
    {
        phyDeviceBufferDeviceAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddress = true;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressCaptureReplay = false;
        phyDeviceBufferDeviceAddrFeatures.bufferDeviceAddressMultiDevice = false;
    }

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR phyDeviceRTPipelineFeatures = {};
    {
        phyDeviceRTPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
        phyDeviceRTPipelineFeatures.pNext = &phyDeviceBufferDeviceAddrFeatures;
        phyDeviceRTPipelineFeatures.rayTracingPipeline = true;
    }

    VkPhysicalDeviceAccelerationStructureFeaturesKHR phyDeviceAccStructureFeatures = {};
    {
        phyDeviceAccStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
        phyDeviceAccStructureFeatures.pNext = &phyDeviceRTPipelineFeatures;
        phyDeviceAccStructureFeatures.accelerationStructure = true;
        phyDeviceAccStructureFeatures.accelerationStructureCaptureReplay = true;
        phyDeviceAccStructureFeatures.accelerationStructureIndirectBuild = false;
        phyDeviceAccStructureFeatures.accelerationStructureHostCommands = false;
        phyDeviceAccStructureFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = false;
    }

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamicRenderingFeature{};
    {
        dynamicRenderingFeature.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;
        dynamicRenderingFeature.pNext = &phyDeviceAccStructureFeatures;
        dynamicRenderingFeature.dynamicRendering = VK_TRUE;
    }

    VkDeviceCreateInfo deviceInfo{};
    {
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pNext = &dynamicRenderingFeature;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceInfo.enabledExtensionCount = requiredDeviceExtensions.size();
        deviceInfo.ppEnabledExtensionNames = requiredDeviceExtensions.data();
        deviceInfo.pEnabledFeatures = nullptr;
    }

    VkDevice device;
    VK_CHECK(vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device));

    // Create a command buffer pool and allocate a command buffer
    VkCommandPoolCreateInfo cmdPoolInfo = {};
    {
        cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        cmdPoolInfo.queueFamilyIndex = queueId;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    }
    VkCommandPool cmdPool;
    VK_CHECK(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));

    VkCommandBufferAllocateInfo cmdBufferAllocInfo{};
    {
        cmdBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdBufferAllocInfo.commandPool = cmdPool;
        cmdBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdBufferAllocInfo.commandBufferCount = 1;
    }
    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdBufferAllocInfo, &cmdBuffer));

    // Init VMA
    VmaVulkanFunctions vkFuncs = {};
    {
        vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
        vkFuncs.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;
    }

    VmaAllocatorCreateInfo allocCreateInfo = {};
    {
        allocCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_3;
        allocCreateInfo.physicalDevice = physicalDevice;
        allocCreateInfo.device = device;
        allocCreateInfo.instance = instance;
        allocCreateInfo.pVulkanFunctions = &vkFuncs;
    }

    VmaAllocator allocator;
    vmaCreateAllocator(&allocCreateInfo, &allocator);

    //
    //
    // BLAS - Bottom Level Acceleration Structure (Verts/Tris)
    //
    //
    const uint32_t numTriangles = 1;
    const uint32_t idxCount = 3;

    float vertices[9] = {
        1.f, 1.f, 0.f,
        -1.f, 1.f, 0.f,
        0.f, -1.f, 0.f
    };

    uint32_t indices[3] = {0, 1, 2};

    // Note: RT doesn't have perspective so it's just <3, 4> x <4, 1>
    VkTransformMatrixKHR transformMatrix = {
        1.f, 0.f, 0.f, 0.f,
        0.f, 1.f, 0.f, 0.f,
        0.f, 0.f, 1.f, 0.f
    };

    // Vertex buffer
    VkBuffer vertBuffer;
    VmaAllocation vertBufferAlloc;
    VkBufferCreateInfo vertBufferInfo = {};
    {
        vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        vertBufferInfo.size = sizeof(vertices);
        vertBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        vertBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo vertBufferAllocInfo = {};
    {
        vertBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        vertBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &vertBufferInfo, &vertBufferAllocInfo, &vertBuffer, &vertBufferAlloc, nullptr);
    
    void* pVertGpuAddr = nullptr;
    vmaMapMemory(allocator, vertBufferAlloc, &pVertGpuAddr);
    memcpy(pVertGpuAddr, vertices, sizeof(vertices));
    vmaUnmapMemory(allocator, vertBufferAlloc);

    VkDeviceOrHostAddressConstKHR vertBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = vertBuffer;
        }
        vertBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Index buffer
    VkBuffer idxBuffer;
    VmaAllocation idxBufferAlloc;
    VkBufferCreateInfo idxBufferInfo = {};
    {
        idxBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        idxBufferInfo.size = sizeof(indices);
        idxBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        idxBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo idxBufferAllocInfo = {};
    {
        idxBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        idxBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                   VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &idxBufferInfo, &idxBufferAllocInfo, &idxBuffer, &idxBufferAlloc, nullptr);

    void* pIdxGpuAddr = nullptr;
    vmaMapMemory(allocator, idxBufferAlloc, &pIdxGpuAddr);
    memcpy(pIdxGpuAddr, indices, sizeof(indices));
    vmaUnmapMemory(allocator, idxBufferAlloc);

    VkDeviceOrHostAddressConstKHR idxBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = idxBuffer;
        }
        idxBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Transform matrix buffer
    VkBuffer transformMatrixBuffer;
    VmaAllocation transformMatrixAlloc;
    VkBufferCreateInfo transformMatrixBufferInfo = {};
    {
        transformMatrixBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        transformMatrixBufferInfo.size = sizeof(transformMatrix);
        transformMatrixBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        transformMatrixBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo transformMatrixAllocInfo = {};
    {
        transformMatrixAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        transformMatrixAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                         VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator, &transformMatrixBufferInfo, &transformMatrixAllocInfo, &transformMatrixBuffer, &transformMatrixAlloc, nullptr);

    void* pTransformMatrixGpuAddr = nullptr;
    vmaMapMemory(allocator, transformMatrixAlloc, &pTransformMatrixGpuAddr);
    memcpy(pTransformMatrixGpuAddr, transformMatrix.matrix, sizeof(transformMatrix));
    vmaUnmapMemory(allocator, transformMatrixAlloc);

    VkDeviceOrHostAddressConstKHR transformMatBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = transformMatrixBuffer;
        }
        transformMatBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    // Build the accerlation structure
    // Get the ray tracing and accelertion structure related function pointers required by this sample
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureBuildSizesKHR"));
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device, "vkGetAccelerationStructureDeviceAddressKHR"));
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device, "vkCmdBuildAccelerationStructuresKHR"));

    struct BottomLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
    };
    BottomLevelAccelerationStructure bottomLevelAccelerationStructure;

    VkAccelerationStructureGeometryKHR blasGeometry = {};
    {
        blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        blasGeometry.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        blasGeometry.flags = VkGeometryFlagBitsKHR::VK_GEOMETRY_OPAQUE_BIT_KHR;
        {
            VkAccelerationStructureGeometryDataKHR accStructureGeoData = {};
            VkAccelerationStructureGeometryTrianglesDataKHR accStructureGeoTriData = {};
            {
                accStructureGeoTriData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
                accStructureGeoTriData.vertexFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
                accStructureGeoTriData.vertexData = vertBufferDeviceAddr;
                accStructureGeoTriData.vertexStride = sizeof(float) * 3;
                accStructureGeoTriData.maxVertex = 2;
                accStructureGeoTriData.indexType = VK_INDEX_TYPE_UINT32;
                accStructureGeoTriData.indexData = idxBufferDeviceAddr;
                accStructureGeoTriData.transformData = transformMatBufferDeviceAddr;
            }
            accStructureGeoData.triangles = accStructureGeoTriData;
            blasGeometry.geometry = accStructureGeoData;
        }
    }

    VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeoInfo = {};
    {
        blasBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        blasBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        blasBuildGeoInfo.flags = VkBuildAccelerationStructureFlagBitsKHR::VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        blasBuildGeoInfo.mode = VkBuildAccelerationStructureModeKHR::VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        blasBuildGeoInfo.geometryCount = 1;
        blasBuildGeoInfo.pGeometries = &blasGeometry;
    }

    // Get BLAS size info
    VkAccelerationStructureBuildSizesInfoKHR blasBuildSizeInfo = {};
    {
        blasBuildSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    }
    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &blasBuildGeoInfo,
        &numTriangles,
        &blasBuildSizeInfo);

    // Create BLAS buffer
    VkBufferCreateInfo blasBufferInfo = {};
    {
        blasBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        blasBufferInfo.size  = blasBuildSizeInfo.accelerationStructureSize;
        blasBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        blasBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo blasBufferAllocInfo = {};
    {
        blasBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        blasBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &blasBufferInfo,
                    &blasBufferAllocInfo,
                    &bottomLevelAccelerationStructure.accStructureBuffer,
                    &bottomLevelAccelerationStructure.accStructureBufferAlloc,
                    nullptr);

    // Create BLAS scratch buffer
    VkBufferCreateInfo blasScratchBufferInfo = {};
    {
        blasScratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        blasScratchBufferInfo.size = blasBuildSizeInfo.buildScratchSize;
        blasScratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        blasScratchBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo blasScratchBufferAllocInfo = {};
    {
        blasScratchBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        blasScratchBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &blasScratchBufferInfo,
                    &blasScratchBufferAllocInfo,
                    &bottomLevelAccelerationStructure.scratchBuffer,
                    &bottomLevelAccelerationStructure.scratchBufferAlloc,
                    nullptr);

    VkAccelerationStructureCreateInfoKHR blasCreateInfo = {};
    {
        blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        blasCreateInfo.buffer = bottomLevelAccelerationStructure.accStructureBuffer;
        blasCreateInfo.size = blasBuildSizeInfo.accelerationStructureSize;
        blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    }
    vkCreateAccelerationStructureKHR(device, &blasCreateInfo, nullptr, &bottomLevelAccelerationStructure.accStructure);

    // Fill in the remaining BLAS meta info for building
    blasBuildGeoInfo.dstAccelerationStructure = bottomLevelAccelerationStructure.accStructure;
    VkBufferDeviceAddressInfo blasBuildScratchBufferDevAddrInfo = {};
    {
        blasBuildScratchBufferDevAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        blasBuildScratchBufferDevAddrInfo.buffer = bottomLevelAccelerationStructure.scratchBuffer;
    }
    blasBuildGeoInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &blasBuildScratchBufferDevAddrInfo);
    
    VkAccelerationStructureBuildRangeInfoKHR blasBuildRangeInfo = {};
    {
        blasBuildRangeInfo.primitiveCount = numTriangles;
        blasBuildRangeInfo.primitiveOffset = 0;
        blasBuildRangeInfo.firstVertex = 0;
        blasBuildRangeInfo.transformOffset = 0;
    }
    VkAccelerationStructureBuildRangeInfoKHR* blasBuildRangeInfos[1] = { &blasBuildRangeInfo };

    // Tell the GPU and driver to build the BLAS.
    VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
    {
        cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    }
    
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &blasBuildGeoInfo, blasBuildRangeInfos);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
    
    vkDeviceWaitIdle(device);

    VkAccelerationStructureDeviceAddressInfoKHR blasAddressInfo = {};
    {
        blasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        blasAddressInfo.accelerationStructure = bottomLevelAccelerationStructure.accStructure;
    }
    bottomLevelAccelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &blasAddressInfo);
    
    vkResetCommandBuffer(cmdBuffer, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    // Destroy build scratch buffers
    vmaDestroyBuffer(allocator,
                     bottomLevelAccelerationStructure.scratchBuffer,
                     bottomLevelAccelerationStructure.scratchBufferAlloc);

    /*
        Build the TLAS. The top level acceleration structure contains the scene's object instances
    */
    struct TopLevelAccelerationStructure
    {
        VkAccelerationStructureKHR accStructure;
        VkBuffer accStructureBuffer;
        VmaAllocation accStructureBufferAlloc;
        uint64_t deviceAddress;
        VkBuffer scratchBuffer;
        VmaAllocation scratchBufferAlloc;
        VkBuffer instancesBuffer;
        VmaAllocation instancesBufferAlloc;
    };
    TopLevelAccelerationStructure topLevelAccelerationStructure{};

    VkAccelerationStructureInstanceKHR tlasInstance = {};
    {
        tlasInstance.transform = transformMatrix;
        tlasInstance.instanceCustomIndex = 0;
        tlasInstance.mask = 0xff;
        tlasInstance.instanceShaderBindingTableRecordOffset = 0;
        tlasInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        tlasInstance.accelerationStructureReference = bottomLevelAccelerationStructure.deviceAddress;
    }

    VkBufferCreateInfo instBufferInfo = {};
    {
        instBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        instBufferInfo.size = sizeof(VkAccelerationStructureInstanceKHR);
        instBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        instBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo instBufferAllocInfo = {};
    {
        instBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        instBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT |
                                    VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(allocator,
                    &instBufferInfo,
                    &instBufferAllocInfo,
                    &topLevelAccelerationStructure.instancesBuffer,
                    &topLevelAccelerationStructure.instancesBufferAlloc,
                    nullptr);

    void* pInstBuffer;
    vmaMapMemory(allocator, topLevelAccelerationStructure.instancesBufferAlloc, &pInstBuffer);
    memcpy(pInstBuffer, &tlasInstance, sizeof(tlasInstance));
    vmaUnmapMemory(allocator, topLevelAccelerationStructure.instancesBufferAlloc);

    VkDeviceOrHostAddressConstKHR instBufferDeviceAddr = {};
    {
        VkBufferDeviceAddressInfo bufferDeviceAddrInfo = {};
        {
            bufferDeviceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
            bufferDeviceAddrInfo.buffer = topLevelAccelerationStructure.instancesBuffer;
        }
        instBufferDeviceAddr.deviceAddress = vkGetBufferDeviceAddress(device, &bufferDeviceAddrInfo);
    }

    VkAccelerationStructureGeometryKHR tlasStructureGeo = {};
    {
        tlasStructureGeo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        tlasStructureGeo.geometryType = VkGeometryTypeKHR::VK_GEOMETRY_TYPE_INSTANCES_KHR;
        tlasStructureGeo.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        tlasStructureGeo.geometry.instances.arrayOfPointers = VK_FALSE;
        tlasStructureGeo.geometry.instances.data = instBufferDeviceAddr;
    }

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeoInfo{};
    {
        tlasBuildGeoInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        tlasBuildGeoInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        tlasBuildGeoInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
        tlasBuildGeoInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        tlasBuildGeoInfo.geometryCount = 1;
        tlasBuildGeoInfo.pGeometries = &tlasStructureGeo;
    }

    VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo{};
    {
        tlasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    }

    vkGetAccelerationStructureBuildSizesKHR(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildGeoInfo,
        &numTriangles,
        &tlasBuildSizesInfo);

    VkBufferCreateInfo tlasBufferInfo = {};
    {
        tlasBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        tlasBufferInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
        tlasBufferInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        tlasBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo tlasBufferAllocInfo = {};
    {
        tlasBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        tlasBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &tlasBufferInfo,
                    &tlasBufferAllocInfo,
                    &topLevelAccelerationStructure.accStructureBuffer,
                    &topLevelAccelerationStructure.accStructureBufferAlloc,
                    nullptr);

    // Create TLAS scratch buffer
    VkBufferCreateInfo tlasScratchBufferInfo = {};
    {
        tlasScratchBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        tlasScratchBufferInfo.size = tlasBuildSizesInfo.buildScratchSize;
        tlasScratchBufferInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
        tlasScratchBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    VmaAllocationCreateInfo tlasScratchBufferAllocInfo = {};
    {
        tlasScratchBufferAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        tlasScratchBufferAllocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
    }

    vmaCreateBuffer(allocator,
                    &tlasScratchBufferInfo,
                    &tlasScratchBufferAllocInfo,
                    &topLevelAccelerationStructure.scratchBuffer,
                    &topLevelAccelerationStructure.scratchBufferAlloc,
                    nullptr);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {};
    {
        tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        tlasCreateInfo.buffer = topLevelAccelerationStructure.accStructureBuffer;
        tlasCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
        tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    }
    vkCreateAccelerationStructureKHR(device, &tlasCreateInfo, nullptr, &topLevelAccelerationStructure.accStructure);
    tlasBuildGeoInfo.dstAccelerationStructure = topLevelAccelerationStructure.accStructure;

    VkBufferDeviceAddressInfo tlasBuildScratchBufferDevAddrInfo = {};
    {
        tlasBuildScratchBufferDevAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        tlasBuildScratchBufferDevAddrInfo.buffer = topLevelAccelerationStructure.scratchBuffer;
    }
    tlasBuildGeoInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device, &tlasBuildScratchBufferDevAddrInfo);

    VkAccelerationStructureBuildRangeInfoKHR tlasBuildRangeInfo = {};
    {
        tlasBuildRangeInfo.primitiveCount = 1;
        tlasBuildRangeInfo.primitiveOffset = 0;
        tlasBuildRangeInfo.firstVertex = 0;
        tlasBuildRangeInfo.transformOffset = 0;
    }
    VkAccelerationStructureBuildRangeInfoKHR* tlasBuildRangeInfos[1] = { &tlasBuildRangeInfo };

    // Tell the GPU and driver to build the TLAS.
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &cmdBufferBeginInfo));

    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &tlasBuildGeoInfo, tlasBuildRangeInfos);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer));

    vkDeviceWaitIdle(device);

    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.scratchBuffer,
                     topLevelAccelerationStructure.scratchBufferAlloc);

    VkAccelerationStructureDeviceAddressInfoKHR tlasAddressInfo = {};
    {
        tlasAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        tlasAddressInfo.accelerationStructure = topLevelAccelerationStructure.accStructure;
    }
    topLevelAccelerationStructure.deviceAddress = vkGetAccelerationStructureDeviceAddressKHR(device, &tlasAddressInfo);

    // Destroy TLAS structure
    vkDestroyAccelerationStructureKHR(device, topLevelAccelerationStructure.accStructure, nullptr);

    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.accStructureBuffer,
                     topLevelAccelerationStructure.accStructureBufferAlloc);
    vmaDestroyBuffer(allocator,
                     topLevelAccelerationStructure.instancesBuffer,
                     topLevelAccelerationStructure.instancesBufferAlloc);

    // Destroy BLAS structure
    vkDestroyAccelerationStructureKHR(device, bottomLevelAccelerationStructure.accStructure, nullptr);

    // Destroy buffers
    vmaDestroyBuffer(allocator,
                     bottomLevelAccelerationStructure.accStructureBuffer,
                     bottomLevelAccelerationStructure.accStructureBufferAlloc);
    vmaDestroyBuffer(allocator, idxBuffer, idxBufferAlloc);
    vmaDestroyBuffer(allocator, vertBuffer, vertBufferAlloc);
    vmaDestroyBuffer(allocator, transformMatrixBuffer, transformMatrixAlloc);

    // Destroy the command pool and release the command buffer
    vkFreeCommandBuffers(device, cmdPool, 1, &cmdBuffer);
    vkDestroyCommandPool(device, cmdPool, nullptr);

    // Destroy the allocator
    vmaDestroyAllocator(allocator);

    vkDestroyDevice(device, nullptr);

    // Destroy debug messenger
    auto fpVkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if(fpVkDestroyDebugUtilsMessengerEXT == nullptr)
    {
        exit(1);
    }
    fpVkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);

    // Destroy instance
    vkDestroyInstance(instance, nullptr);
}
