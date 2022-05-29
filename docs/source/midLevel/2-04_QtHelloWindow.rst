2-04 QtHelloWindow
=====================================================

**Keywords:** ``Qt``, ``QVulkanWindow``, ``QVulkanWindowRenderer``.

**Link to Repo:** `2-04 QtHelloWindow <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/2-04_QtHelloWindow/ReleaseSample>`_.

Introduction
-------------
This example is used to illustrate how to combine Qt and Vulkan together. Qt has its own abstration of ``VkInstance``, ``Frame Buffer`` and ``Swapchain``, which
can significantly simpilfy the development process by comparing to ``glfw``. However, it uses dynamic load of Vulkan's functions and it has its own preprocessor
and development environment. So, we need to take care of them if we don't use the Qt Creator.

An Example Output
-----------------
.. image:: ../imgs/qtHelloWindow.gif

Reference
-----------
`Qt HellowWindow page <https://doc.qt.io/qt-5/qtgui-hellovulkanwindow-example.html>`_.
`Qt Vulkan examples <https://code.qt.io/cgit/qt/qtbase.git/tree/examples/vulkan?h=5.15>`_.