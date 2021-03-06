#include "render.h"

#include <random>

#ifdef WIN32
#include <glew.h>
#else
#include <GL/glew.h>
#endif 

#include "camera.h"
#include "graphicsbatch.h"
#include "../util/filesystem.h"
#include "debug_opengl.h"
#include "rendercomponent.h"
#include "meshmanager.h"
#include "../nodes/entity.h"

#include <glm/common.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>

/// Pass handling code - used for debuggging at this moment
inline void pass_started(const std::string& msg) {
#if defined(__LINUX__) || defined(WIN32)
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, msg.c_str());
#endif
}

inline void pass_ended() {
#if defined(__LINUX__) || defined(WIN32)
  glPopDebugGroup();
#endif
}

uint32_t Renderer::get_next_free_texture_unit() {
  int32_t max_texture_units;
  glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_units);
  static int32_t next_texture_unit = 0;
  next_texture_unit++;
  if (next_texture_unit >= max_texture_units) {
    Log::error("Reached max texture units: " + std::to_string(max_texture_units));
    exit(1);
  }
  return next_texture_unit;
}

void Renderer::load_environment_map(const std::vector<std::string>& faces) {
  Texture texture;
  const auto resource = TextureResource{ faces };
  texture.data = Texture::load_textures(resource);
  if (texture.data.pixels) {
    texture.gl_texture_target = GL_TEXTURE_CUBE_MAP_ARRAY;
    texture.id = resource.to_hash();

    gl_environment_map_texture_unit = Renderer::get_next_free_texture_unit();
    glActiveTexture(GL_TEXTURE0 + gl_environment_map_texture_unit);
    uint32_t gl_environment_map_texture = 0; 
    glGenTextures(1, &gl_environment_map_texture);
    glBindTexture(texture.gl_texture_target, gl_environment_map_texture);
    glTexParameteri(texture.gl_texture_target, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(texture.gl_texture_target, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexStorage3D(texture.gl_texture_target, 1, GL_RGB8, texture.data.width, texture.data.height, texture.data.faces);
    glTexSubImage3D(texture.gl_texture_target, 0, 0, 0, 0, texture.data.width, texture.data.height, texture.data.faces, GL_RGB, GL_UNSIGNED_BYTE, texture.data.pixels);
    environment_map = texture;

    glObjectLabel(GL_TEXTURE, gl_environment_map_texture, -1, "Environment texture");
  } else {
    Log::warn("Could not load environment map");
  }
}

Renderer::~Renderer() = default;

Renderer::Renderer(): graphics_batches{} {
  glewExperimental = (GLboolean) true;
  glewInit();

#if defined(WIN32)
  // OpenGL debug output
  glEnable(GL_DEBUG_OUTPUT);
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(gl_debug_callback, 0);
  glDebugMessageControl(GL_DEBUG_SOURCE_APPLICATION, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_FALSE);
#endif
  
  const int screen_width = 1280; // TODO: Remove after singleton is removed
  const int screen_height = 720;

  /// Global geometry pass framebuffer
  glGenFramebuffers(1, &gl_depth_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_depth_fbo);

  // Global depth buffer
  gl_depth_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_depth_texture_unit);
  glGenTextures(1, &gl_depth_texture);
  glBindTexture(GL_TEXTURE_2D, gl_depth_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  // glTexParameteri(GL_TEXTURE_2D, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_DEPTH_COMPONENT); // Default value (intention only to read depth values from texture)
  glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, screen_width, screen_height, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, gl_depth_texture, 0);

  // Global normal buffer
  gl_normal_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_normal_texture_unit);
  glGenTextures(1, &gl_normal_texture);
  glBindTexture(GL_TEXTURE_2D, gl_normal_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_normal_texture, 0);

  // Global position buffer
  gl_position_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_position_texture_unit);
  glGenTextures(1, &gl_position_texture);
  glBindTexture(GL_TEXTURE_2D, gl_position_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, gl_position_texture, 0);

  // Global diffuse buffer
  gl_diffuse_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_diffuse_texture_unit);
  glGenTextures(1, &gl_diffuse_texture);
  glBindTexture(GL_TEXTURE_2D, gl_diffuse_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screen_width, screen_height, 0, GL_RGBA, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, gl_diffuse_texture, 0);

  // Global PBR parameters buffer
  gl_pbr_parameters_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_pbr_parameters_texture_unit);
  glGenTextures(1, &gl_pbr_parameters_texture);
  glBindTexture(GL_TEXTURE_2D, gl_pbr_parameters_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT3, gl_pbr_parameters_texture, 0);

  // Global ambient occlusion map
  gl_ambient_occlusion_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_ambient_occlusion_texture_unit);
  glGenTextures(1, &gl_ambient_occlusion_texture);
  glBindTexture(GL_TEXTURE_2D, gl_ambient_occlusion_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT4, gl_ambient_occlusion_texture, 0);

  // Global emissive map 
  gl_emissive_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_emissive_texture_unit);
  glGenTextures(1, &gl_emissive_texture);
  glBindTexture(GL_TEXTURE_2D, gl_emissive_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, screen_width, screen_height, 0, GL_RGB, GL_FLOAT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT5, gl_emissive_texture, 0);

  // Global shading model id 
  gl_shading_model_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_shading_model_texture_unit);
  glGenTextures(1, &gl_shading_model_texture);
  glBindTexture(GL_TEXTURE_2D, gl_shading_model_texture);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, screen_width, screen_height, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT6, gl_shading_model_texture, 0);

  uint32_t depth_attachments[7] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2, GL_COLOR_ATTACHMENT3, GL_COLOR_ATTACHMENT4, GL_COLOR_ATTACHMENT5, GL_COLOR_ATTACHMENT6 };
  glDrawBuffers(7, depth_attachments);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    Log::error("Lightning framebuffer status not complete.");
  }

  /// Point lightning framebuffer
  glGenFramebuffers(1, &gl_lightning_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, gl_lightning_fbo);

  GLuint gl_lightning_rbo;
  glGenRenderbuffers(1, &gl_lightning_rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, gl_lightning_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, screen_width, screen_height);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, gl_lightning_rbo);

  gl_lightning_texture_unit = Renderer::get_next_free_texture_unit();
  glActiveTexture(GL_TEXTURE0 + gl_lightning_texture_unit);
  glGenTextures(1, &gl_lightning_texture);
  glBindTexture(GL_TEXTURE_2D, gl_lightning_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, screen_width, screen_height, 0, GL_RGBA, GL_FLOAT, nullptr);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, gl_lightning_texture, 0);

  uint32_t lightning_attachments[1] = { GL_COLOR_ATTACHMENT0 };
  glDrawBuffers(1, lightning_attachments);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    Log::error("Point lightning framebuffer status not complete.");
  }

  /// Lightning pass shader
  bool success = false;
  std::string err_msg;
  lightning_shader = new Shader{Filesystem::base + "shaders/lightning.vert", Filesystem::base + "shaders/lightning.frag" };
  std::tie(success, err_msg) = lightning_shader->compile();
  if (!success) {
    Log::error("Lightning shader compilation failed; " + err_msg);
  }

  /// Point light pass setup
  {
    const auto program = lightning_shader->gl_program;
    glGenVertexArrays(1, &gl_lightning_vao);
    glBindVertexArray(gl_lightning_vao);

    GLuint gl_vbo;
    glGenBuffers(1, &gl_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Primitive::quad), &Primitive::quad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(glGetAttribLocation(program, "position"));
    glVertexAttribPointer(glGetAttribLocation(program, "position"), 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
  
    /// Shader storage buffer object for PointLights: bind it to the SSBO
    GLuint gl_ssbo_block_idx = glGetProgramResourceIndex(program, GL_SHADER_STORAGE_BLOCK, "PointLightBlock");
    std::cerr << gl_ssbo_block_idx << std::endl;
    glShaderStorageBlockBinding(program, gl_ssbo_block_idx, gl_pointlight_ssbo_binding_point_idx);
  }

  pointlights.emplace_back(PointLight(Vec3f(0.0, 0.0, 5.0)));
  pointlights.emplace_back(PointLight(Vec3f(10.0, 10.0, 5.0)));
  pointlights.emplace_back(PointLight(Vec3f(0.0, 10.0, 5.0)));
  pointlights.emplace_back(PointLight(Vec3f(10.0, 0.0, 5.0)));

  /// Create SSBO for the PointLights
  glGenBuffers(1, &gl_pointlight_ssbo);
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_pointlight_ssbo);
  glBufferData(GL_SHADER_STORAGE_BUFFER, pointlights.size() * sizeof(PointLight), pointlights.data(), GL_DYNAMIC_COPY);
  glBindBufferBase(GL_SHADER_STORAGE_BUFFER, gl_pointlight_ssbo_binding_point_idx, gl_pointlight_ssbo);

  /// Update
  glBindBuffer(GL_SHADER_STORAGE_BUFFER, gl_pointlight_ssbo);
  GLvoid* ssbo = glMapBufferRange(GL_SHADER_STORAGE_BUFFER, 0, pointlights.size() * sizeof(PointLight), GL_MAP_WRITE_BIT);
  std::memcpy(ssbo, pointlights.data(), pointlights.size() * sizeof(PointLight)); 
  glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);

  glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);
  glEnable(GL_CULL_FACE);
  glCullFace(GL_BACK);

  /// Camera
  const auto position  = Vec3f{8.0f, 8.0f, 8.0f};  
  const auto direction = Vec3f{0.0,  0.0, -1.0};
  const auto world_up  = Vec3f{0.0f, 1.0f, 0.0f};  
  camera = new Camera(position, direction, world_up);
}

