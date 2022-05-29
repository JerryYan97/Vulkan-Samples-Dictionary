1-16 InitVMA
=====================================================

**Keywords:** ``VkInstance``, ``Debug Layer``, ``VkPhysicalDevice``, ``VMA``, ``VkBuffer``, ``VkMemory``.

**Link to Repo:** `1-16 InitVMA <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/1-16_InitVkMemAlloc>`_.

Introduction
-------------
This is a simple example used to illustrate how to use the Vulkan Memory Allocator. As you can see, it can significantly simpilfy the process of
creating a buffer, a memory and linking them together. Speficifally, if you have a large enough texture that is needed to be loaded into the gpu memory,
you need to apply for several blocks of memory and VMA can help to eliminate these chores.

An Example Output
-----------------
``Device name:AMD Radeon RX 6800 XT``

Reference
-----------------
`Vulkan Memory Allocator doc <https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/>`_.