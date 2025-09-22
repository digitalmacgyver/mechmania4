/* SDL2Graphics.h
 * Modern SDL2-based graphics system for MechMania IV
 * Replaces X11/Xlib graphics
 */

#ifndef _SDL2GRAPHICS_H_
#define _SDL2GRAPHICS_H_

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <map>
#include <memory>
#include "stdafx.h"

// Color structure
struct Color {
    Uint8 r, g, b, a;
    Color(Uint8 r = 0, Uint8 g = 0, Uint8 b = 0, Uint8 a = 255)
        : r(r), g(g), b(b), a(a) {}
};

class SDL2Graphics {
private:
    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;
    TTF_Font* smallFont;
    TTF_Font* boldFont;
    TTF_Font* boldSmallFont;

    // Screen dimensions
    int displayWidth;
    int displayHeight;
    int spaceWidth;
    int spaceHeight;
    int borderX;
    int borderY;

    // Colors
    Color black;
    Color white;
    Color gray;
    Color red;
    Color lasCol;
    Color fuelCol;
    Color vinylCol;
    Color teamColors[6];

    // Textures for double buffering
    SDL_Texture* canvas;
    SDL_Texture* spaceCanvas;

    // Image cache
    std::map<std::string, SDL_Texture*> imageCache;

    // Helper functions
    SDL_Color ColorToSDL(const Color& c) const;
    void SetDrawColor(const Color& c);

public:
    SDL2Graphics();
    ~SDL2Graphics();

    // Initialization
    bool Init(int width = 0, int height = 0, bool fullscreen = false);
    void Cleanup();

    // Window management
    void SetWindowTitle(const std::string& title);
    void Clear(const Color& color = Color(0, 0, 0));
    void Present();

    // Drawing primitives
    void DrawPixel(int x, int y, const Color& color);
    void DrawLine(int x1, int y1, int x2, int y2, const Color& color);
    void DrawRect(int x, int y, int w, int h, const Color& color, bool filled = false);
    void DrawCircle(int cx, int cy, int radius, const Color& color, bool filled = false);
    void DrawArc(int cx, int cy, int radius, double startAngle, double endAngle, const Color& color);
    void DrawPolygon(const int* xPoints, const int* yPoints, int nPoints, const Color& color, bool filled = false);

    // Text rendering
    bool LoadFont(const std::string& fontPath, int size);
    // Prefer bold fonts by default to match original X11 "misc-fixed bold" look
    void DrawText(const std::string& text, int x, int y, const Color& color, bool small = false, bool bold = true);
    void GetTextSize(const std::string& text, int& w, int& h, bool small = false);
    // Extended: measure with bold selection to match DrawText rendering
    void GetTextSizeEx(const std::string& text, int& w, int& h, bool small = false, bool bold = false);

    // Image handling
    SDL_Texture* LoadImage(const std::string& path);
    SDL_Texture* LoadXPM(const char** xpmData);
    void DrawImage(SDL_Texture* image, int x, int y, double angle = 0.0, double scale = 1.0);
    void DrawImageClipped(SDL_Texture* image, int sx, int sy, int sw, int sh,
                         int dx, int dy, int dw, int dh);

    // Canvas/buffer operations
    void SetRenderTarget(SDL_Texture* target = nullptr);
    SDL_Texture* CreateTexture(int w, int h);
    void CopyTexture(SDL_Texture* src, SDL_Rect* srcRect, SDL_Rect* dstRect);

    // Event handling
    bool PollEvent(SDL_Event& event);

    // Color utilities
    Color GetPixelValue(const std::string& colorName);
    Uint32 MapRGB(Uint8 r, Uint8 g, Uint8 b);

    // Getters
    int GetDisplayWidth() const { return displayWidth; }
    int GetDisplayHeight() const { return displayHeight; }
    int GetSpaceWidth() const { return spaceWidth; }
    int GetSpaceHeight() const { return spaceHeight; }
    SDL_Renderer* GetRenderer() { return renderer; }
};

#endif // _SDL2GRAPHICS_H_