void Renderer::render(uint32_t delta) {
  /// Reset render stats
  state = RenderState(state);
  state.frame++;

  /// Renderer caches the transforms of components thus we need to fetch the ones who changed during the last frame 
  if (state.frame % 10 == 0) { 
    TransformSystem::instance().reset_dirty();
  }
  update_transforms();  

  /// Culls objects in all batches
  // cull_objects();

  glm::mat4 camera_transform = camera->transform(); 

  /// Geometry pass
  pass_started("Geometry pass");
  {
    glBindFramebuffer(GL_FRAMEBUFFER, gl_depth_fbo);
    glEnable(GL_DEPTH_TEST);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT); // Always update the depth buffer with the new values
    for (size_t i = 0; i < graphics_batches.size(); i++) {
      const auto& batch = graphics_batches[i];
      const auto program = batch.depth_shader.gl_program;
      glUseProgram(program);
      glUniformMatrix4fv(glGetUniformLocation(program, "camera_view"), 1, GL_FALSE, glm::value_ptr(camera_transform));
      
      glBindBuffer(GL_ARRAY_BUFFER, batch.gl_depth_models_buffer_object);
      glBufferData(GL_ARRAY_BUFFER, batch.objects.transforms.size() * sizeof(Mat4<float>), batch.objects.transforms.data(), GL_DYNAMIC_DRAW);
      
      glBindBuffer(GL_ARRAY_BUFFER, batch.gl_diffuse_textures_layer_idx);
      glBufferData(GL_ARRAY_BUFFER, batch.objects.diffuse_texture_idxs.size() * sizeof(uint32_t), batch.objects.diffuse_texture_idxs.data(), GL_DYNAMIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, batch.gl_shading_model_buffer_object);
      glBufferData(GL_ARRAY_BUFFER, batch.objects.shading_models.size() * sizeof(ShadingModel), batch.objects.shading_models.data(), GL_DYNAMIC_DRAW);

      glBindBuffer(GL_ARRAY_BUFFER, batch.gl_pbr_scalar_buffer_object);
      glBufferData(GL_ARRAY_BUFFER, batch.objects.pbr_scalar_parameters.size() * sizeof(Vec3<float>), batch.objects.pbr_scalar_parameters.data(), GL_DYNAMIC_DRAW);

      glBindVertexArray(batch.gl_depth_vao);
      
      glDrawElementsInstanced(GL_TRIANGLES, batch.mesh.indices.size(), GL_UNSIGNED_INT, nullptr, batch.objects.transforms.size());
      
      state.entities += batch.objects.transforms.size();
      state.draw_calls++;
    }
  }
  pass_ended();

  pass_started("Lightning pass");
  {
    const auto program = lightning_shader->gl_program;
    glBindFramebuffer(GL_FRAMEBUFFER, gl_lightning_fbo);

    glBindVertexArray(gl_lightning_vao);
    glUseProgram(program);

    glUniform1i(glGetUniformLocation(program, "environment_map_sampler"), gl_environment_map_texture_unit);
    glUniform1i(glGetUniformLocation(program, "shading_model_id_sampler"), gl_shading_model_texture_unit);
    glUniform1i(glGetUniformLocation(program, "emissive_sampler"), gl_emissive_texture_unit);
    glUniform1i(glGetUniformLocation(program, "ambient_occlusion_sampler"), gl_ambient_occlusion_texture_unit);
    glUniform1i(glGetUniformLocation(program, "pbr_parameters_sampler"), gl_pbr_parameters_texture_unit);
    glUniform1i(glGetUniformLocation(program, "diffuse_sampler"), gl_diffuse_texture_unit);
    glUniform1i(glGetUniformLocation(program, "normal_sampler"), gl_normal_texture_unit);
    glUniform1i(glGetUniformLocation(program, "position_sampler"), gl_position_texture_unit);
    glUniform1f(glGetUniformLocation(program, "screen_width"), screen_width);
    glUniform1f(glGetUniformLocation(program, "screen_height"), screen_height);

    glUniform3fv(glGetUniformLocation(program, "camera"), 1, &camera->position.x);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  pass_ended();

  /// Copy final pass into default FBO
  pass_started("Final blit pass");
  {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, gl_lightning_fbo);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    auto mask = GL_COLOR_BUFFER_BIT;
    auto filter = GL_NEAREST;
    glBlitFramebuffer(0, 0, screen_width, screen_height, 0, 0, screen_width, screen_height, mask, filter);
  }
  pass_ended();

  log_gl_error();
  state.graphic_batches = graphics_batches.size();
}

