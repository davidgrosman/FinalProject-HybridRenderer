Final Project: Hybrid Ray-Raster Renderer in Vulkan
===============

## Team members
- **Names:** Trung Le and David Grosman.
- **Tested on:** 
 * Microsoft Windows 10 Home, i7-4790 CPU @ 3.60GHz 12GB, GTX 980 Ti (Person desktop).
 * Microsoft Windows  7 Professional, i7-5600U @ 2.6GHz, 256GB, GeForce 840M (Personal laptop).

# Pitch

For quick summary: we are building a hybrid raytracer on top of the G-Buffer of a deferred renderer using the compute shader in Vulkan.
Please see [FinalProjectPitch.md](/docs/FinalProjectPitch.md) for full description of the project.

# Base code

Majority of the code base was started from [Vulkan Samples](https://github.com/SaschaWillems/Vulkan) by Sascha Willems and [Vulkan Tutorial](https://vulkan-tutorial.com/) by Alexander Overvoorde. [Github](https://github.com/Overv/VulkanTutorial)

# Progress update Nov 28, 2016

### Milestone 1

This milestone was the preparation step to setup a working Vulkan application that's flexible enough as a starting point for our team. We implemented CMake build and an infrastructure of Vulkan utilities to create Vulkan devices and queues, Vulkan resources (buffers and images), Vulkan command buffers, and the graphics and the compute pipeline.


### Milestone 2

Initially, our goal for this milestone was to have a working hybrid renderer for simple glTF models. However, due to Thanksgiving break, we were limited in time to meet this end. Instead, we leveraged this time to polish on the separate renderers - deferred renderer and ray tracer.

The high level overview of the application is:

```
Application -> VulkanRenderer -> [VulkanDeferredRenderer, VulkanRaytracers] -> VulkanHybridRayRaster
```

- **Deferred renderer**: The deferred renderer is built on top of Sascha Willems' sample and currently reading in a collada file format and .ktx texture. The renderer can render multiple fast moving lights. We demonstrated it here with a [small Sponza scene](https://github.com/domme/VoxelConeTracing/tree/master/bin/assets/meshes)

![](/docs/images/raytraced_sponza.gif)

- **Ray tracer with glTF**: This raytracer uses a compute shader to shoot ray into the scene. The glTF model is loaded using [tinygltfloader](https://github.com/syoyo/tinygltfloader) by [@soyoyo](https://github.com/syoyo), then converted to a list of materials, a list of indices, vertex positions, vertex normals, and vertex texture coordinates. The compute shader then parse this information to reconstruct triangles to perform ray-triangle intersection. The shaded fragment is then stored inside an image buffer to be display in the screen's framebuffer. 

![](/docs/images/raytraced_cornell.gif)

Below is a scene with the [octocat]() by [Sally Kong](https://sketchfab.com/models/cad2ffa5d8a24423ab246ee0916a7f3e) inside a Cornell box. The scene was prepared in Maya. 

![](/docs/images/raytraced_octocat.png)

_Notice the shadow from the light source onto the ground caused by using light feeler rays_


### Milestone 3

- **Merge ray-raster**: We are planning on using milestone 3 to combine ray tracing on top of the G-Buffer from the deferred renderer. This should complete the basic requirement for a hybrid ray-raster.

- **Performance analysis**: It is crucial to show the performance comparison with other types of renderer. Right now, we likely will target the CUDA path tracer and CUDA rasterizer for analysis.

### Final milestone

- **Demo scenes**: We will begin testing with interesting scenes to showcase the benefit of a hybrid ray-raster with transparency and shadows.

- **Fixing bugs**: There will likely still be polishing work that can be done here.

- **Optimization**: If time permitted, we would like to add an accelaration data structure to speed up the raytracing component. Additionally, async compute could be something worth investigating to perhaps progressively update frames when camera not moving. 

 
# Build

- Build using x64 Visual Studio 2013 on Windows with a [Vulkan](https://www.khronos.org/vulkan/) support graphics card (Most discrete GPU in the last couple years should have Vulkan support). You can also check [NVIDIA support](https://developer.nvidia.com/vulkan-driver).

- Run CMake on top level directory, using Visual Studio 2013 x64 configuration.

# Third party

 - [tinygltfloader](https://github.com/syoyo/tinygltfloader) by [@soyoyo](https://github.com/syoyo)
 - [obj2gltf](https://github.com/AnalyticalGraphicsInc/OBJ2GLTF) by [AnalyticalGraphicsInc](https://github.com/AnalyticalGraphicsInc)
 - [spdlog](https://github.com/gabime/spdlog) by [gabime](https://github.com/gabime/) (see LICENSE for details on LICENSE)

### References

  - [Vulkan Tutorial](https://vulkan-tutorial.com/) by Alexander Overvoorde. [Github](https://github.com/Overv/VulkanTutorial). 
  - WSI Tutorial by Chris Hebert
  - [Vulkan Samples](https://github.com/SaschaWillems/Vulkan) by Sascha Willems
  - [Vulkan Whitepaper](https://www.kdab.com/wp-content/uploads/stories/KDAB-whitepaper-Vulkan-2016-01-v4.pdf)
  - [Vulkan 1.0.28 - A Specification](https://www.khronos.org/registry/vulkan/specs/1.0-wsi_extensions/pdf/vkspec.pdf)
  * [Practical techniques for ray-tracing in games](http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php)
  * [GDCVault14: Practical techniques for ray-tracing in games] (http://www.gdcvault.com/play/1020688/Practical-Techniques-for-Ray-Tracing)
  * [Vulkan, Industry Forged](https://www.khronos.org/vulkan/)
  * [Asynchronous Compute in DX12 & Vulkan: Dispelling Myths & Misconceptions Concurrently](https://youtu.be/XOGIDMJThto)
  * [Doom benchmarks return: Vulkan vs. OpenGL](http://www.pcgamer.com/doom-benchmarks-return-vulkan-vs-opengl/2/)
  * [Rise of the Tomb Raider async compute update boosts performance on AMD hardware](https://www.extremetech.com/gaming/231481-rise-of-the-tomb-raider-async-compute-update-improves-performance-on-amd-hardware-flat-on-maxwell)
  * [Imagination PowerVR 6XT GR6500 mobile GPU - Ray Tracing demos vs Nvidia Geforce GTX 980 Ti](https://youtu.be/ND96G9UZxxA)

 ### Models

* [glTF Sample Models](https://github.com/KhronosGroup/glTF/blob/master/sampleModels/README.md)
* [octocat]() by [Sally Kong](https://sketchfab.com/models/cad2ffa5d8a24423ab246ee0916a7f3e). Model is converted using [obj2gltf](https://github.com/AnalyticalGraphicsInc/OBJ2GLTF).
* [wolf]() by [Rachel Hwang](https://www.linkedin.com/in/rachel-hwang-84a3b989). Model is converted using [obj2gltf](https://github.com/AnalyticalGraphicsInc/OBJ2GLTF).
* [centaur model](http://tf3dm.com/3d-model/free-base-mesh-centaur--67384.html) by [BAQStudio](http://tf3dm.com/user/baqstudio), Model is converted using [obj2gltf](https://github.com/AnalyticalGraphicsInc/OBJ2GLTF).
* Infinite, [3D Head Scan]() by Lee Perry-Smith is licensed under a Creative Commons Attribution 3.0 Unported License. Based on a work at www.triplegangers.com. This distribution was created by Morgan McGuire and Guedis Cardenas http://graphics.cs.williams.edu/data/. See [LICENSE](/gltfs/head/Infinite-Scan_License.txt). Model is converted using [obj2gltf](https://github.com/AnalyticalGraphicsInc/OBJ2GLTF).
* [Crytek Sponza](http://graphics.cs.williams.edu/data/meshes.xml#2) modified by Morgan McGuire. The model is converted from .obj format to .dae using Maya for loading in the scene.
* [Small Sponza](https://github.com/domme/VoxelConeTracing/tree/master/bin/assets/meshes) at [VoxelConeTracing](https://github.com/domme/VoxelConeTracing)

