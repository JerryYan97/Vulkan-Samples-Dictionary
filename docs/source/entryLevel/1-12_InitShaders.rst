1-12 InitShaders
=====================================================

**Keywords:** ``VkInstance``, ``Debug Layer``, ``VkPhysicalDevice``, ``VkDevice``, ``VkShaderModule``, ``Shader Compile``, ``glsl``.

**Link to Repo:** `1-12 InitShaders <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/1-12_InitShaders>`_.

Introduction
-------------
This is a simple example used to illustrate how to create a shader module from a spv shader script. It would print the first physical device's device name.
Even though the shader compilation would be handled by CMake, it would be good to know how to do it by using the command line.

How to compile shaders
-----------------------
As long as you have installed Vulkan, you would have the compiler already. Normally, it should be in a directory specified by your ``PATH`` environment.
And you can use the example code to generate ``spv`` shader from glsl by using command below. For more usage, you can execute this tool with `-h`.

``glslc.exe init_shaders.vert -o init_shaders.vert.spv``

An Example Output
-----------------
``Device name:AMD Radeon RX 6800 XT``
