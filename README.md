# Final Project: Hybrid Ray-Raster Renderer in Vulkan
===============

![](/docs/images/bear_boxes.gif)


# Team members
- **Names:** Trung Le ([website](http://www.trungtuanle.com/)) and David Grosman.
- **Tested on:** 
 * Microsoft Windows 10 Home, i7-4790 CPU @ 3.60GHz 12GB, GTX 980 Ti (Desktop).
 * Microsoft Windows  7 Professional, i7-5600U @ 2.6GHz, 256GB, GeForce 840M (Laptop).

### Video demo (click on image)

<a href="https://www.youtube.com/embed/LI-4krLiWOo" target="_blank"><img src="http://img.youtube.com/vi/LI-4krLiWOo/0.jpg" 
alt="Hybrid ray-raster" width="560" height="415" border="1" /></a>

# Overview

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
With deferred shading aid, we basically eliminate entirely this first bounce cost and transfer that onto the rasterization cost done inside the fragment shader in hardware.

With raytracing, we're now able to implement shadows and and refractive or reflective material. For this project, we are not implementing PBR materials.

# Our pipeline

Our rendering pipeline is broken down into three stages: deferred, raytracing, and on screen output.

![](/docs/Hybrid_ray_raster_pipeline.png)

As you can see in the image above, the CPU first computed the BVH structure of our scene. The deferred pass is renderred off-screen, then passes the G-buffer to the compute shader to perform raytracing. The result is then taken from the image texture from compute shader and rendered on-screen.

# About Vulkan

Vulkan is a great graphics API. It is quite verbose, required careful planning and code structuring since everything is recorded before it runs. This project allows us some working experience with building a Vulkan rendering framework.

Our application's overview from the top-down:

![](/docs/FinalProject-HybridRayRaster_ComponentDiagram.png)

# Optimization

### BVH

![](/docs/images/bvh.gif)

### Ray-triangle intersection

We used [Muller's fast triangle intersection test](https://www.cs.virginia.edu/~gfx/Courses/2003/ImageSynthesis/papers/Acceleration/Fast%20MinimumStorage%20RayTriangle%20Intersection.pdf). This skips computing the plane's equation.

### Shadows computation

We only computed shadows for ground-level surfaces to avoid unnecessarily checking surfaces that are high up and unlikely to be in shadows for our test scene. We think this is a reasonable approach, since it's a common practice in game design.

### G-buffer packing

- Material ID is passed into the shader first as the fourth element of indices
- Instead of having an additional G-buffer for material ID, using the w element of position inside the position layer. This value was normalized by the material count of the entire scene.

### Small optimizations
- Pass in the triangles into the raytracing compute shaders as triangle soup. Since the scene can be quite big, the triangle soup helps reduce the amount of vertices having to pass to our shaders.
- Use uint16_t indices for binding index buffer to the pipeline.

### Early termination
- If geometry's normal == vec3(0), don't raytrace

# Debugging tools

**1. Deferred shading layers**

_This debugging tool can be enabled by hitting 'F' key_

![](/docs/images/testscene_bear_far_gbuffer.png)


**2. BVH visualization**

_This debugging tool can be enabled by hitting 'G' key_

![](/docs/images/bvh_01.png)


**3. Color by ray bounces**

_This debugging tool can be enabled by hitting 'C' key_

![](/docs/images/testscene_bear_far_color_by_ray_bounces.png)

**4. Toggleable effects**

![](/docs/images/testscene_bear_far_reflection.png)

All effects in our renderer can be toggled on and off using the following keys:

- 'F': toggle G-buffer viewing
- 'G': toggle BVH visualization
- 'B': toggle BVH 
- 'Y': toggle shadows
- 'T': toggle refraction
- 'R': toggle reflection
- 'L': add more lights
- 'C': toggle coloring by number of ray bounces

# Performance Analysis

The bottleneck of the pipeline is in the ray tracing pass. This has been traditionally quite slow. 

In order to test our performance we a) varying the number of moving lights, 2) zoomed in from the camera to cover more pixels and 3) toggling on and off shadows, refraction, and BVH optimization. Our scene configuration is:

- Image size: 800x800
- Compute shader work groups: 16x16
- 5086 triangles and 15258 vertices
- 7 materials: 3 refractive surfaces and 4 diffuse surfaces
- 3 refractive spheres and 7 diffuse objects
- Framerate is capped at 60FPS for all tests
- Tested on Microsoft Windows 10 Home, Microsoft Visual Studio 2015, target x64, i7-4790 CPU @ 3.60GHz 12GB, GTX 980 Ti

