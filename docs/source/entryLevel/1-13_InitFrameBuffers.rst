1-13 InitFrameBuffers
=====================================================

**Keywords:** ``VkInstance``, ``Debug Layer``, ``VkPhysicalDevice``, ``VkDevice``, ``VkImage``, ``Render Pass``, ``Frame Buffer``.

**Link to Repo:** `1-13 InitFrameBuffers <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/1-13_InitFrameBuffers>`_.

Introduction
-------------
This is a simple example used to illustrate how to create a ``Render Pass``, ``Color Attachment`` and ``Depth Stencil Attachment``.
Then, the example would connect attachments to the ``Render Pass``.Color image and depth-stencil image are created to back the ``Frame Buffer``. 
Finally, they would be connected together to create the ``Frame Buffer``. It would print the first physical device's device name.

An Example Output
-----------------
``Device name:AMD Radeon RX 6800 XT``