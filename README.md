# Final Project: Hybrid Ray-Raster Renderer in Vulkan
===============

## Team members
- **Names:** Trung Le and David Grosman.
- **Tested on:** 
 * Microsoft Windows 10 Home, i7-4790 CPU @ 3.60GHz 12GB, GTX 980 Ti (Person desktop).
 * Microsoft Windows  7 Professional, i7-5600U @ 2.6GHz, 256GB, GeForce 840M (Personal laptop).

## Overview

[insert gif here]

Deferred rendering has gained major popularity in real-time rendering. Some of its advantages are the fact that it reduces the rendering algorithm complexity from `O(numLights*numObjects)` to `O(numLights + numObjects)` by rendering a scene in two passes: It first renders the scene geometry into a G-Buffer and then, uses that G-Buffer to calculate the scene lighting in a second pass. It is also easier to maintain since the Lighting stage is entirely disconnected from the Geometry stage. Unfortunately, deferred rendering is not the best solution for all cases:

1. The G-Buffer can require a lot of memory especially at higher screen resolutions. Furthermore, material characteristics are stored in diverse render targets. The more diverse the materials, the more target buffers are needed which might hit a hardware limit (Thus, the introduction of light pre-pass renderer).
2. There is no direct support for refractive and reflective materials because the G-Buffer can only retain information for one 'layer'.
3. Shadows are typicaly computed using shadow mapping. This can introduce inaccurate shadows due to aliasing from low resolution shadow maps. Cascaded shadow mapping can mitigate these issues, but requires more memory for each cascaded level.
 
To combat with the above issues, we implemented a [hybrid raytracer-rasterizer][Practical techniques for ray-tracing in games] program using the explicit graphics API [Vulkan][Vulkan] to accommodate rendering transparent objects for games. This techique is currently being used by the [PowerVR Raytracing GPU family](https://youtu.be/rjvaxcM4g7c) for real time application. Our version is more light-weight and handles lower geometry details. 

The basic concept is to first use rasterization through a deferred-renderer to capture all objects in our scene and then apply a full-screen ray tracing pass by tracing rays initialized from the G-buffer information. 

![](http://blog.imgtec.com/wp-content/uploads/2014/03/08_Ray-tracing-in-games_Hybrid-rendering-in-a-game-engine.png)
_Image taken from [Practical techniques for ray-tracing in games](http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php)_

There are only 3 layers needed in the G-buffer: position, normal, material ID to detect the first bounce. Differently from a traditional raytracing, in our version, the first ray-triangle intersection has been precomputed by the G-buffer pass. For comparison, with a 800x800 resolution image, with a 1000 triangles scene, assuming we cast one ray per pixel, then we'll have for first bounce:
```
800 x 800 x 1000 ray-triangle intersections = 640,000,000 ray-triangle intersections
```
With deferred shading aid, we basically eliminate this first bounce cost and transfer that onto the rasterization cost done inside the fragment shader in hardware.

 
# Our pipeline

[insert image here]

 
# Debugging tools

1. Deferred shading layers
2. BVH visualization
3. Feature user input to toggle
 
# Performance Analysis

_Our performance analysis was done using the following test scenes_

1. Far scene

[insert image here] 

2. Close scene

[insert image here] 


# Optimization

### BVH

[insert image here]

### Ray-triangle intersection

We used [Muller's fast triangle intersection test](https://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf). This skips computing the plane's equation.

### Shadows computation

### G-buffer packing

- Material ID is passed into the shader first as the fourth element of indices
- Instead of having an additional G-buffer for material ID, using the w element of position inside the position layer
- Passing in the triangles into the raytracing compute shaders as triangle soup.
- Uses uint16_t indices

### Early termination
- If geometry's normal == vec3(0), don't raytrace

# Gallery

[insert images here]

# Build instruction

Our project uses CMake to build. Requires a Vulkan-capable graphics card, Visual Studio 2013, target platform x64.

# References
* [Practical techniques for ray-tracing in games][Practical techniques for ray-tracing in games]
* [GDCVault14: Practical techniques for ray-tracing in games](http://www.gdcvault.com/play/1020688/Practical-Techniques-for-Ray-Tracing)
* [Vulkan, Industry Forged] [Vulkan] 
* [Asynchronous Compute in DX12 & Vulkan: Dispelling Myths & Misconceptions Concurrently](https://youtu.be/XOGIDMJThto)
* [Doom benchmarks return: Vulkan vs. OpenGL](http://www.pcgamer.com/doom-benchmarks-return-vulkan-vs-opengl/2/)
* [Rise of the Tomb Raider async compute update boosts performance on AMD hardware](https://www.extremetech.com/gaming/231481-rise-of-the-tomb-raider-async-compute-update-improves-performance-on-amd-hardware-flat-on-maxwell)
* [Imagination PowerVR 6XT GR6500 mobile GPU - Ray Tracing demos vs Nvidia Geforce GTX 980 Ti](https://youtu.be/ND96G9UZxxA)



[Practical techniques for ray-tracing in games]: http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php

[Vulkan]: https://www.khronos.org/vulkan/