#include <QGuiApplication>
#include <QVulkanInstance>
#include <QLoggingCategory>
#include <QVulkanWindow>
#include <QVulkanDeviceFunctions>

/* QT dynamically loads the vulkan functions, which is set by VK_NO_PROTOTYPES. So, we cannot statically use them.
 * Alternatively, we can also use the following function to load the vulkan functions if they are not specified in the
 * QVulkanDeviceFunctions. */
extern "C" {
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *pName);
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *pName);
}


class VulkanRenderer : public QVulkanWindowRenderer
{
public:
    explicit VulkanRenderer(QVulkanWindow *w)
            : m_window(w), m_green(0.f)
    {}

    void initResources() override
    {
        qDebug("initResources");
        m_devFuncs = m_window->vulkanInstance()->deviceFunctions(m_window->device());
    }

    void initSwapChainResources() override { qDebug("initSwapChainResources"); }
    void releaseSwapChainResources() override { qDebug("releaseSwapChainResources"); }
    void releaseResources() override { qDebug("releaseResources"); }

    void startNextFrame() override
    {
        m_green += 0.005f;
        if (m_green > 1.0f)
            m_green = 0.0f;

        VkClearColorValue clearColor = {{ 0.0f, m_green, 0.0f, 1.0f }};
        VkClearDepthStencilValue clearDS = { 1.0f, 0 };
        VkClearValue clearValues[2];
        memset(clearValues, 0, sizeof(clearValues));
        clearValues[0].color = clearColor;
        clearValues[1].depthStencil = clearDS;

        VkRenderPassBeginInfo rpBeginInfo;
        memset(&rpBeginInfo, 0, sizeof(rpBeginInfo));
        rpBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpBeginInfo.renderPass = m_window->defaultRenderPass();
        rpBeginInfo.framebuffer = m_window->currentFramebuffer();
        const QSize sz = m_window->swapChainImageSize();
        rpBeginInfo.renderArea.extent.width = sz.width();
        rpBeginInfo.renderArea.extent.height = sz.height();
        rpBeginInfo.clearValueCount = 2;
        rpBeginInfo.pClearValues = clearValues;
        VkCommandBuffer cmdBuf = m_window->currentCommandBuffer();
        // m_devFuncs->vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // An alternative way to call vkCmdBeginRenderPass. This can also be applied to the vkCmdEndRenderPass then.
        // But we use both ways to illustrate their usages.
        PFN_vkCmdBeginRenderPass vkCmdBeginRenderPass = (PFN_vkCmdBeginRenderPass) vkGetDeviceProcAddr(m_window->device(), "vkCmdBeginRenderPass");
        vkCmdBeginRenderPass(cmdBuf, &rpBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        // Do nothing else. We will just clear to green, changing the component on
        // every invocation. This also helps verifying the rate to which the thread
        // is throttled to. (The elapsed time between startNextFrame calls should
        // typically be around 16 ms. Note that rendering is 2 frames ahead of what
        // is displayed.)

        m_devFuncs->vkCmdEndRenderPass(cmdBuf);

        m_window->frameReady();
        m_window->requestUpdate(); // render continuously, throttled by the presentation rate
    }

private:
    QVulkanWindow *m_window;
    QVulkanDeviceFunctions *m_devFuncs;
    float m_green;

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
    if (!inst.create())
        qFatal("Failed to create Vulkan instance: %d", inst.errorCode());

    VulkanWindow w;
    w.setVulkanInstance(&inst);

    w.resize(1024, 768);
    w.show();

    return app.exec();
}
