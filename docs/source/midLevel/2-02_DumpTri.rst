2-02 DumpTri
=====================================================

**Keywords:** ``Triangle``.

**Link to Repo:** `2-02 DumpTri <https://github.com/JerryYan97/Vulkan-Samples-Dictionary/tree/master/Samples/2-02_DumpTri>`_.

Introduction
-------------
This example is used to illustrate how to draw a triangle as simple as possible. It doesn't have a vertex buffer. Instead, the data
is written in the vertex shader and each threads would get the corresponding vertex data by using its own id. 

An Example Output
-----------------
.. image:: ../imgs/dumpTri.png
