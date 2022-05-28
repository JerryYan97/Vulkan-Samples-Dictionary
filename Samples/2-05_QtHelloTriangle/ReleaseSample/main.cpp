#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include <QGuiApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QVulkanWindow>
#include <QVulkanDeviceFunctions>
#include <fstream>

Q_LOGGING_CATEGORY(lcVk, "qt.vulkan")

/* QT dynamically loads the vulkan functions, which is set by VK_NO_PROTOTYPES. So, we cannot statically use them.
 * Alternatively, we can also use the following function to load the vulkan functions if they are not specified in the
 * QVulkanDeviceFunctions.
 * Besides, VMA also needs to load vulkan functions dynamically so if we need VMA then the declaration below is also
 * necessary.
 * */
extern "C" {
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName);
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName);
}

// Format: local_x, local_y, local_z, col_r, col_g, col_b;
// Front: CCW
static float vertexData[] = {
    0.5f, 0.f, 0.f, 1.f, 0.f, 0.f, // Right bottom
    0.f, 1.f, 0.f, 0.f, 1.f, 0.f, // Top
    -0.5f, 0.f, 0.f, 0.f, 0.f, 1.f // Left bottom
};

VkShaderModule createShaderModule(const std::string& spvName, const VkDevice& device)
{
    // Create  Shader Module -- SOURCE_PATH is a MACRO definition passed in during compilation, which is specified in
    //                          the CMakeLists.txt file in the same level of repository.
    std::string shaderPath = std::string(SOURCE_PATH) + spvName;
    std::ifstream inputShader(shaderPath.c_str(), std::ios::binary | std::ios::in);
    std::vector<unsigned char> inputShaderStr(std::istreambuf_iterator<char>(inputShader), {});
    inputShader.close();
    VkShaderModuleCreateInfo shaderModuleCreateInfo{};
    {
        shaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleCreateInfo.codeSize = inputShaderStr.size();
        shaderModuleCreateInfo.pCode = (uint32_t*) inputShaderStr.data();
    }
    VkShaderModule shaderModule;
    vkCreateShaderModule(device, &shaderModuleCreateInfo, nullptr, &shaderModule);

    return shaderModule;
}

class VulkanRenderer : public QVulkanWindowRenderer
{
public:
    VulkanRenderer(QVulkanWindow *w)
        : m_window(w)
    {}

