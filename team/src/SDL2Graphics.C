/* SDL2Graphics.C
 * Implementation of SDL2-based graphics system
 */

#include "SDL2Graphics.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdlib>

SDL2Graphics::SDL2Graphics()
    : window(nullptr), renderer(nullptr), font(nullptr), smallFont(nullptr),
      boldFont(nullptr), boldSmallFont(nullptr),
      displayWidth(0), displayHeight(0), canvas(nullptr), spaceCanvas(nullptr) {

    // Initialize default colors to match X11 appearance
    black = Color(0, 0, 0);
    white = Color(255, 255, 255);
    gray = Color(160, 160, 160);  // Window background gray
    red = Color(255, 0, 0);
    lasCol = red;
    fuelCol = Color(0, 255, 0);
    vinylCol = Color(255, 0, 255);  // Magenta for vinyl asteroids

    // Team colors matching X11 original
    teamColors[0] = Color(0xFF, 0xB5, 0x73);     // Orange #FFB573
    teamColors[1] = Color(0x00, 0xC6, 0x8C);     // Teal #00C68C
    teamColors[2] = Color(0, 255, 0);            // Green (fallback)
    teamColors[3] = Color(255, 255, 0);          // Yellow (fallback)
    teamColors[4] = Color(255, 0, 255);          // Magenta (fallback)
    teamColors[5] = Color(0, 255, 255);          // Cyan (fallback)
}

SDL2Graphics::~SDL2Graphics() {
    Cleanup();
}

bool SDL2Graphics::Init(int width, int height, bool fullscreen) {
    // Check if we have a display available
    const char* display = std::getenv("DISPLAY");
    const char* sdl_driver = std::getenv("SDL_VIDEODRIVER");

    if (!display && !sdl_driver) {
        std::cerr << "No display available. Set SDL_VIDEODRIVER=dummy for headless mode" << std::endl;
        // Try to continue anyway with software/dummy driver
        SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
    }

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "SDL could not initialize! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Initialize SDL_image
    int imgFlags = IMG_INIT_PNG | IMG_INIT_JPG;
    if (!(IMG_Init(imgFlags) & imgFlags)) {
        std::cerr << "SDL_image could not initialize! SDL_image Error: " << IMG_GetError() << std::endl;
        return false;
    }

    // Initialize SDL_ttf
    if (TTF_Init() == -1) {
        std::cerr << "SDL_ttf could not initialize! SDL_ttf Error: " << TTF_GetError() << std::endl;
        return false;
    }

    // Get display dimensions if not specified
    if (width == 0 || height == 0) {
        // Use a reasonable default window size
        // that should fit on most monitors
        width = 1280;
        height = 960;
    }

    displayWidth = width;
    displayHeight = height;

    // Calculate space dimensions (70% of width)
    spaceWidth = static_cast<int>(displayWidth * 0.7);
    spaceHeight = spaceWidth;
    borderX = static_cast<int>(displayWidth * 0.015);
    borderY = static_cast<int>((displayHeight - spaceHeight) * 0.1);

    // Create window
    Uint32 windowFlags = SDL_WINDOW_SHOWN;
    if (fullscreen) {
        windowFlags |= SDL_WINDOW_FULLSCREEN;
    }

    window = SDL_CreateWindow(
        "MechMania IV: The Vinyl Frontier",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        displayWidth,
        displayHeight,
        windowFlags
    );

    if (!window) {
        std::cerr << "Window could not be created! SDL_Error: " << SDL_GetError() << std::endl;
        return false;
    }

    // Create renderer - try accelerated first, then software
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        // Try software renderer as fallback
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
        if (!renderer) {
            std::cerr << "Renderer could not be created! SDL_Error: " << SDL_GetError() << std::endl;
            return false;
        }
        std::cerr << "Warning: Using software renderer" << std::endl;
    }

    // Enable blending
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Create canvas textures
    canvas = CreateTexture(displayWidth, displayHeight);
    spaceCanvas = CreateTexture(spaceWidth, spaceHeight);

    // Load default font
    LoadFont("", 12);  // Will use default system font

    return true;
}

