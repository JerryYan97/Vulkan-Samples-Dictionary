# The tool converts equirectangular 2d HDRI to cubemap HDRI

## Description

The standard input for an environment map in a game engine is an equirectangular 2d hdri. However, in the produced game, it has to be a cubemap for efficient sampling and IBL (Image based lighting). In order to put the feature into the game engine reliably, I implement the standalone tool here to transform a 2d image to the corresponding cubemap. It will be an educational reference point and experiment ground. The output results can be debugged by using the written vulkan examples easily.

### Algorithem

The algorithem would follow [3D space vector to cubemap](http://paulbourke.net/panorama/cubemaps/cubemapinfo.pdf).

### Vulkan Implementation

As for implementation details, we will make use of the multiview feature since it is designed for this kind of seneriaos.

First of all, we will use the graphics pipeline with one vulkan draw but 6 mutiviews' draws. The mutiview ids would simply be [0 - 5]. As for geometry, we just draw the quad that can cover the whole screen. For the pixel shader, we generate the ray from the world position of each pixel and and use the ray to sample the spherical hdri and put the sampled value into the corresponding cubemap place.

One thing that we need to pay attention to is that rays generated are in the plane space. Thus, we have to put the rotation matrices into a uniform buffer and pass that to the fragment shader stage, so we can transform the ray.

Besides, according to Vulkan Spec:

> The view index is provided in the ViewIndex shader input variable, and color, depth/stencil, and input attachments all read/write the layer of the framebuffer corresponding to the view index.

> typedef struct VkRenderingInfo: viewMask is the view mask indicating the indices of attachment layers that will be rendered when it is not 0.

Therefore, the driver has already helped us link the right color attachment layer to ps shader output. The only thing that we need to note is that our projection matrices' sequence has to obey the [cubemap tradition](https://registry.khronos.org/vulkan/specs/1.3/html/chap16.html#_cube_map_face_selection).

We may need a normalize or not. It's still hard to tell from the spec so experiments are necessary. The first thing to check is that whether the input texture has values that are bigger than 1.0. The next thing to check is that whether the output render image has values that are bigger than 1.0.

I guess we don't need to normalize since why the sfloat exists?


## Reference

* [3D space vector to cubemap](http://paulbourke.net/panorama/cubemaps/cubemapinfo.pdf)

* [2D corrdinates to 3D space vectors](http://paulbourke.net/panorama/cubemaps/#3)
  
* [Vulkan cubemap tradition](https://registry.khronos.org/vulkan/specs/1.3/html/chap16.html#_cube_map_face_selection)