    void initResources() override
    {
        qDebug("initResources");
        const VkPhysicalDevice& physicalDevice = m_window->physicalDevice();
        const VkDevice& device = m_window->device();
        const VkInstance& instance = m_window->vulkanInstance()->vkInstance();
        m_devFuncs = m_window->vulkanInstance()->deviceFunctions(device);

        // Prepare the vertex and uniform buffer.
        VmaVulkanFunctions vkFuncs = {};
        {
            vkFuncs.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
            vkFuncs.vkGetDeviceProcAddr   = &vkGetDeviceProcAddr;
        }

        VmaAllocatorCreateInfo allocCreateInfo = {};
        {
            allocCreateInfo.vulkanApiVersion = VK_API_VERSION_1_2;
            allocCreateInfo.physicalDevice   = physicalDevice;
            allocCreateInfo.device           = device;
            allocCreateInfo.instance         = instance;
            allocCreateInfo.pVulkanFunctions = &vkFuncs;
        }

        vmaCreateAllocator(&allocCreateInfo, &m_vmaAllocator);

        VkBufferCreateInfo vertBufferInfo = {};
        {
            vertBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vertBufferInfo.size = sizeof(float) * 18;
            vertBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        }

        VmaAllocationCreateInfo allocInfo = {};
        {
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
                              VMA_ALLOCATION_CREATE_MAPPED_BIT;
        }

        vmaCreateBuffer(m_vmaAllocator, &vertBufferInfo, &allocInfo, &m_vertBuffer, &m_vertAllocation, nullptr);

        void* mappedData;
        vmaMapMemory(m_vmaAllocator, m_vertAllocation, &mappedData);
        memcpy(mappedData, vertexData, sizeof(float) * 18);
        vmaUnmapMemory(m_vmaAllocator, m_vertAllocation);

        VkBufferCreateInfo uniformBufferInfo = {};
        {
            uniformBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            uniformBufferInfo.size = sizeof(float) * 16;
            uniformBufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        }

        vmaCreateBuffer(m_vmaAllocator, &uniformBufferInfo, &allocInfo, &m_uniformBuffer, &m_uniformAllocation, nullptr);

        // Pipeline creation.
        // Pipeline vertex input state init.
        VkVertexInputBindingDescription vertBindingDesc = {};
        {
            vertBindingDesc.binding = 0;
            vertBindingDesc.stride = 6 * sizeof(float);
            vertBindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        }

        VkVertexInputAttributeDescription vertAttrDesc[2];
        {
            // Position
            vertAttrDesc[0].location = 0;
            vertAttrDesc[0].binding = 0;
            vertAttrDesc[0].format = VK_FORMAT_R32G32B32_SFLOAT;
            vertAttrDesc[0].offset = 0;
            // Color
            vertAttrDesc[1].location = 1;
            vertAttrDesc[1].binding = 0;
            vertAttrDesc[1].format = VK_FORMAT_R32G32B32_SFLOAT;
            vertAttrDesc[1].offset = 3 * sizeof(float);
        }

        VkPipelineVertexInputStateCreateInfo pipelineVertInputInfo = {};
        {
            pipelineVertInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            pipelineVertInputInfo.pNext = nullptr;
            pipelineVertInputInfo.vertexBindingDescriptionCount = 1;
            pipelineVertInputInfo.pVertexBindingDescriptions = &vertBindingDesc;
            pipelineVertInputInfo.vertexAttributeDescriptionCount = 2;
            pipelineVertInputInfo.pVertexAttributeDescriptions = vertAttrDesc;
        }

        // Descriptor allocation
        VkDescriptorPoolSize descriptorPoolSize = {};
        {
            descriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorPoolSize.descriptorCount = 1;
        }

        VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
        {
            descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            descriptorPoolInfo.maxSets = 1;
            descriptorPoolInfo.poolSizeCount = 1;
            descriptorPoolInfo.pPoolSizes = &descriptorPoolSize;
        }

        VkResult res = m_devFuncs->vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &m_descPool);
        if(res != VK_SUCCESS)
        {
            qFatal("Failed to create descriptor pool: %d", res);
        }

        VkDescriptorSetLayoutBinding uniformBufDesSetLayoutBinding = {};
        {
            uniformBufDesSetLayoutBinding.binding = 1;
            uniformBufDesSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformBufDesSetLayoutBinding.descriptorCount = 1;
            uniformBufDesSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            uniformBufDesSetLayoutBinding.pImmutableSamplers = nullptr;
        }

        VkDescriptorSetLayoutCreateInfo descLayoutInfo = {};
        {
            descLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            descLayoutInfo.bindingCount = 1;
            descLayoutInfo.pBindings = &uniformBufDesSetLayoutBinding;
        }

        res = m_devFuncs->vkCreateDescriptorSetLayout(device, &descLayoutInfo, nullptr, &m_descSetLayout);
        if (res != VK_SUCCESS)
        {
            qFatal("Failed to create descriptor set layout: %d", res);
        }

        VkDescriptorSetAllocateInfo uniformBufDesSetAllocInfo = {};
        {
            uniformBufDesSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            uniformBufDesSetAllocInfo.pNext = nullptr;
            uniformBufDesSetAllocInfo.descriptorPool = m_descPool;
            uniformBufDesSetAllocInfo.descriptorSetCount = 1;
            uniformBufDesSetAllocInfo.pSetLayouts = &m_descSetLayout;
        }
        res = m_devFuncs->vkAllocateDescriptorSets(device, &uniformBufDesSetAllocInfo, &m_uniformBufferDescriptorSet);
        if(res != VK_SUCCESS)
        {
            qFatal("Failed to allocate descriptor set: %d", res);
        }

