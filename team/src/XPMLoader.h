/* XPMLoader.h
 * XPM file loader for SDL2
 * Converts XPM images to SDL2 textures
 */

#ifndef _XPM_LOADER_H_
#define _XPM_LOADER_H_

#include <SDL2/SDL.h>

#include <map>
#include <string>
#include <vector>

class XPMLoader {
 public:
  // Load an XPM file and return an SDL_Texture
  static SDL_Texture* LoadXPM(SDL_Renderer* renderer,
                              const std::string& filename);

 private:
  // Parse XPM header to get dimensions and color info
  struct XPMInfo {
    int width;
    int height;
    int numColors;
    int charsPerPixel;
  };

  // Parse XPM file
  static bool ParseXPMFile(const std::string& filename, XPMInfo& info,
                           std::map<std::string, SDL_Color>& colorMap,
                           std::vector<std::string>& pixels);

  // Parse color string (handle different formats)
  static SDL_Color ParseColor(const std::string& colorStr);

  // Convert hex color to SDL_Color
  static SDL_Color HexToColor(const std::string& hex);
};

#endif  // _XPM_LOADER_H_