void Renderer::update_projection_matrix(const float fov) {
  // TODO: Adjust all the passes textures sizes & all the global texture buffers
  const float aspect = (float) screen_width / (float) screen_height;
  this->projection_matrix = glm::perspective(glm::radians(fov), aspect, 0.1f, 1000.0f);
  glViewport(0, 0, screen_width, screen_height); 
}

void Renderer::link_batch(GraphicsBatch& batch) {
  /// Geometry pass setup
  {    
    /// Shaderbindings
    const auto program = batch.depth_shader.gl_program;
    glUseProgram(program);
    glUniformMatrix4fv(glGetUniformLocation(program, "projection"), 1, GL_FALSE, glm::value_ptr(projection_matrix));
    glUniform1i(glGetUniformLocation(program, "diffuse"), batch.gl_diffuse_texture_unit);
    glUniform1i(glGetUniformLocation(program, "pbr_parameters"), batch.gl_metallic_roughness_texture_unit);
    glUniform1i(glGetUniformLocation(program, "ambient_occlusion"), batch.gl_ambient_occlusion_texture_unit);
    glUniform1i(glGetUniformLocation(program, "emissive"), batch.gl_emissive_texture_unit);
    
    glGenVertexArrays(1, &batch.gl_depth_vao);
    glBindVertexArray(batch.gl_depth_vao);

    glGenBuffers(1, &batch.gl_depth_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_depth_vbo);
    glBufferData(GL_ARRAY_BUFFER, batch.mesh.byte_size_of_vertices(), batch.mesh.vertices.data(), GL_STATIC_DRAW);

    const auto position_attrib = glGetAttribLocation(program, "position");
    glVertexAttribPointer(position_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *) offsetof(Vertex<float>, position));
    glEnableVertexAttribArray(position_attrib);

    const auto normal_attrib = glGetAttribLocation(program, "normal");
    glVertexAttribPointer(normal_attrib, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *) offsetof(Vertex<float>, normal));
    glEnableVertexAttribArray(normal_attrib);

    const auto texcoord_attrib = glGetAttribLocation(program, "texcoord");
    glVertexAttribPointer(texcoord_attrib, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex<float>), (const void *) offsetof(Vertex<float>, tex_coord));
    glEnableVertexAttribArray(texcoord_attrib);

    // Buffer for all the model matrices
    glGenBuffers(1, &batch.gl_depth_models_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_depth_models_buffer_object);

    const auto model_attrib = glGetAttribLocation(program, "model");
    for (int i = 0; i < 4; i++) {
      glVertexAttribPointer(model_attrib + i, 4, GL_FLOAT, GL_FALSE, sizeof(Mat4<float>), (const void *) (sizeof(float) * i * 4));
      glEnableVertexAttribArray(model_attrib + i);
      glVertexAttribDivisor(model_attrib + i, 1);
    }

    // Buffer for all the diffuse texture indices
    glGenBuffers(1, &batch.gl_diffuse_textures_layer_idx);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_diffuse_textures_layer_idx);
    glVertexAttribIPointer(glGetAttribLocation(program, "diffuse_layer_idx"), 1, GL_UNSIGNED_INT, sizeof(GLint), nullptr);
    glEnableVertexAttribArray(glGetAttribLocation(program, "diffuse_layer_idx"));
    glVertexAttribDivisor(glGetAttribLocation(program, "diffuse_layer_idx"), 1);

    glGenBuffers(1, &batch.gl_shading_model_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_shading_model_buffer_object);
    glVertexAttribIPointer(glGetAttribLocation(program, "shading_model_id"), 1, GL_UNSIGNED_INT, sizeof(GLuint), nullptr);
    glEnableVertexAttribArray(glGetAttribLocation(program, "shading_model_id"));
    glVertexAttribDivisor(glGetAttribLocation(program, "shading_model_id"), 1);

    // FIXME: Not all configurations needs this
    glGenBuffers(1, &batch.gl_pbr_scalar_buffer_object);
    glBindBuffer(GL_ARRAY_BUFFER, batch.gl_pbr_scalar_buffer_object);
    glVertexAttribPointer(glGetAttribLocation(program, "pbr_scalar_parameters"), 3, GL_FLOAT, GL_FALSE, sizeof(Vec3<float>), nullptr);
    glEnableVertexAttribArray(glGetAttribLocation(program, "pbr_scalar_parameters"));
    glVertexAttribDivisor(glGetAttribLocation(program, "pbr_scalar_parameters"), 1);

    GLuint EBO;
    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, batch.mesh.byte_size_of_indices(), batch.mesh.indices.data(), GL_STATIC_DRAW);
  }
}

