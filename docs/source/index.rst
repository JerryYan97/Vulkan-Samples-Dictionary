.. Vulkan Samples Dictionary documentation master file, created by
   sphinx-quickstart on Mon Feb 14 21:02:47 2022.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Welcome to Vulkan Samples Dictionary's documentation!
=====================================================

.. contents:: Table of Contents
   :depth: 3
   :local:
   :backlinks: none

Purpose
==================

This project aims to provide people who use Vulkan an efficient reference place, which I believe it can smooth the 
learning curve and reduce the costs of using Vulkan API to build useful compute/graphics products.

Currently, you can find some good resources to learn Vulkan. But no one can be used as an efficient reference and
we are introducing them here (:ref:`my-reference-label`) to help you understand why this project maybe more helpful than them for reference as 
well as in case you don't know these places.

.. toctree::
   :maxdepth: 3
   :caption: Contents

   configureProj.rst
   ./entryLevelIndex.rst
   ./midLevelIndex.rst
   ./upperLevelIndex.rst

Design principles 
==================

* **Easy to build**:
   Each samples only has its own CMakeList.txt file and one source file with several shader files.

* **Small topics**:
   Entry level samples focus on correctly using API commands (Within 500 lines);

   Mid level samples focus on small use cases like triangle rendering (Within 2000 lines);

   Upper level samples focus on relatively large and common topics like deferred rendering (Within 5000 lines);

* **Easy to find**:
   You can search keywords on this site.

* **Easy to read**:
   Code would be extensively commented and you can find more info on their own pages. Besides, we also separate 
   debug mode code and release mode code of each samples to make your reading more enjoyable. 

* **Convenient to copy and paste**:
   Abstraction of the code would be kept as less as possible. Although it doesn't fulfill software engineering principles,
   it is helpful for the learning process like copying the code to your own projects and run them.

Configuring this project
=========================

All examples use their own ``CMakeList.txt`` file and there is a ``CMakeUtil.cmake`` file under the CMakeFuncSupport providing some checking supports.
For myself, I use CLion to build and execute each examples. But, I believe you can also use other ways to build and run these examples. E.g. Use Cmake to 
generate VS studio project file and then use VS studio to run the project. Or, using Cmake to generate ninja files and then build and run an example by using ninja.
In general, we would assume that you have already gotten a viable C++ development environment and this project wouldn't introduce how to install VS/CLion/CMake/Ninja...

Currently, we only test it on the Windows platform and we cannot guarantee whether it would work on other platforms.

Clone down this repo by using this command:
-------------------------------------------

``git clone --recurse-submodules https://github.com/JerryYan97/Vulkan-Samples-Dictionary.git``

Download vulkan and set environment variable:
----------------------------------------------

You can download Vulkan `here <https://vulkan.lunarg.com/>`_. Besides, you also need to set ``VULKAN_SDK`` System variable to the path of your Vulkan (E.g. C:\\VulkanSDK\\1.2.198.0) in your ``Environment Variables`` panel.
This environment variable will be used to find your Vulkan.

Build an example by using CLion:
----------------------------------------------

You can find ``ReleaseSample`` or ``DebugSample`` folders under an example folder. By using CLion you can just open these folders as a project and run it as usual. 

Build an example by using CMake and Visual Studio:
--------------------------------------------------

1. Using CMake to generate the Visual Studio project file by using the following command (Alternatively, you can also set corresponding places in CMake-Gui ). E.g.

``cmake -G "Visual Studio 16 2019" -A x64 ..``

You may need to change the generator and architecture to accomodate your development environment. 

2. Set the example as the startup project.

3. Run the example.

Build an example by using ninja
----------------------------------------------

1. Using CMake to generate the Ninja building files. E.g. 

``cmake -H. -GNinja ..``

2. Run Ninja to build and compile. This would generate an executive file. E.g.

``ninja``

3. Execuate the executive file. E.g.

``InitDevice.exe``

Additional configuring for examples using Qt:
----------------------------------------------

Some of the examples using Qt. So, if you are interested in these examples, you need to configure environment as following:

1. Download MinGW Qt environment.

2. Set ``QT_CMAKE_ENV_PATH`` to your Qt development environment's cmake folder. E.g.

``C:\Qt\6.2.4\mingw_64\lib\cmake``

3. Build the example by using ways introduced above.

.. _my-reference-label:

Current Vulkan resources
============================

`Khronos Group Vulkan-Samples`_ : I believe this is the most comprehensive samples repository on the internet. But, it
is hard for reference and to build the code to play with. For instance, if you are interested in making a Vulkan App
using compute shader, you firstly need to spend some time digging out the titles that maybe suitable. Then, you may get
stuck at building it because it relies on too many 3rd party repos. Finally, you need to spend lots of time
understanding its architecture first and then learn what you really care about: Compute Shader.

`LunarG Vulkan Samples`_ : This is the repository that this project wants to inherit and improves on.

`Vulkan Tutorial`_ : Personaly, this is the best tutorial for beginners and I read it
for three times. But, I can hardly build any Vulkan API from scrach by just relying on this.

`Awesome Vulkan`_ : This is a place that documents almost everything about Vulkan, which is a place that you should
know if you play with Vulkan.

.. _Khronos Group Vulkan-Samples: https://github.com/KhronosGroup/Vulkan-Samples

.. _Vulkan Tutorial: https://vulkan-tutorial.com/

.. _LunarG Vulkan Samples: https://github.com/LunarG/VulkanSamples

.. _Awesome Vulkan: https://github.com/vinjn/awesome-vulkan