void SDL2Graphics::Cleanup() {
    // Clean up image cache
    for (auto& pair : imageCache) {
        if (pair.second) {
            SDL_DestroyTexture(pair.second);
        }
    }
    imageCache.clear();

    // Destroy textures
    if (canvas) {
        SDL_DestroyTexture(canvas);
        canvas = nullptr;
    }
    if (spaceCanvas) {
        SDL_DestroyTexture(spaceCanvas);
        spaceCanvas = nullptr;
    }

    // Close fonts
    if (font) {
        TTF_CloseFont(font);
        font = nullptr;
    }
    if (smallFont) {
        TTF_CloseFont(smallFont);
        smallFont = nullptr;
    }
    if (boldFont) {
        TTF_CloseFont(boldFont);
        boldFont = nullptr;
    }
    if (boldSmallFont) {
        TTF_CloseFont(boldSmallFont);
        boldSmallFont = nullptr;
    }

    // Destroy renderer and window
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }

    // Quit SDL subsystems
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void SDL2Graphics::SetWindowTitle(const std::string& title) {
    if (window) {
        SDL_SetWindowTitle(window, title.c_str());
    }
}

void SDL2Graphics::Clear(const Color& color) {
    SetDrawColor(color);
    SDL_RenderClear(renderer);
}

void SDL2Graphics::Present() {
    SDL_RenderPresent(renderer);
}

SDL_Color SDL2Graphics::ColorToSDL(const Color& c) const {
    SDL_Color sdlColor;
    sdlColor.r = c.r;
    sdlColor.g = c.g;
    sdlColor.b = c.b;
    sdlColor.a = c.a;
    return sdlColor;
}

void SDL2Graphics::SetDrawColor(const Color& c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
}

void SDL2Graphics::DrawPixel(int x, int y, const Color& color) {
    SetDrawColor(color);
    SDL_RenderDrawPoint(renderer, x, y);
}

void SDL2Graphics::DrawLine(int x1, int y1, int x2, int y2, const Color& color) {
    SetDrawColor(color);
    SDL_RenderDrawLine(renderer, x1, y1, x2, y2);
}

void SDL2Graphics::DrawRect(int x, int y, int w, int h, const Color& color, bool filled) {
    SDL_Rect rect = {x, y, w, h};
    SetDrawColor(color);
    if (filled) {
        SDL_RenderFillRect(renderer, &rect);
    } else {
        SDL_RenderDrawRect(renderer, &rect);
    }
}

void SDL2Graphics::DrawCircle(int cx, int cy, int radius, const Color& color, bool filled) {
    SetDrawColor(color);

    if (filled) {
        for (int w = 0; w < radius * 2; w++) {
            for (int h = 0; h < radius * 2; h++) {
                int dx = radius - w;
                int dy = radius - h;
                if ((dx*dx + dy*dy) <= (radius * radius)) {
                    SDL_RenderDrawPoint(renderer, cx + dx, cy + dy);
                }
            }
        }
    } else {
        // Bresenham's circle algorithm
        int x = radius;
        int y = 0;
        int err = 0;

        while (x >= y) {
            SDL_RenderDrawPoint(renderer, cx + x, cy + y);
            SDL_RenderDrawPoint(renderer, cx + y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - y, cy + x);
            SDL_RenderDrawPoint(renderer, cx - x, cy + y);
            SDL_RenderDrawPoint(renderer, cx - x, cy - y);
            SDL_RenderDrawPoint(renderer, cx - y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + y, cy - x);
            SDL_RenderDrawPoint(renderer, cx + x, cy - y);

            if (err <= 0) {
                y += 1;
                err += 2*y + 1;
            }
            if (err > 0) {
                x -= 1;
                err -= 2*x + 1;
            }
        }
    }
}

