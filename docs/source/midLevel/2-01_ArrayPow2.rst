2-01 ArrayPow2
=====================================================

**Keywords:** ``Compute Shader``, ``VkFence``.

**Link to Repo:** `2-01 ArrayPow2 <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/2-01_ArrayPow2>`_.

Introduction
-------------
This example is used to illustrate how to use the compute functionalities and fence in Vulkan. It creates a buffer as both input and output
data storage. In the compute shader, each thread just multiplies each entries by itself and put it back. To ensure that the computation
finishes when we try to access this buffer, a fence is also added. 

An Example Output
-----------------

::

    Device name:AMD Radeon RX 6800 XT
    0, 1, 4
    9, 16, 25
    36, 49, 8
