/* XPMLoader.C
 * XPM file loader implementation for SDL2
 */

#include "XPMLoader.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>

SDL_Texture* XPMLoader::LoadXPM(SDL_Renderer* renderer, const std::string& filename) {
    XPMInfo info;
    std::map<std::string, SDL_Color> colorMap;
    std::vector<std::string> pixels;

    if (!ParseXPMFile(filename, info, colorMap, pixels)) {
        std::cerr << "Failed to parse XPM file: " << filename << std::endl;
        return nullptr;
    }

    // Create surface
    SDL_Surface* surface = SDL_CreateRGBSurface(0, info.width, info.height, 32,
                                                 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
    if (!surface) {
        std::cerr << "Failed to create surface for XPM: " << SDL_GetError() << std::endl;
        return nullptr;
    }

    // Fill surface with pixel data
    Uint32* pixelData = (Uint32*)surface->pixels;

    for (int y = 0; y < info.height; y++) {
        const std::string& row = pixels[y];
        for (int x = 0; x < info.width; x++) {
            std::string key = row.substr(x * info.charsPerPixel, info.charsPerPixel);

            auto it = colorMap.find(key);
            if (it != colorMap.end()) {
                SDL_Color& color = it->second;
                Uint32 pixel = SDL_MapRGBA(surface->format, color.r, color.g, color.b, color.a);
                pixelData[y * info.width + x] = pixel;
            }
        }
    }

    // Create texture from surface
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (!texture) {
        std::cerr << "Failed to create texture from XPM: " << SDL_GetError() << std::endl;
    }

    return texture;
}

bool XPMLoader::ParseXPMFile(const std::string& filename,
                             XPMInfo& info,
                             std::map<std::string, SDL_Color>& colorMap,
                             std::vector<std::string>& pixels) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string line;
    bool inData = false;
    bool readHeader = false;
    int colorsRead = 0;
    int pixelRowsRead = 0;

    while (std::getline(file, line)) {
        // Skip empty lines and C-style comments
        if (line.empty() || line.find("/*") == 0) {
            continue;
        }

        // Look for start of data
        if (!inData) {
            if (line.find("static char") != std::string::npos) {
                inData = true;
            }
            continue;
        }

        // Remove quotes and commas
        size_t firstQuote = line.find('"');
        size_t lastQuote = line.rfind('"');
        if (firstQuote == std::string::npos || lastQuote == firstQuote) {
            continue;
        }

        std::string data = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);

        // Parse header
        if (!readHeader) {
            std::istringstream iss(data);
            iss >> info.width >> info.height >> info.numColors >> info.charsPerPixel;
            readHeader = true;
            continue;
        }

        // Parse color definitions
        if (colorsRead < info.numColors) {
            // Extract color key (first charsPerPixel characters)
            std::string key = data.substr(0, info.charsPerPixel);

            // Find 'c' for color definition
            size_t cPos = data.find(" c ");
            if (cPos != std::string::npos) {
                std::string colorStr = data.substr(cPos + 3);
                // Trim whitespace
                colorStr.erase(0, colorStr.find_first_not_of(" \t"));
                colorStr.erase(colorStr.find_last_not_of(" \t") + 1);

                SDL_Color color = ParseColor(colorStr);
                colorMap[key] = color;
            }

            colorsRead++;
            continue;
        }

        // Parse pixel data
        if (pixelRowsRead < info.height) {
            pixels.push_back(data);
            pixelRowsRead++;
        }

        // Check if we're done
        if (pixelRowsRead >= info.height) {
            break;
        }
    }

    file.close();
    return readHeader && (colorsRead == info.numColors) && (pixelRowsRead == info.height);
}

SDL_Color XPMLoader::ParseColor(const std::string& colorStr) {
    SDL_Color color = {0, 0, 0, 255};

    if (colorStr == "None" || colorStr == "none") {
        color.a = 0; // Transparent
        return color;
    }

    if (colorStr[0] == '#') {
        // Hex color
        return HexToColor(colorStr);
    }

    // Named color - implement basic ones
    if (colorStr == "black") return {0, 0, 0, 255};
    if (colorStr == "white") return {255, 255, 255, 255};
    if (colorStr == "red") return {255, 0, 0, 255};
    if (colorStr == "green") return {0, 255, 0, 255};
    if (colorStr == "blue") return {0, 0, 255, 255};
    if (colorStr == "yellow") return {255, 255, 0, 255};
    if (colorStr == "cyan") return {0, 255, 255, 255};
    if (colorStr == "magenta") return {255, 0, 255, 255};
    if (colorStr == "gray" || colorStr == "grey") return {128, 128, 128, 255};

    // Default to black if unknown
    return {0, 0, 0, 255};
}

SDL_Color XPMLoader::HexToColor(const std::string& hex) {
    SDL_Color color = {0, 0, 0, 255};

    if (hex.length() < 7) return color;

    try {
        std::string r = hex.substr(1, 2);
        std::string g = hex.substr(3, 2);
        std::string b = hex.substr(5, 2);

        color.r = std::stoi(r, nullptr, 16);
        color.g = std::stoi(g, nullptr, 16);
        color.b = std::stoi(b, nullptr, 16);
    } catch (...) {
        // Return black on error
    }

    return color;
}