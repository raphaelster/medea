# Medea
Vulkan C++ GPU-driven renderer

Note: This is copy-pasted from a closed-source game project, and this hasn't been set up as a standalone demo yet.

Brief demo video:
https://youtu.be/1wPAgU4xABE

Notable technical details:
- Clustered forward renderer
- GPU-driven renderer
- Reasonably modern lighting system
  - Arbitrary number of in-world lights, culled to 2048 lights renderable at once in world
  - Shadowmap LOD; uses an 8k by 8k shadow atlas, lights use sub-regions of texture based on quality heuristic
- Volumetric lighting
  - Nonstandard approach: phase * in-scatter light accumulated into 3d texture, then final vol light lookup 3d texture raymarch uses Z-based multisampling of the fog material function
- Transparent data types between C++ and shaders
  - Basically, can set a C++ struct as a vertex data type, and the same struct is defined to be used in vertex shader and fragment shader code (with the same field names!)
  - Works via boost::pfr for reflection on struct fields, and template metaprogramming to generate the full code for shaders given a Medea-specific vertex/fragment shader file
- PBR and simple tonemapping (hejl-burgess)
- Decent performance
  - 80FPS on benchmark scene with 1024 lights, and 30FPS on 2048 lights (on AMD RX 6750 XT, performance is largely GPU bound)

