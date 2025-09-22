/* SpriteManager.C
 * Manages sprite loading and retrieval for SDL2 observer
 */

#include "SpriteManager.h"
#include "XPMLoader.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <vector>
#include <SDL2/SDL.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

SpriteManager::SpriteManager(SDL_Renderer* rend) 
    : renderer(rend), spritesLoaded(false) {
    sprites.resize(SPRITE_COUNT, nullptr);
}

SpriteManager::~SpriteManager() {
    for (auto& sprite : sprites) {
        if (sprite) {
            SDL_DestroyTexture(sprite);
        }
    }
}

static inline std::string JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    char sep = '/';
    if (a.back() == sep) return a + b;
    return a + sep + b;
}

static inline bool FileExists(const std::string& p)
{
    std::ifstream f(p);
    return f.good();
}

static std::vector<std::string> GetBaseDirs()
{
    std::vector<std::string> dirs;

    // Executable base and common relatives
    char* base = SDL_GetBasePath();
    if (base)
    {
        std::string baseDir(base);
        SDL_free(base);
        dirs.push_back(baseDir);
        dirs.push_back(JoinPath(baseDir, "../"));
        dirs.push_back(JoinPath(baseDir, "../team/src"));
    }

    // Installed share dir
    #ifdef MM4_SHARE_DIR
    dirs.push_back(MM4_SHARE_DIR);
    #endif

    // Legacy CWD
    dirs.push_back(".");

    return dirs;
}

// Resolve a registry file path robustly
static std::string ResolveRegistryPath(const std::string& input)
{
    // Try as given
    if (FileExists(input)) return input;

    // Absolute path failed – nothing else to do
    if (!input.empty() && (input[0] == '/'
        #ifdef _WIN32
        || (input.size() > 1 && input[1] == ':')
        #endif
        ))
    {
        return input;
    }

    // Try across base dirs
    auto dirs = GetBaseDirs();
    for (const auto& d : dirs)
    {
        std::string cand = JoinPath(d, input);
        if (FileExists(cand)) return cand;
    }
    return input; // return original (will fail later with a clear error)
}

bool SpriteManager::LoadSprites(const std::string& registryFile) {
    std::string regPath = ResolveRegistryPath(registryFile);
    std::vector<std::string> spriteFiles = ParseGraphicsRegistry(regPath);

    if (spriteFiles.size() != SPRITE_COUNT) {
        std::cerr << "Sprite count mismatch - expected " << SPRITE_COUNT
                  << ", found " << spriteFiles.size() << std::endl;
        // This is OK - continue with what we have
    }

    // Load each sprite (up to the minimum of available files or SPRITE_COUNT)
    size_t numToLoad = std::min(spriteFiles.size(), static_cast<size_t>(SPRITE_COUNT));
    for (size_t i = 0; i < numToLoad; i++) {
        // The registry entries are resolved relative to the registry file location
        sprites[i] = XPMLoader::LoadXPM(renderer, spriteFiles[i]);

        if (!sprites[i]) {
            std::cerr << "Failed to load sprite: " << spriteFiles[i] << std::endl;
            // Continue loading other sprites
        }
    }

    spritesLoaded = true;
    return true;
}

std::vector<std::string> SpriteManager::ParseGraphicsRegistry(const std::string& filename) {
    std::vector<std::string> files;
    std::ifstream file(filename);

    if (!file.is_open()) {
        std::cerr << "Failed to open graphics registry: " << filename << std::endl;
        return files;
    }

    // Base directory used to resolve relative sprite paths
    auto dirOf = [](const std::string& p) -> std::string {
        size_t pos = p.find_last_of("/\\");
        if (pos == std::string::npos) return std::string(".");
        return p.substr(0, pos);
    };
    std::string baseDir = dirOf(filename);

    std::string line;
    while (std::getline(file, line)) {
        // Skip comments (lines starting with semicolon) and empty lines
        if (line.empty() || line[0] == ';') {
            continue;
        }

        // Each non-comment line is a sprite filename; resolve relative to the registry file
        if (!line.empty()) {
            if (line[0] == '/'
                #ifdef _WIN32
                || (line.size() > 1 && line[1] == ':')
                #endif
                )
            {
                files.push_back(line);
            } else {
                files.push_back(JoinPath(baseDir, line));
            }
        }
    }

    file.close();
    return files;
}

