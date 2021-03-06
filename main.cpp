#include <memory>
#include <chrono>

#include "render/render.h"
#include "include/imgui/imgui_impl_sdl.h"
#include "render/camera.h"
#include "nodes/model.h"
#include "util/filesystem.h"
#include "scene/world.hpp"
#include "render/debug_opengl.h"
#include "nodes/skybox.h"
#include "scene/world.hpp"
#include "render/graphicsbatch.h"

struct Resolution {
  int width, height;
};

static auto HD      = Resolution{1280, 720};
static auto FULL_HD = Resolution{1920, 1080};

#ifdef WIN32
int wmain() {
#else
int main() {
#endif
  SDL_Init(SDL_INIT_EVERYTHING);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 6);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  auto window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MOUSE_CAPTURE;
  SDL_Window* window = SDL_CreateWindow("MeineKraft", 100, 100, HD.width, HD.height, window_flags);
  SDL_GLContext context = SDL_GL_CreateContext(window);
  SDL_GL_SetSwapInterval(0); // Disables vsync

  OpenGLContextInfo gl_context_info;

  // Init sdl2_image
  atexit(IMG_Quit);
  IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG);

  // Init ImGui
  ImGui_ImplSdlGL3_Init(window);
  
  // Inits GLEW
  Renderer& renderer = Renderer::instance();
  renderer.screen_width = HD.width;
  renderer.screen_height = HD.height;
  renderer.update_projection_matrix(70);

  Skybox skybox;

  Model model{ Filesystem::home + "Desktop/", "DamagedHelmet.gltf" };
  
  World world;  

  bool toggle_mouse_capture = true;
  bool DONE = false;
  auto last_tick = std::chrono::high_resolution_clock::now();
  auto current_tick = last_tick;
  int64_t delta = 0;
  
  /// Delta values
  const int num_deltas = 100;
  float deltas[num_deltas];
  
  while (!DONE) {
      current_tick = std::chrono::high_resolution_clock::now();
      delta = std::chrono::duration_cast<std::chrono::milliseconds>(current_tick - last_tick).count();
      last_tick = current_tick;

      /// Process input
      SDL_Event event{};
      while (SDL_PollEvent(&event) != 0) {
        ImGui_ImplSdlGL3_ProcessEvent(&event);
        switch (event.type) {
        case SDL_MOUSEMOTION:
          if (toggle_mouse_capture) { break; }
          // renderer.camera->pitch = 0; 
          // renderer.camera->yaw = 0;
          renderer.camera->pitch += event.motion.yrel;
          renderer.camera->yaw += event.motion.xrel;
          renderer.camera->direction = renderer.camera->recalculate_direction();
          break;
        case SDL_KEYDOWN:
          switch (event.key.keysym.sym) {
            case SDLK_w:
              renderer.camera->move_forward(true);
              break;
            case SDLK_a:
              renderer.camera->move_left(true);
              break;
            case SDLK_s:
              renderer.camera->move_backward(true);
              break;
            case SDLK_d:
              renderer.camera->move_right(true);
              break;
            case SDLK_q:
              renderer.camera->move_down(true);
              break;
            case SDLK_e:
              renderer.camera->move_up(true);
              break;
            case SDLK_TAB:
              toggle_mouse_capture = !toggle_mouse_capture;
              break;
            case SDLK_ESCAPE:
              DONE = true;
              break;
          }
          break;
        case SDL_KEYUP:
          switch (event.key.keysym.sym) {
            case SDLK_w:
              renderer.camera->move_forward(false);
              break;
            case SDLK_a:
              renderer.camera->move_left(false);
              break;
            case SDLK_s:
              renderer.camera->move_backward(false);
              break;
            case SDLK_d:
              renderer.camera->move_right(false);
              break;
            case SDLK_q:
              renderer.camera->move_down(false);
              break;
            case SDLK_e:
              renderer.camera->move_up(false);
          }
        case SDL_WINDOWEVENT:
          switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:
              renderer.update_projection_matrix(70);
              break;
          }
          break;
        case SDL_QUIT:
          DONE = true;
          break;
      }
    }
    renderer.camera->position = renderer.camera->update(delta);

    /// Run all actions
    ActionSystem::instance().execute_actions(renderer.state.frame, delta);

    /// Let the game do its thing
    world.tick();

    /// Render the world
    renderer.render(delta);

    /// ImGui - Debug instruments
    {
      ImGui_ImplSdlGL3_NewFrame(window);
      auto io = ImGui::GetIO();
      ImGui::Begin("Information Panel");

      if (ImGui::CollapsingHeader("Render System", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Text("Frame: %llu", renderer.state.frame);
        ImGui::Text("Entities: %llu", renderer.state.entities);
        ImGui::Text("Average %lld ms / frame (%.1f FPS)", delta, io.Framerate);

        static size_t i = -1; i = (i + 1) % num_deltas;
        deltas[i] = float(delta);
        ImGui::PlotLines("", deltas, num_deltas, 0, "ms / frame", 0.0f, 50.0f, ImVec2(ImGui::GetWindowWidth(), 100));

        if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
          ImGui::InputFloat3("Position", &renderer.camera->position.x);
          ImGui::InputFloat3("Direction", &renderer.camera->direction.x);
        }

        if (ImGui::CollapsingHeader("Graphics batches")) {
          ImGui::Text("Graphics batches: %llu", renderer.state.graphic_batches);
          for (size_t batch_num = 0; batch_num < renderer.graphics_batches.size(); batch_num++) {
            const auto& batch = renderer.graphics_batches[batch_num];
            std::string batch_title = "Batch #" + std::to_string(batch_num);
            if (ImGui::CollapsingHeader(batch_title.c_str())) {
              ImGui::Text("Size: %llu", batch.entity_ids.size());
              if (ImGui::CollapsingHeader("Members")) {
                for (const auto& id : batch.entity_ids) {
                  ImGui::Text("Entity id: %llu", id);
                  Vec3f position = TransformSystem::instance().lookup(id).matrix.get_translation();
                  ImGui::InputFloat3("Position", &position.x);
                }
              }
            }
          }
        }

      }

      ImGui::End();
      ImGui::Render();
    }
    SDL_GL_SwapWindow(window);
  }
  JobSystem::instance().wait_on_all();
  ImGui_ImplSdlGL3_Shutdown();
  SDL_GL_DeleteContext(context);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return EXIT_SUCCESS;
}
