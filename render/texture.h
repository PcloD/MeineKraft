#pragma once
#ifndef MEINEKRAFT_TEXTURE_H
#define MEINEKRAFT_TEXTURE_H

#include <cstdint>
#include <string>
#include <vector>
#include <cstring>

#ifdef _WIN32
#include <SDL_image.h>
#else
#include <SDL2/SDL_image.h>
#endif 

#include "../util/logging.h"

/// Opaque ID type used to reference resources throughout the engine
typedef uint64_t ID;

struct RawTexture {
  uint8_t* pixels = nullptr;
  uint32_t size   = 0; // Byte size per face
  uint32_t width  = 0; // Measured in pixels
  uint32_t height = 0; 
  uint32_t faces  = 0; // Number of faces, used for cube maps
  RawTexture() = default;
};

struct TextureResource {
  std::vector<std::string> files;
  
  explicit TextureResource(std::string file): files{file} {};
  explicit TextureResource(std::vector<std::string> files): files{files} {};
  
  uint64_t to_hash() const {
    uint64_t hash = 0;
    for (const auto& file : files) {
      hash += std::hash<std::string>{}(file);
    }
    return hash;
  }
};

struct Texture {
  static RawTexture load_textures(const TextureResource resource) {
    RawTexture texture{};

    if (resource.files.empty()) { return texture; }

    const auto& file = resource.files.front();
    SDL_Surface* image = IMG_Load(file.c_str());

    if (image) {
      texture.width = static_cast<uint32_t>(image->w);
      texture.height = static_cast<uint32_t>(image->h);
      const auto bytes_per_pixel = image->format->BytesPerPixel;
      SDL_FreeSurface(image); // FIXME: Wasteful

      // Assumes that the files are the same size, in right order, same encoding, etc
      texture.size = bytes_per_pixel * texture.width * texture.height;
      texture.pixels = static_cast<uint8_t*>(std::calloc(1, texture.size * resource.files.size()));

      // Load all the files into a linear memory region
      for (size_t i = 0; i < resource.files.size(); i++) {
        image = IMG_Load(resource.files[i].c_str());
        if (!image) {
          Log::error("Could not load texture: " + std::string(IMG_GetError()));
          continue;
        }
        if (image->w != texture.width || image->h != texture.height) {
          Log::error("Textures non-uniform size");
          return texture;
        }
        // Convert it to OpenGL friendly format
        const auto desired_img_format = bytes_per_pixel == 3 ? SDL_PIXELFORMAT_RGB24 : SDL_PIXELFORMAT_RGBA32;
        SDL_Surface* conv = SDL_ConvertSurfaceFormat(image, desired_img_format, 0);
        if (conv) {
          std::memcpy(texture.pixels + texture.size * i, conv->pixels, texture.size);
          SDL_FreeSurface(image);
        } else {
          Log::error(SDL_GetError());
        }

        texture.faces++;
      }
    } else {
      Log::error("Could not load textures: " + std::string(IMG_GetError()));
    }

    return texture;
  }
  
  /// Texture id
  ID id = 0;
  
  RawTexture data;

  /// OpenGL texture target; CUBE_MAP, CUBE_MAP_ARRAY, TEXTURE_2D, etc
  uint32_t gl_texture_target = 0;

  enum class Type: uint8_t {
    Diffuse, MetallicRoughness, AmbientOcclusion, Emissive
  };
};

#endif // MEINEKRAFT_TEXTURE_H