SDL_Texture* SpriteManager::GetSprite(SpriteType type, int frame) {
    int index = static_cast<int>(type) + frame;
    
    if (index < 0 || index >= SPRITE_COUNT) {
        return nullptr;
    }
    
    return sprites[index];
}

SDL_Texture* SpriteManager::GetShipSprite(int team, int imageSet, double angle) {
    int frame = AngleToFrame(angle);

    SpriteType baseType;
    // Team 0 uses Team 1 sprites, Team 1+ uses Team 2 sprites
    // Or we can use modulo to alternate: team % 2 == 0 for team 1 sprites
    if (team % 2 == 0) {
        switch (imageSet) {
            case 0: baseType = SPRITE_T1_SHIP_NORMAL; break;
            case 1: baseType = SPRITE_T1_SHIP_THRUST; break;
            case 2: baseType = SPRITE_T1_SHIP_BRAKE; break;
            case 3: baseType = SPRITE_T1_SHIP_LEFT; break;
            case 4: baseType = SPRITE_T1_SHIP_RIGHT; break;
            default: baseType = SPRITE_T1_SHIP_NORMAL; break;
        }
    } else {
        switch (imageSet) {
            case 0: baseType = SPRITE_T2_SHIP_NORMAL; break;
            case 1: baseType = SPRITE_T2_SHIP_THRUST; break;
            case 2: baseType = SPRITE_T2_SHIP_BRAKE; break;
            case 3: baseType = SPRITE_T2_SHIP_LEFT; break;
            case 4: baseType = SPRITE_T2_SHIP_RIGHT; break;
            default: baseType = SPRITE_T2_SHIP_NORMAL; break;
        }
    }
    
    return GetSprite(baseType, frame);
}

SDL_Texture* SpriteManager::GetAsteroidSprite(bool isVinyl, double mass, int frame) {
    // Determine size based on mass thresholds
    // Large > 300, Medium 150-300, Small < 150
    SpriteType type;

    if (isVinyl) {
        if (mass > 300.0) {
            type = SPRITE_VINYL_LARGE;
        } else if (mass > 150.0) {
            type = SPRITE_VINYL_MEDIUM;
        } else {
            type = SPRITE_VINYL_SMALL;
        }
    } else {
        if (mass > 300.0) {
            type = SPRITE_URANIUM_LARGE;
        } else if (mass > 150.0) {
            type = SPRITE_URANIUM_MEDIUM;
        } else {
            type = SPRITE_URANIUM_SMALL;
        }
    }

    // Ensure frame is within 0-17 range
    frame = frame % 18;

    return GetSprite(type, frame);
}

SDL_Texture* SpriteManager::GetStationSprite(int team, int frame) {
    // Team 0 uses Team 1 sprites, Team 1+ uses Team 2 sprites
    // Or use modulo to alternate
    SpriteType type = (team % 2 == 0) ? SPRITE_T1_STATION : SPRITE_T2_STATION;
    
    // Ensure frame is within 0-17 range
    frame = frame % 18;
    
    return GetSprite(type, frame);
}

int SpriteManager::AngleToFrame(double angle) {
    // Normalize angle to 0-360 range
    while (angle < 0) angle += 2 * M_PI;
    while (angle >= 2 * M_PI) angle -= 2 * M_PI;
    
    // Convert radians to degrees
    double degrees = angle * 180.0 / M_PI;
    
    // Map to frame (18 frames for 360 degrees)
    // Frame 0 is at 0 degrees (right), going counter-clockwise
    int frame = static_cast<int>((degrees + 10.0) / 20.0) % 18;
    
    return frame;
}
