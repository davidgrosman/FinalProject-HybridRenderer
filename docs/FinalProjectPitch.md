# University of Pennsylvania, CIS 565: GPU Programming and Architecture.
Final Project: Hybrid Ray-Raster Renderer in Vulkan
===============

## Team members
- **Names:** Trung Le and David Grosman.
- **Tested on:** 
 * Microsoft Windows 10 Home, i7-4790 CPU @ 3.60GHz 12GB, GTX 980 Ti (Person desktop).
 * Microsoft Windows  7 Professional, i7-5600U @ 2.6GHz, 256GB, GeForce 840M (Personal laptop).

## Overview
Deferred rendering has gained major popularity over the last few years in the development of video games. Some of its advantages are the fact that it reduces the rendering algorithm complexity from O(numLights*numObjects) to O(numLights+numObjects) by rendering a
scene in two passes: It first renders the scene geometry into a G-Buffer and then, uses that G-Buffer to calculate the scene lighting in a second pass. It is also easier to maintain since the Lighting stage is entirely disconnected from the Geometry stage. Unfortunately, deferred rendering is not the best solution for all cases:
 1. The G-Buffer can require a lot of memory especially at higher screen resolutions. Furthermore, material characteristics are stored in diverse render targets. The more diverse the materials, the more target buffers are needed which might hit A hardware limit (Thus, the introduction of light pre-pass renderer).
 2. It does not handle hardware anti-aliasing (ie.: depth, position attributes cannot be blended).
 3. There is no direct support for translucent objects (The G-Buffer can only retain information for one 'layer').

To mitigate these issues, we want to implement a hybrid raytracer-rasterizer[1] program in Vulkan[2] to accommodate rendering transparent objects for games. The basic concept is to first use rasterization through a deferred-renderer to capture all objects in our scene and then apply a full-screen ray tracing pass by tracing rays initialized from the G-buffer information (ie.: position, normal, albedo of the closest layer from the camera) to render transparency and correct shadows. A ray-raster hybrid can also offer better anti-aliasing than the traditional shadow mapping of a pure rasterizer. There would also not be any need for composing the G-Buffer of many render targets.

The ray tracing component relies heavily on the compute power of the GPU, so we decided to use the Vulkan API since it supports access to both the graphics queue and the compute queue. In comparison to OpenGL, Vulkan doesn’t offer a drastic improvement in performance if the pipeline follows the rasterize-then-raytrace pattern. However, rasterization and ray tracing can be done asynchronously because Vulkan (and DirectX12) supports async compute[3]. This is a new feature that explicit graphics API offer over the traditional OpenGL style API that puts modern game engines in the more effective multithreading pattern. Games such as DOOM[4] and Rise of the Tomb Raider[5] take advantage of this async compute power to optimize for performance on threads that are idle and ready to submit compute command buffers. Note that not many hardware in the market support async-compute natively and we aren't yet sure where/how we might use it for our application.

To summarize, our final project is two-fold: 
 1. Take advantage of a deferred-renderer (by focusing on the 'closest' layer only) while mitigating its issues with transparent objects, anti-aliasing and large memory requirements.
 2. Taking advantage of the Vulkan’s Compute/Graphics queues to optimize for performance.

As the end result, we would like to demonstrate both of the above bullet points to be feasible and analyze the performance with async compute on versus off (or Ray-Raster vs Deferred-Renderer Only) to validate our hypothesis of its advantage.

Part of the goal for this project is also to learn about explicit graphics API for rendering.

![A](TLVulkanRenderer/images/DefRayTracing.png)
_Image taken from [Practical techniques for ray-tracing in games](http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php)_
 
 
### Plan
 1. A basic Vulkan deferred renderer with glTF mesh support.
 2. Ray tracing for transparent objects using compute shaders.
 3. Physically accurate shadows and better support for anti-aliasing via ray tracing.
 4. An acceleration data structure, BVH or kd-tree, for ray tracing. (Async Compute)
 5. Async compute for multithreading.
 
## Credits
* [Practical techniques for ray-tracing in games](http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php)
* [GDCVault14: Practical techniques for ray-tracing in games] (http://www.gdcvault.com/play/1020688/Practical-Techniques-for-Ray-Tracing)
* [Vulkan, Industry Forged](https://www.khronos.org/vulkan/)
* [Asynchronous Compute in DX12 & Vulkan: Dispelling Myths & Misconceptions Concurrently](https://youtu.be/XOGIDMJThto)
* [Doom benchmarks return: Vulkan vs. OpenGL](http://www.pcgamer.com/doom-benchmarks-return-vulkan-vs-opengl/2/)
* [Rise of the Tomb Raider async compute update boosts performance on AMD hardware](https://www.extremetech.com/gaming/231481-rise-of-the-tomb-raider-async-compute-update-improves-performance-on-amd-hardware-flat-on-maxwell)
* [Imagination PowerVR 6XT GR6500 mobile GPU - Ray Tracing demos vs Nvidia Geforce GTX 980 Ti](https://youtu.be/ND96G9UZxxA)

 