void SDL2Graphics::DrawArc(int cx, int cy, int radius, double startAngle, double endAngle, const Color& color) {
    SetDrawColor(color);

    // Convert angles to radians if needed
    double step = 0.01; // Angle step in radians

    for (double angle = startAngle; angle <= endAngle; angle += step) {
        int x = cx + static_cast<int>(radius * cos(angle));
        int y = cy + static_cast<int>(radius * sin(angle));
        SDL_RenderDrawPoint(renderer, x, y);
    }
}

void SDL2Graphics::DrawPolygon(const int* xPoints, const int* yPoints, int nPoints, const Color& color, bool filled) {
    if (nPoints < 3) return;

    SetDrawColor(color);

    if (!filled) {
        // Draw outline
        for (int i = 0; i < nPoints; i++) {
            int next = (i + 1) % nPoints;
            SDL_RenderDrawLine(renderer, xPoints[i], yPoints[i], xPoints[next], yPoints[next]);
        }
    } else {
        // Simple scanline fill (not optimal but works)
        int minY = yPoints[0], maxY = yPoints[0];
        for (int i = 1; i < nPoints; i++) {
            minY = std::min(minY, yPoints[i]);
            maxY = std::max(maxY, yPoints[i]);
        }

        for (int y = minY; y <= maxY; y++) {
            int intersections[100];
            int count = 0;

            for (int i = 0; i < nPoints && count < 100; i++) {
                int next = (i + 1) % nPoints;
                int y1 = yPoints[i];
                int y2 = yPoints[next];

                if ((y1 <= y && y2 > y) || (y2 <= y && y1 > y)) {
                    int x1 = xPoints[i];
                    int x2 = xPoints[next];
                    int x = x1 + (y - y1) * (x2 - x1) / (y2 - y1);
                    intersections[count++] = x;
                }
            }

            // Sort intersections
            std::sort(intersections, intersections + count);

            // Fill between pairs
            for (int i = 0; i < count - 1; i += 2) {
                SDL_RenderDrawLine(renderer, intersections[i], y, intersections[i + 1], y);
            }
        }
    }
}

bool SDL2Graphics::LoadFont(const std::string& fontPath, int size) {
    // Try to load specified font, fall back to default
    std::string path = fontPath;
    if (path.empty()) {
        // Try common font paths on Linux
        const char* fontPaths[] = {
            "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
            "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
            "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
            "/usr/share/fonts/truetype/freefont/FreeMono.ttf",
            nullptr
        };

        for (int i = 0; fontPaths[i] != nullptr; i++) {
            if (SDL_RWFromFile(fontPaths[i], "r")) {
                path = fontPaths[i];
                break;
            }
        }
    }

    if (!path.empty()) {
        font = TTF_OpenFont(path.c_str(), size);
        smallFont = TTF_OpenFont(path.c_str(), size - 2);

        // Load bold variants
        boldFont = TTF_OpenFont(path.c_str(), size);
        if (boldFont) {
            TTF_SetFontStyle(boldFont, TTF_STYLE_BOLD);
        }

        boldSmallFont = TTF_OpenFont(path.c_str(), size - 2);
        if (boldSmallFont) {
            TTF_SetFontStyle(boldSmallFont, TTF_STYLE_BOLD);
        }
    }

    return font != nullptr;
}

void SDL2Graphics::DrawText(const std::string& text, int x, int y, const Color& color, bool small, bool bold) {
    if (text.empty()) return;

    TTF_Font* useFont;
    if (bold) {
        useFont = small ? (boldSmallFont ? boldSmallFont : boldFont) : boldFont;
    } else {
        useFont = small ? (smallFont ? smallFont : font) : font;
    }

    if (!useFont) {
        // Fallback to regular font if bold not available
        useFont = small ? (smallFont ? smallFont : font) : font;
    }
    if (!useFont) return;

    SDL_Color sdlColor = ColorToSDL(color);
    SDL_Surface* surface = TTF_RenderText_Solid(useFont, text.c_str(), sdlColor);
    if (!surface) return;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_Rect dstRect = {x, y, surface->w, surface->h};
        SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
        SDL_DestroyTexture(texture);
    }

    SDL_FreeSurface(surface);
}