        VkWriteDescriptorSet uniformBufDesSetWrite = {};
        {
            uniformBufDesSetWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformBufDesSetWrite.dstSet = m_uniformBufferDescriptorSet;
            uniformBufDesSetWrite.dstBinding = 1;
            uniformBufDesSetWrite.descriptorCount = 1;
            uniformBufDesSetWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            m_uniformBufferDescriptorInfo.buffer = m_uniformBuffer;
            m_uniformBufferDescriptorInfo.offset = 0;
            m_uniformBufferDescriptorInfo.range = sizeof(float) * 16;
            uniformBufDesSetWrite.pBufferInfo = &m_uniformBufferDescriptorInfo;
        }
        m_devFuncs->vkUpdateDescriptorSets(device, 1, &uniformBufDesSetWrite, 0, nullptr);

        // Pipeline cache
        VkPipelineCacheCreateInfo pipelineCacheInfo = {};
        {
            pipelineCacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
        }
        res = m_devFuncs->vkCreatePipelineCache(device, &pipelineCacheInfo, nullptr, &m_pipelineCache);
        if(res != VK_SUCCESS)
        {
            qFatal("Failed to create pipeline cache: %d", res);
        }

        // Pipeline layout
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
        {
            pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutCreateInfo.setLayoutCount = 1;
            pipelineLayoutCreateInfo.pSetLayouts = &m_descSetLayout;
        }
        res = m_devFuncs->vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout);
        if(res != VK_SUCCESS)
        {
            qFatal("Failed to create pipeline layout: %d", res);
        }

        // Shaders
        VkShaderModule vertShaderModule = createShaderModule("/qtTri_vert.spv", device);
        VkShaderModule fragShaderModule = createShaderModule("/qtTri_frag.spv", device);
        VkPipelineShaderStageCreateInfo shaderStgInfo[2];
        {
            shaderStgInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStgInfo[0].pNext = nullptr;
            shaderStgInfo[0].flags = 0;
            shaderStgInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
            shaderStgInfo[0].module = vertShaderModule;
            shaderStgInfo[0].pName = "main";
            shaderStgInfo[0].pSpecializationInfo = nullptr;

            shaderStgInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStgInfo[1].pNext = nullptr;
            shaderStgInfo[1].flags = 0;
            shaderStgInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            shaderStgInfo[1].module = fragShaderModule;
            shaderStgInfo[1].pName = "main";
            shaderStgInfo[1].pSpecializationInfo = nullptr;
        }

        // Input Assembly
        VkPipelineInputAssemblyStateCreateInfo iaInfo = {};
        {
            iaInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            iaInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        }

        // Viewport and scissor setting
        VkPipelineViewportStateCreateInfo vpInfo = {};
        {
            vpInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            vpInfo.viewportCount = 1;
            vpInfo.scissorCount = 1;
        }

        // Rasterization setting
        VkPipelineRasterizationStateCreateInfo rsInfo = {};
        {
            rsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rsInfo.polygonMode = VK_POLYGON_MODE_FILL;
            rsInfo.cullMode = VK_CULL_MODE_NONE;
            rsInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            rsInfo.lineWidth = 1.f;
        }

        // Multi-sample setting
        VkPipelineMultisampleStateCreateInfo msInfo = {};
        {
            msInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            msInfo.rasterizationSamples = m_window->sampleCountFlagBits();
        }

        // Depth stencil setting
        VkPipelineDepthStencilStateCreateInfo dsInfo = {};
        {
            dsInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            dsInfo.depthTestEnable = VK_TRUE;
            dsInfo.depthWriteEnable = VK_TRUE;
            dsInfo.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        }

        // Color blend setting
        VkPipelineColorBlendStateCreateInfo cbInfo = {};
        {
            cbInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            VkPipelineColorBlendAttachmentState att = {};
            att.colorWriteMask = 0xF;
            cbInfo.attachmentCount = 1;
            cbInfo.pAttachments = &att;
        }

        // Dynamic states
        VkPipelineDynamicStateCreateInfo dynInfo = {};
        {
            dynInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            VkDynamicState dynEnable[2];
            dynEnable[0] = VK_DYNAMIC_STATE_VIEWPORT;
            dynEnable[1] = VK_DYNAMIC_STATE_SCISSOR;
            dynInfo.dynamicStateCount = 2;
            dynInfo.pDynamicStates = dynEnable;
        }

        // Create Graphics pipeline
        VkGraphicsPipelineCreateInfo pipelineInfo = {};
        {
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStgInfo;
            pipelineInfo.pVertexInputState = &pipelineVertInputInfo;
            pipelineInfo.pInputAssemblyState = &iaInfo;
            pipelineInfo.pViewportState = &vpInfo;
            pipelineInfo.pRasterizationState = &rsInfo;
            pipelineInfo.pMultisampleState = &msInfo;
            pipelineInfo.pDepthStencilState = &dsInfo;
            pipelineInfo.pColorBlendState = &cbInfo;
            pipelineInfo.pDynamicState = &dynInfo;
            pipelineInfo.layout = m_pipelineLayout;
            pipelineInfo.renderPass = m_window->defaultRenderPass();
        }

        res = m_devFuncs->vkCreateGraphicsPipelines(device, m_pipelineCache, 1, &pipelineInfo, nullptr, &m_pipeline);
        if(res != VK_SUCCESS)
        {
            qFatal("Failed to create graphics pipeline: %d", res);
        }

        if (vertShaderModule)
        {
            m_devFuncs->vkDestroyShaderModule(device, vertShaderModule, nullptr);
        }

        if(fragShaderModule)
        {
            m_devFuncs->vkDestroyShaderModule(device, fragShaderModule, nullptr);
        }
    }

    void initSwapChainResources() override
    {
        qDebug("initSwapChainResources");

        // Init MVP matrix
        m_mvp = m_window->clipCorrectionMatrix();
        const QSize sz = m_window->swapChainImageSize();
        float aspectRatio = (float) sz.width() / (float) sz.height();
        m_mvp.perspective(45.f, aspectRatio, 0.01f, 100.f);
        m_mvp.lookAt({0.f, 0.f, 0.f}, {0.f, 0.f, -1.f}, {0.f, 1.f, 0.f});
        m_mvp.translate({0.f, -0.5f, -3.f});
    }

    void releaseSwapChainResources() override
    {
        qDebug("releaseSwapChainResources");
    }

    void releaseResources() override
    {
        qDebug("releaseResources");

        VkDevice dev = m_window->device();

        if (m_pipeline) {
            m_devFuncs->vkDestroyPipeline(dev, m_pipeline, nullptr);
            m_pipeline = VK_NULL_HANDLE;
        }

        if (m_pipelineLayout) {
            m_devFuncs->vkDestroyPipelineLayout(dev, m_pipelineLayout, nullptr);
            m_pipelineLayout = VK_NULL_HANDLE;
        }

        if (m_pipelineCache) {
            m_devFuncs->vkDestroyPipelineCache(dev, m_pipelineCache, nullptr);
            m_pipelineCache = VK_NULL_HANDLE;
        }

        if (m_descSetLayout) {
            m_devFuncs->vkDestroyDescriptorSetLayout(dev, m_descSetLayout, nullptr);
            m_descSetLayout = VK_NULL_HANDLE;
        }

        if (m_descPool) {
            m_devFuncs->vkDestroyDescriptorPool(dev, m_descPool, nullptr);
            m_descPool = VK_NULL_HANDLE;
        }

        vmaDestroyBuffer(m_vmaAllocator, m_vertBuffer, m_vertAllocation);
        vmaDestroyAllocator(m_vmaAllocator);
    }

    void startNextFrame() override
    {
        VkDevice device = m_window->device();
        VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
        const QSize sz = m_window->swapChainImageSize();

        VkClearColorValue clearColor = {{ 0, 0, 0, 1 }};
        VkClearDepthStencilValue clearDS = { 1, 0 };
        VkClearValue clearValues[3];
        memset(clearValues, 0, sizeof(clearValues));
        clearValues[0].color = clearValues[2].color = clearColor;
        clearValues[1].depthStencil = clearDS;

        VkRenderPassBeginInfo rpBeginInfo;
        memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = m_window->defaultRenderPass();
        rpBeginInfo.framebuffer = m_window->currentFramebuffer();
        rpBeginInfo.renderArea.extent.width = sz.width();
        rpBeginInfo.renderArea.extent.height = sz.height();
        rpBeginInfo.clearValueCount = m_window->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
        rpBeginInfo.pClearValues = clearValues;
        m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);


        // Upload MVP matrix data to GPU
        QMatrix4x4 m = m_mvp;
        m.rotate(m_rotation, 0, 1, 0);
        void* mappedData;
        vmaMapMemory(m_vmaAllocator, m_uniformAllocation, &mappedData);
        memcpy(mappedData, m.data(), sizeof(float) * 16);
        vmaUnmapMemory(m_vmaAllocator, m_uniformAllocation);

        // Update rotation
        m_rotation += 1.0f;

        // Bind the graphics pipeline
        m_devFuncs->vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);
        m_devFuncs->vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipelineLayout, 0, 1,
                                            &m_uniformBufferDescriptorSet, 0, nullptr);
        VkDeviceSize vbOffset = 0;
        m_devFuncs->vkCmdBindVertexBuffers(cmdBuf, 0, 1, &m_vertBuffer, &vbOffset);

        VkViewport viewport;
        viewport.x = viewport.y = 0;
        viewport.width = sz.width();
        viewport.height = sz.height();
        viewport.minDepth = 0;
        viewport.maxDepth = 1;
        m_devFuncs->vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

        VkRect2D scissor;
        scissor.offset.x = scissor.offset.y = 0;
        scissor.extent.width = viewport.width;
        scissor.extent.height = viewport.height;
        m_devFuncs->vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

        m_devFuncs->vkCmdDraw(cmdBuf, 3, 1, 0, 0);

        m_devFuncs->vkCmdEndRenderPass(cmdBuf);

        m_window->frameReady();
        m_window->requestUpdate();
    }