**1. Far scene.** Camera is at -30.0f Z unit away

| Scene | Analysis |
| --- | ---|
|![](/docs/images/testscene_bear_far.png)| ![](/docs/analysis/zoomed_out_bear_boxes.png)|

- Refraction: Interestingly, refraction doesn't seem to be affected. We were able to maintain the same frame rate through out.
- Shadow: this effect does get hit big. We tanked right away as soon as shadow is turned on, dropped below ~15 FPS. With the aid of BVH, we were able to gain back ~10 FPS. Acceleration data structure choice and configuration plays an important role here. This showcase that our renderer isn't quite ready for real-time application yet.

**2. Close scene.** Camera is at -15.0f unit away

| Scene | Analysis |
| --- | ---|
|![](/docs/images/testscene_bear_close.png)| ![](/docs/analysis/zoomed_in_bear_boxes.png)|

- Refraction: Up close, more weaknesses can be scene with hybrid rendering. Refraction now has a significant drop to 20FPS. However, it stays the same across varying number of light sources. This is because refraction is affected by material types, not the number of lights in the scene.
- Shadow: Again, shadow isn't in good shape. We need more improvement to increase our framerate.

A lot of the improvement lies in an efficient acceleration data structure and effecient memory access pattern. 

Similarly, we compared the same scene with our raytracing only renderer, but the framerate was consistently at 1FPS, so we decided that a hybrid renderer is in fact faster that traditional raytracing.

### Different work groups size for compute shader

We varied the dimension of the compute shader invocations, but that didn't affect performance.

# Final thoughts

The project had a great deal of software engineering in term of developing a Vulkan graphics engine and team collaboration. It was also a great opportunity for us to explore the possibility of using raytracing in real-time application. Eventhough we were not able to achieve the frame-rate compare to PowerVR raytracing demo, we gained a great deal of experience.

# Gallery

![](/docs/images/knot_shadows_02.png)

![](/docs/images/astronauts_02.png)

![](/docs/images/bear.gif)

![](/docs/images/boxes_reflective_tracedepth_3.png)

![](/docs/images/raytraced_octocat.png)

### Bloopers (yes, of course)

![](/docs/images/bloopers/hybrid_circular.png)

Refraction gone wrong.

![](/docs/images/bloopers/hybrid_messed_up_grunt.png)

This is just bad attribute stride.

![](/docs/images/bloopers/hybrid_uninitialized_colors.png)

This is when a vertex attribute weren't intialized correctly,

# Build instruction

Our project uses CMake to build. Requires a Vulkan-capable graphics card, Visual Studio 2013, target platform x64.

# Credits

We would like to thank:
* [Sascha Willems](https://github.com/SaschaWillems/Vulkan) and [Alexander Overvoorde](https://github.com/Overv/VulkanTutorial) which greatly inspired our initial code-base; Big thanks to their incredible effort to bring Vulkan to the community!
* [Morgan McGuire](http://cs.williams.edu/~morgan/) and [Gareth Morgan, Jesper Mortensen](http://www.gdcvault.com/play/1020688/Practical-Techniques-for-Ray-Tracing) for giving us the idea of implement an Hybrid Renderer.

 
# References
* [GDCVault14: Practical techniques for ray-tracing in games](http://www.gdcvault.com/play/1020688/Practical-Techniques-for-Ray-Tracing)
* [Vulkan, Industry Forged] (https://www.khronos.org/vulkan/)
* [Practical techniques for ray-tracing in games](http://www.gamasutra.com/blogs/AlexandruVoica/20140318/213148/Practical_techniques_for_ray_tracing_in_games.php)
* [Imagination PowerVR 6XT GR6500 mobile GPU - Ray Tracing demos vs Nvidia Geforce GTX 980 Ti](https://youtu.be/ND96G9UZxxA)
* [Asynchronous Compute in DX12 & Vulkan: Dispelling Myths & Misconceptions Concurrently](https://youtu.be/XOGIDMJThto)
* [Doom benchmarks return: Vulkan vs. OpenGL](http://www.pcgamer.com/doom-benchmarks-return-vulkan-vs-opengl/2/)
* [Rise of the Tomb Raider async compute update boosts performance on AMD hardware](https://www.extremetech.com/gaming/231481-rise-of-the-tomb-raider-async-compute-update-improves-performance-on-amd-hardware-flat-on-maxwell)