void SDL2Graphics::GetTextSize(const std::string& text, int& w, int& h, bool small) {
    TTF_Font* useFont = small ? (smallFont ? smallFont : font) : font;
    if (!useFont) {
        w = h = 0;
        return;
    }

    TTF_SizeText(useFont, text.c_str(), &w, &h);
}

SDL_Texture* SDL2Graphics::LoadImage(const std::string& path) {
    // Check cache first
    auto it = imageCache.find(path);
    if (it != imageCache.end()) {
        return it->second;
    }

    SDL_Surface* surface = IMG_Load(path.c_str());
    if (!surface) {
        std::cerr << "Failed to load image: " << path << " Error: " << IMG_GetError() << std::endl;
        return nullptr;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);

    if (texture) {
        imageCache[path] = texture;
    }

    return texture;
}

SDL_Texture* SDL2Graphics::LoadXPM(const char** xpmData) {
    // SDL2 doesn't have native XPM support, but SDL_image can load XPM from memory
    // For now, return nullptr - we'll implement XPM conversion if needed
    return nullptr;
}

void SDL2Graphics::DrawImage(SDL_Texture* image, int x, int y, double angle, double scale) {
    if (!image) return;

    int w, h;
    SDL_QueryTexture(image, nullptr, nullptr, &w, &h);

    SDL_Rect dstRect = {
        x - static_cast<int>(w * scale / 2),
        y - static_cast<int>(h * scale / 2),
        static_cast<int>(w * scale),
        static_cast<int>(h * scale)
    };

    SDL_RenderCopyEx(renderer, image, nullptr, &dstRect, angle * 180.0 / PI, nullptr, SDL_FLIP_NONE);
}

void SDL2Graphics::DrawImageClipped(SDL_Texture* image, int sx, int sy, int sw, int sh,
                                   int dx, int dy, int dw, int dh) {
    if (!image) return;

    SDL_Rect srcRect = {sx, sy, sw, sh};
    SDL_Rect dstRect = {dx, dy, dw, dh};

    SDL_RenderCopy(renderer, image, &srcRect, &dstRect);
}

void SDL2Graphics::SetRenderTarget(SDL_Texture* target) {
    SDL_SetRenderTarget(renderer, target);
}

SDL_Texture* SDL2Graphics::CreateTexture(int w, int h) {
    SDL_Texture* texture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_TARGET,
        w, h
    );

    if (texture) {
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }

    return texture;
}

void SDL2Graphics::CopyTexture(SDL_Texture* src, SDL_Rect* srcRect, SDL_Rect* dstRect) {
    SDL_RenderCopy(renderer, src, srcRect, dstRect);
}

bool SDL2Graphics::PollEvent(SDL_Event& event) {
    return SDL_PollEvent(&event) == 1;
}

Color SDL2Graphics::GetPixelValue(const std::string& colorName) {
    // Basic color name parsing
    if (colorName == "black") return black;
    if (colorName == "white") return white;
    if (colorName == "gray" || colorName == "grey") return gray;
    if (colorName == "red") return red;
    if (colorName == "green") return Color(0, 255, 0);
    if (colorName == "blue") return Color(0, 0, 255);
    if (colorName == "yellow") return Color(255, 255, 0);
    if (colorName == "cyan") return Color(0, 255, 255);
    if (colorName == "magenta") return Color(255, 0, 255);

    // Handle hex colors
    if (colorName[0] == '#' && colorName.length() >= 7) {
        int r = std::stoi(colorName.substr(1, 2), nullptr, 16);
        int g = std::stoi(colorName.substr(3, 2), nullptr, 16);
        int b = std::stoi(colorName.substr(5, 2), nullptr, 16);
        return Color(r, g, b);
    }

    return gray;  // Default
}

Uint32 SDL2Graphics::MapRGB(Uint8 r, Uint8 g, Uint8 b) {
    SDL_PixelFormat* format = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA8888);
    Uint32 pixel = SDL_MapRGB(format, r, g, b);
    SDL_FreeFormat(format);
    return pixel;
}