protected:
    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;

    VmaAllocator m_vmaAllocator;
    VkBuffer m_vertBuffer;
    VmaAllocation m_vertAllocation;
    VkBuffer m_uniformBuffer;
    VmaAllocation m_uniformAllocation;
    VkDescriptorSet m_uniformBufferDescriptorSet;
    VkDescriptorBufferInfo m_uniformBufferDescriptorInfo;

    VkDescriptorPool m_descPool;
    VkDescriptorSetLayout m_descSetLayout;
    VkPipelineLayout m_pipelineLayout;
    VkPipeline m_pipeline;

    VkPipelineCache m_pipelineCache;

    QMatrix4x4 m_mvp;
    float m_rotation = 0.0f;
};

class VulkanWindow : public QVulkanWindow
{
public:
    QVulkanWindowRenderer *createRenderer() override
    {
        return new VulkanRenderer(this);
    }
};

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    QLoggingCategory::setFilterRules(QStringLiteral("qt.vulkan=true"));

    QVulkanInstance inst;
    inst.setLayers({ "VK_LAYER_KHRONOS_validation" });
    inst.setApiVersion({1, 2});

    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    qDebug(qUtf8Printable(inst.apiVersion().toString()));

    VulkanWindow w;
    w.setVulkanInstance(&inst);

    w.resize(1024, 768);
    w.show();

    return app.exec();
}