void Renderer::add_component(const RenderComponent comp, const ID entity_id) {
  // Handle the config of the Shader from the component
  std::set<Shader::Defines> comp_shader_config;

  if (comp.diffuse_texture.data.pixels) {
    switch (comp.diffuse_texture.gl_texture_target) {
    case GL_TEXTURE_2D_ARRAY:
      comp_shader_config.insert(Shader::Defines::Diffuse2D);
      break;
    case GL_TEXTURE_CUBE_MAP_ARRAY:
      comp_shader_config.insert(Shader::Defines::DiffuseCubemap);
      break;
    default:
      Log::error("Depth shader diffuse texture type not handled.");
    }
  }

  // Shader configuration and mesh id defines the uniqueness of a GBatch
  for (auto& batch : graphics_batches) {
    if (batch.mesh_id != comp.mesh_id) { continue; }
    if (comp_shader_config != batch.depth_shader.defines) { continue; }
    if (comp.diffuse_texture.data.pixels) {
      bool batch_contains_texture = batch.layer_idxs.count(comp.diffuse_texture.id) == 0;
      if (!batch_contains_texture) {
        batch.objects.diffuse_texture_idxs.push_back(batch.layer_idxs[comp.diffuse_texture.id]);
      } else {
        /// Expand texture buffer if needed
        if (batch.diffuse_textures_count + 1 > batch.diffuse_textures_capacity) {
          batch.expand_texture_buffer(comp.diffuse_texture, &batch.gl_diffuse_texture_array, &batch.diffuse_textures_capacity, batch.gl_diffuse_texture_unit);
        }

        /// Update the mapping from texture id to layer idx and increment count
        batch.layer_idxs[comp.diffuse_texture.id] = batch.diffuse_textures_count++;
        batch.objects.diffuse_texture_idxs.push_back(batch.layer_idxs[comp.diffuse_texture.id]);

        /// Upload the texture to OpenGL
        batch.upload(comp.diffuse_texture, batch.gl_diffuse_texture_unit, batch.gl_diffuse_texture_array);
      }
    }
    add_graphics_state(batch, comp, entity_id);
    return;
  }

  GraphicsBatch batch{comp.mesh_id};
  batch.mesh = MeshManager::mesh_from_id(comp.mesh_id);

  /// Batch shader prepass (depth pass) shader creation process
  batch.depth_shader = Shader{ Filesystem::base + "shaders/geometry.vert", Filesystem::base + "shaders/geometry.frag" };
  batch.depth_shader.defines = comp_shader_config;

  if (comp.diffuse_texture.data.pixels) {
    batch.gl_diffuse_texture_unit = Renderer::get_next_free_texture_unit();

    batch.init_buffer(comp.diffuse_texture, &batch.gl_diffuse_texture_array, batch.gl_diffuse_texture_unit, &batch.diffuse_textures_capacity);

    /// Update the mapping from texture id to layer idx and increment count
    batch.layer_idxs[comp.diffuse_texture.id] = batch.diffuse_textures_count++;
    batch.objects.diffuse_texture_idxs.push_back(batch.layer_idxs[comp.diffuse_texture.id]);

    /// Upload the texture to OpenGL
    batch.upload(comp.diffuse_texture, batch.gl_diffuse_texture_unit, batch.gl_diffuse_texture_array);
  }

  if (comp.metallic_roughness_texture.data.pixels) {
    const Texture& texture = comp.metallic_roughness_texture;
    batch.gl_metallic_roughness_texture_unit = Renderer::get_next_free_texture_unit();
    glActiveTexture(GL_TEXTURE0 + batch.gl_metallic_roughness_texture_unit);
    uint32_t gl_metallic_roughness_texture = 0;
    glGenTextures(1, &gl_metallic_roughness_texture);
    glBindTexture(texture.gl_texture_target, gl_metallic_roughness_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(texture.gl_texture_target, 0, GL_RGB, texture.data.width, texture.data.height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture.data.pixels);
  }

  if (comp.ambient_occlusion_texture.data.pixels) {
    const Texture& texture = comp.ambient_occlusion_texture;
    batch.gl_ambient_occlusion_texture_unit = Renderer::get_next_free_texture_unit();
    glActiveTexture(GL_TEXTURE0 + batch.gl_ambient_occlusion_texture_unit);
    uint32_t gl_ambient_occlusion_texture = 0;
    glGenTextures(1, &gl_ambient_occlusion_texture);
    glBindTexture(texture.gl_texture_target, gl_ambient_occlusion_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(texture.gl_texture_target, 0, GL_RGB, texture.data.width, texture.data.height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture.data.pixels);
  }

  if (comp.emissive_texture.data.pixels) {
    const Texture& texture = comp.emissive_texture;
    batch.gl_emissive_texture_unit = Renderer::get_next_free_texture_unit();
    glActiveTexture(GL_TEXTURE0 + batch.gl_emissive_texture_unit);
    uint32_t gl_emissive_texture = 0;
    glGenTextures(1, &gl_emissive_texture);
    glBindTexture(texture.gl_texture_target, gl_emissive_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(texture.gl_texture_target, 0, GL_RGB, texture.data.width, texture.data.height, 0, GL_RGB, GL_UNSIGNED_BYTE, texture.data.pixels);
  }

  std::string err_msg;
  bool success;
  std::tie(success, err_msg) = batch.depth_shader.compile();
  if (!success) {
    Log::error("Shader compilation failed; " + err_msg);
    return;
  }

  link_batch(batch);

  add_graphics_state(batch, comp, entity_id);
  graphics_batches.push_back(batch);
}

void Renderer::remove_component(ID entity_id) {
  // TODO: Implement
}

void Renderer::add_graphics_state(GraphicsBatch& batch, const RenderComponent& comp, ID entity_id) {
  batch.entity_ids.push_back(entity_id);
  batch.data_idx[entity_id] = batch.entity_ids.size() - 1;
  batch.objects.transforms.push_back(TransformSystem::instance().lookup(entity_id));
  batch.objects.pbr_scalar_parameters.push_back(comp.pbr_scalar_parameters);
  batch.objects.shading_models.push_back(comp.shading_model);
}

void Renderer::update_transforms() {
  std::vector<ID> job_ids(graphics_batches.size());
  const std::vector<ID> t_ids = TransformSystem::instance().get_dirty_transforms();
  // Log::info("Dirty ids: " + std::to_string(t_ids.size()));
  for (size_t i = 0; i < graphics_batches.size(); i++) {
    ID job_id = JobSystem::instance().execute([=](){ // FIXME: Remove the copy of t_ids
      auto& batch = graphics_batches[i];
      for (const auto& t_id : t_ids) {
        const auto idx = batch.data_idx.find(t_id);
        if (idx == batch.data_idx.cend()) { continue; }
        batch.objects.transforms[idx->second] = TransformSystem::instance().lookup(t_id);
      }
    });
    job_ids.push_back(job_id);
  }

  JobSystem::instance().wait_on_all();
}