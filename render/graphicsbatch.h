#ifndef MEINEKRAFT_GRAPHICSBATCH_H
#define MEINEKRAFT_GRAPHICSBATCH_H

#include <map>
#include <SDL_log.h>
#include <iostream>
#include <cstring>

#include "primitives.h"
#include "shader.h"
#include "debug_opengl.h"

#ifdef _WIN32
#include <glew.h>
#include <SDL_image.h>
#include <SDL_opengl.h>
#else
#include <GL/glew.h>
#include <SDL2/SDL_image.h>
#include "SDL2/SDL_opengl.h"
#endif 

class RenderComponent;

/**
* Contains the rendering context for a given set of geometry data
* RenderComponents are batched in to a GraphicsBatch based on this geometry data & shader config.
*/
// TODO: Docs
class GraphicsBatch {
public:
  explicit GraphicsBatch(ID mesh_id): mesh_id(mesh_id), components{}, mesh{}, id(0), diffuse_textures{}, layer_idxs{},
    diffuse_textures_capacity(3), diffuse_textures_count(0), specular_textures_count(0), specular_textures_capacity(3) {};
  
  // FIXME: Handle size changes for texture buffer(s)
  
  void init_buffer(uint32_t* gl_buffer, uint32_t gl_buffer_type, uint32_t gl_texture_unit, uint32_t buffer_size) {
    // FIXME: Assumes OpenGL texture type
    glGenTextures(1, gl_buffer); // FIXME: OpenGL error 1280 (0x500) here in this function
    glActiveTexture(GL_TEXTURE0 + gl_texture_unit);
    glBindTexture(gl_buffer_type, *gl_buffer);
    // FIXME: Texture information is assumed here
    auto layers_faces = 1 * buffer_size;
    glTexStorage3D(gl_buffer_type, 1, GL_RGB8, 2048, 2048, layers_faces);
  }

  /// GL buffer type or in GL-speak target rather than type
  void expand_texture_buffer(uint32_t* gl_buffer, uint32_t gl_buffer_type) {
    // TODO: Not texture aware
    /// Allocate new memory
    uint32_t gl_new_texture_array;
    glGenTextures(1, &gl_new_texture_array);
    glActiveTexture(GL_TEXTURE0 + gl_diffuse_texture_unit);
    glBindTexture(gl_buffer_type, gl_new_texture_array);
    // # of new textures to accommodate
    auto new_textures_capacity = (uint32_t) std::ceil(diffuse_textures_capacity * texture_array_growth_factor);
    auto layers_faces = 6 * new_textures_capacity;
    glTexStorage3D(gl_buffer_type, 1, GL_RGB8, 512, 512, layers_faces);
    /// Copy
    auto size = 512 * 512 * diffuse_textures_count; // 1 pixel = 1B given GL_RGB8
    
    /// Update state
    *gl_buffer = gl_new_texture_array;
    diffuse_textures_capacity = new_textures_capacity;
  }

  /// Id given to each unique mesh loaded by MeshManager
  ID mesh_id;
  Mesh mesh;
  
  /// Batch id
  ID id;
  
  /// Textures
  std::vector<ID> texture_ids; // FIXME: Used?
  std::map<ID, uint32_t> layer_idxs;
  float texture_array_growth_factor = 1.5; // new_buf_size = ceil(old_buf_size * growth_factor)
  // Diffuse texture buffer
  std::vector<ID> diffuse_textures;
  uint32_t diffuse_textures_count;    // # texture currently in the GL buffer
  uint32_t diffuse_textures_capacity; // # textures the GL buffer can hold
  
  uint32_t gl_diffuse_texture_array;
  uint32_t gl_diffuse_texture_type; // CUBE_MAP_ARRAY, 2D_TEXTURE_ARRAY, etc
  uint32_t gl_diffuse_texture_unit = 11; // FIXME: Hard coded
  
  // Specular texture buffer
  std::vector<ID> specular_textures; // TODO: Interleave with diffuse?
  uint32_t specular_textures_count;    // # texture currently in the GL buffer
  uint32_t specular_textures_capacity; // # textures the GL buffer can hold
  
  uint32_t gl_specular_texture_array;
  uint32_t gl_specular_texture_type; // CUBE_MAP_ARRAY, 2D_TEXTURE_ARRAY, etc
  uint32_t gl_specular_texture_unit = 12; // FIXME: Hard coded
  
  std::vector<RenderComponent*> components;
  
  /// Depth pass variables
  uint32_t gl_depth_vao;
  uint32_t gl_depth_models_buffer_object;
};

#endif // MEINEKRAFT_GRAPHICSBATCH_H
