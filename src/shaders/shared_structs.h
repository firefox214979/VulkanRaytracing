
#ifndef SHARED_STRUCTS
#define SHARED_STRUCTS

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

// Information of a obj model when referenced in a shader
struct ObjDesc
{
  int      txtOffset;             // Texture index offset in the array of textures
  uint64_t vertexAddress;         // Address of the Vertex buffer
  uint64_t indexAddress;          // Address of the index buffer
  uint64_t materialAddress;       // Address of the material buffer
  uint64_t materialIndexAddress;  // Address of the triangle material index buffer
};

// An emitter
struct Emitter
{
  uint index; // the triangles index in the model¡¯s list of triangles
  vec3 v0, v1, v2; // Vertices of light emitting triangle
  vec3 emission; // Its emission
  vec3 normal; // its normal
  vec3 point;
  float area; // Its triangle area
};

// Uniform buffer set at each frame
struct MatrixUniforms
{
  mat4 viewProj;      // Camera view * projection
  mat4 priorViewProj; // Camera view * projection
  mat4 viewInverse;   // Camera inverse view matrix
  mat4 projInverse;   // Camera inverse projection matrix
};

#ifdef __cplusplus
#define ALIGNAS(N) alignas(N)
#else
#define ALIGNAS(N)
#endif

// For structures used by both C++ and GLSL, the byte alignment
// differs between the two languages.  Ints, uints, and floats align
// nicely, but bool's do not.
//  Use ALIGNAS(4) to align bools.
//  Use ALIGNAS(4) or not for ints, uints, and floats as they naturally align to 4 byte boundaries.
//  Use ALIGNAS(16) for vec3 and vec4, and probably mat4 (untested).
//
// Do a sanity check by creating a sentinel value, say
// "alignmentTest" as the last variable in the PushConstantRay
// structure.  In the C++ program, set this to a know value.  In
// raytrace.gen, test for that value and signal failure if not found.
// For example:
// if (pcRay.alignmentTest != 1234) {
//    imageStore(colCurr,  ivec2(gl_LaunchIDEXT.xy), vec4(1,0,0,0)); return; }

// Also, If your alignment is incorrect, the VULKAN validation layer
// may now produce an error message.


// Push constant structure for the raster
struct PushConstantRaster
{
    ALIGNAS(16) vec3  scLightPos;
    ALIGNAS(16) vec3  scLightInt;
    ALIGNAS(16) vec3  scLightAmb;
    ALIGNAS(16) mat4  modelMatrix;  // matrix of the instance
    ALIGNAS(4) uint  objIndex;     // index of instance
};



// Push constant structure for the ray tracer
struct PushConstantRay
{
    // @@ Raycasting:	Declare 3 temporary light values.  
       /*ALIGNAS(16) vec3 scLightPos;
       ALIGNAS(16) vec3 scLightInt;
       ALIGNAS(16) vec3 scLightAmb;*/
    // @@ Pathtracing:	Remove those 3 temporary light values. 
    ALIGNAS(4) int frameSeed;
    ALIGNAS(4) float rr;
    ALIGNAS(4) int depth;
    ALIGNAS(4) bool explicitLight;
    // @@ History:	 ...
    // @@ Denoise:	 ...
    ALIGNAS(4) bool clear;  // Tell the ray generation shader to start accumulation from scratch
    ALIGNAS(4) float exposure;
    ALIGNAS(4) int alignmentTest; // Set to a known value in C++;  Test in the shader!
};

struct Vertex  // Created by readModel; used in shaders
{
  vec3 pos;
  vec3 nrm;
  vec2 texCoord;
};

struct Material  // Created by readModel; used in shaders
{
  vec3  diffuse;
  vec3  specular;
  vec3  emission;
  float shininess;
  int   textureId;
};


// Push constant structure for the ray tracer
struct PushConstantDenoise
{
  float normFactor;
  float depthFactor;
  int  stepwidth;  
};

struct RayPayload
{
    uint seed;
    bool hit;           // Does the ray intersect anything or not?
    vec3 hitPos;	// The world coordinates of the hit point.      
    float hitDist;
    int instanceIndex;  // Index of the object instance hit (we have only one, so =0)
    int primitiveIndex; // Index of the hit triangle primitive within object
    vec3 bc;            // Barycentric coordinates of the hit point within triangle
};

#endif
