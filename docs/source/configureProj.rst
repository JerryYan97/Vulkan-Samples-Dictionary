.. Vulkan Samples Dictionary documentation master file, created by
   sphinx-quickstart on Mon Feb 14 21:02:47 2022.
   You can adapt this file completely to your liking, but it should at least
   contain the root `toctree` directive.

Get Started
=========================

All examples use their own ``CMakeList.txt`` file and there is a ``CMakeUtil.cmake`` file under the CMakeFuncSupport providing some checking supports.
For myself, I use CLion to build and execute each examples. But, I believe you can also use other ways to build and run these examples. E.g. Use Cmake to 
generate VS studio project file and then use VS studio to run the project. Or, using Cmake to generate ninja files and then build and run an example by using ninja.
In general, we would assume that you have already gotten a viable C++ development environment and this project wouldn't introduce how to install VS/CLion/CMake/Ninja...

Currently, we only test it on the Windows platform and we cannot guarantee whether it would work on other platforms.

Clone down the project and setup Vulkan
----------------------------------------

**Clone down this repo by using this command:**

``git clone --recurse-submodules https://github.com/JerryYan97/Vulkan-Samples-Dictionary.git``

**Download vulkan and set environment variable:**

You can download Vulkan `here <https://vulkan.lunarg.com/>`_. Besides, you also need to set ``VULKAN_SDK`` System variable to the path of your Vulkan (E.g. C:\\VulkanSDK\\1.2.198.0) in your ``Environment Variables`` panel.
This environment variable will be used to find your Vulkan.

Bulid an example with CMake
----------------------------------------

**Build an example by using CLion:**

You can find ``ReleaseSample`` or ``DebugSample`` folders under an example folder. By using CLion you can just open these folders as a project and run it as usual. 

**Build an example by using CMake and Visual Studio:**

1. Using CMake to generate the Visual Studio project file by using the following command (Alternatively, you can also set corresponding places in CMake-Gui ). E.g.

``cmake -G "Visual Studio 16 2019" -A x64 ..``

You may need to change the generator and architecture to accomodate your development environment. 

2. Set the example as the startup project.

3. Run the example.

**Build an example by using ninja**

1. Using CMake to generate the Ninja building files. E.g. 

``cmake -H. -GNinja ..``

2. Run Ninja to build and compile. This would generate an executive file. E.g.

``ninja``

3. Execuate the executive file. E.g.

``InitDevice.exe``

Configurations for examples using additional dependencies (Like Qt)
--------------------------------------------------------------------

**Additional configuring for examples using Qt:**

Some of the examples using Qt. So, if you are interested in these examples, you need to configure environment as following:

1. Download MinGW Qt environment.

2. Set ``QT_CMAKE_ENV_PATH`` to your Qt development environment's cmake folder. E.g.

``C:\Qt\6.2.4\mingw_64\lib\cmake``

3. Build the example by using ways introduced above.

In addition, I try to avoid using Qt Creator as my IDE for Qt development because I want to make my development process consistent under
CLion, which I think it has a clear process from buliding a project to running an application and you can control it.

Due to Qt's cross-platform nature, we cannot use MSVC. However, if you are a fan of VS, here is a `tutorial <https://devblogs.microsoft.com/cppblog/using-mingw-and-cygwin-with-visual-cpp-and-open-folder/>`_ 
that can let your VS use MinGW environment downloaded.

It is also possible to use Ninja and MinGW64 to build and compile the project. But, specific steps that you need to follow is under exploration. Currently, CLion with its default MinGW toolchain
is the most stable way to build the Qt projects.