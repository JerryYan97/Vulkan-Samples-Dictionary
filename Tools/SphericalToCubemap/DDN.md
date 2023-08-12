# The tool converts equirectangular 2d HDRI to cubemap HDRI

## Description

The standard input for an environment map in a game engine is an equirectangular 2d hdri. However, in the produced game, it has to be a cubemap for efficient sampling and IBL (Image based lighting). In order to put the feature into the game engine reliably, I implement the standalone tool here to transform a 2d image to the corresponding cubemap. It will be an educational reference point and experiment ground. The output results can be debugged by using the written vulkan examples easily.

### Algorithem

The algorithem would follow [3D space vector to cubemap](http://paulbourke.net/panorama/cubemaps/cubemapinfo.pdf).

### Vulkan Implementation

As for implementation details, we will make use of the multiview feature since it is designed for this kind of seneriaos.

First of all, we will use the graphics pipeline with one vulkan draw but 6 mutiviews' draws. The mutiview ids would simply be [0 - 5]. As for geometry, we just draw the quad that can cover the whole screen. For the pixel shader, we generate the ray from the world position of each pixel and and use the ray to sample the spherical hdri and put the sampled value into the corresponding cubemap place.

One thing that we need to pay attention to is that rays generated are in the plane space. Thus, we have to put the rotation matrices into a uniform buffer and pass that to the fragment shader stage, so we can transform the ray.

## Reference

* [3D space vector to cubemap](http://paulbourke.net/panorama/cubemaps/cubemapinfo.pdf)

* [2D corrdinates to 3D space vectors](http://paulbourke.net/panorama/cubemaps/#3)