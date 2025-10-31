/* SpriteManager.h
 * Manages sprite loading and retrieval for SDL2 observer
 */

#ifndef _SPRITE_MANAGER_H_
#define _SPRITE_MANAGER_H_

#include <SDL2/SDL.h>

#include <map>
#include <string>
#include <vector>

#include "SDL2Graphics.h"

// Sprite types based on graphics.reg - indices match order in parsed file
// (comments excluded)
enum SpriteType {
  // Impact/damage sprites (0-17 + bonk at 12)
  SPRITE_SHIP_IMPACT =
      0,             // shpimp000-017 (first 18 sprites, bonk.xpm at index 12)
  SPRITE_BONK = 12,  // bonk.xpm replaces shpimp012

  // Laser sprites (18-35)
  SPRITE_SHIP_LASER = 18,  // shplas000-017

  // Station impact/laser sprites (36-71)
  SPRITE_STATION_IMPACT = 36,  // stnimp000-017
  SPRITE_STATION_LASER = 54,   // stnlas000-017

  // Vinyl asteroid sprites (72-125)
  SPRITE_VINYL_LARGE = 72,   // vinlrg000-017
  SPRITE_VINYL_MEDIUM = 90,  // vinmed000-017
  SPRITE_VINYL_SMALL = 108,  // vinsml000-017

  // Uranium asteroid sprites (126-179)
  SPRITE_URANIUM_LARGE = 126,   // urnlrg000-017
  SPRITE_URANIUM_MEDIUM = 144,  // urnmed000-017
  SPRITE_URANIUM_SMALL = 162,   // urnsml000-017

  // Team 1 station (180-197)
  SPRITE_T1_STATION = 180,  // t1stat000-017

  // Team 1 ship sprites (198-287)
  SPRITE_T1_SHIP_NORMAL = 198,  // t1ship000-017
  SPRITE_T1_SHIP_THRUST = 216,  // t1shf000-017 (forward thrust)
  SPRITE_T1_SHIP_BRAKE = 234,   // t1shb000-017 (backward thrust)
  SPRITE_T1_SHIP_LEFT = 252,    // t1shl000-017
  SPRITE_T1_SHIP_RIGHT = 270,   // t1shr000-017

  // Team 2 station (288-305)
  SPRITE_T2_STATION = 288,  // t2stat000-017

  // Team 2 ship sprites (306-395)
  SPRITE_T2_SHIP_NORMAL = 306,  // t2ship000-017
  SPRITE_T2_SHIP_THRUST = 324,  // t2shf000-017 (forward thrust)
  SPRITE_T2_SHIP_BRAKE = 342,   // t2shb000-017 (backward thrust)
  SPRITE_T2_SHIP_LEFT = 360,    // t2shl000-017
  SPRITE_T2_SHIP_RIGHT = 378,   // t2shr000-017

  SPRITE_COUNT = 396  // Total actual sprites in registry
};

class SpriteManager {
 private:
  SDL_Renderer* renderer;
  std::vector<SDL_Texture*> sprites;
  bool spritesLoaded;

  // Parse graphics.reg file
  std::vector<std::string> ParseGraphicsRegistry(const std::string& filename);

  struct CustomShipArt {
    SDL_Texture* frames[16]{};
    bool valid = false;
  };
  std::map<std::string, CustomShipArt> customShipArtCache;

  CustomShipArt LoadCustomShipArtInternal(const std::string& artKey,
                                          const std::string& baseDir,
                                          const std::string& faction,
                                          const std::string& ship);

 public:
  SpriteManager(SDL_Renderer* rend);
  ~SpriteManager();

  // Load all sprites from graphics.reg
  bool LoadSprites(const std::string& registryFile = "graphics.reg");

  // Get sprite texture by type and frame
  SDL_Texture* GetSprite(SpriteType type, int frame = 0);

  // Get ship sprite based on team, state, and angle
  SDL_Texture* GetShipSprite(int team, int imageSet, double angle);

  // Get asteroid sprite based on type and size
  SDL_Texture* GetAsteroidSprite(bool isVinyl, double mass, int frame);

  // Get station sprite
  SDL_Texture* GetStationSprite(int team, int frame);

  // Check if sprites are loaded
  bool IsLoaded() const { return spritesLoaded; }

  // Convert angle to sprite frame (0-17)
  int AngleToFrame(double angle);

  // Custom ship art handling
  bool LoadCustomShipArt(const std::string& artKey, const std::string& baseDir,
                         const std::string& faction,
                         const std::string& ship);
  SDL_Texture* GetCustomShipTexture(const std::string& artKey, int frame) const;

 private:
};

#endif  // _SPRITE_MANAGER_H_
