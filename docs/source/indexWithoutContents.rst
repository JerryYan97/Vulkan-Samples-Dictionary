Welcome to Vulkan Samples Dictionary's documentation!
=====================================================

Purpose
==================

This project aims to provide people who use Vulkan an efficient reference place, which I believe it can smooth the 
learning curve and reduce the costs of using Vulkan API to build useful compute/graphics products.

Currently, you can find some good resources to learn Vulkan. But no one can be used as an efficient reference and
we are introducing them here (:ref:`my-reference-label`) to help you understand why this project maybe more helpful than them for reference as 
well as in case you don't know these places.

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
