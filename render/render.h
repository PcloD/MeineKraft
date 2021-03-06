#pragma once
#ifndef MEINEKRAFT_RENDER_H
#define MEINEKRAFT_RENDER_H

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <array>
#include <memory>

#include "texture.h"
#include "light.h"

#include <glm/mat4x4.hpp>

struct Camera;
struct RenderComponent;
struct GraphicsBatch;
struct Shader;
struct RenderPass;

class Renderer {
public:
  Renderer(Renderer& render) = delete;
  ~Renderer();

  /// Singleton instance of core Render, use with caution.
  static Renderer& instance() {
    static Renderer instance;
    return instance;
  }

  /// Main render function, renders all the graphics batches
  void render(uint32_t delta);
  
  /// Adds the data of a RenderComponent to a internal batch
  void add_component(const RenderComponent comp, const ID entity_id);

  void remove_component(ID entity_id);

  /// Updates all the shaders projection matrices in order to support resizing of the window
  void update_projection_matrix(const float fov);

  /// Returns the next unused texture unit
  static uint32_t get_next_free_texture_unit();

  void load_environment_map(const std::vector<std::string>& faces);

  Camera* camera;
  RenderState state;
  glm::mat4 projection_matrix; 
  float screen_width;
  float screen_height;
  std::vector<GraphicsBatch> graphics_batches;
  std::vector<PointLight> pointlights;

private:
  Renderer();
  void add_graphics_state(GraphicsBatch& batch, const RenderComponent& comp, ID entity_id);
  void update_transforms();
  void link_batch(GraphicsBatch& batch);
  
  /// Geometry pass related
  uint32_t gl_depth_fbo;
  uint32_t gl_depth_texture;
  uint32_t gl_depth_texture_unit;

  /// Lightning pass related
  Shader* lightning_shader;
  // Used since default fbo is not to be trusted
  uint32_t gl_lightning_texture;
  uint32_t gl_lightning_fbo;
  uint32_t gl_lightning_texture_unit;
  uint32_t gl_lightning_vao;

  uint32_t gl_pointlight_ssbo_binding_point_idx = 0;
  uint32_t gl_pointlight_ssbo;

  /// Global buffers
  // Normals
  uint32_t gl_normal_texture;
  uint32_t gl_normal_texture_unit;
  // Positions
  uint32_t gl_position_texture;
  uint32_t gl_position_texture_unit;
  // Diffuse
  uint32_t gl_diffuse_texture;
  uint32_t gl_diffuse_texture_unit;
  // PBR Parameters
  uint32_t gl_pbr_parameters_texture;
  uint32_t gl_pbr_parameters_texture_unit;
  // Ambient occlusion map
  uint32_t gl_ambient_occlusion_texture;
  uint32_t gl_ambient_occlusion_texture_unit;
  // Emissive map
  uint32_t gl_emissive_texture_unit;
  uint32_t gl_emissive_texture;
  // Shading model 
  uint32_t gl_shading_model_texture_unit;
  uint32_t gl_shading_model_texture;

  // Environment map
  Texture environment_map; 
  uint32_t gl_environment_map_texture_unit;
};

#endif // MEINEKRAFT_RENDER_H
