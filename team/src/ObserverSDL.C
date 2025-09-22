/* ObserverSDL.C
 * SDL2-based Observer implementation
 */

#include "ObserverSDL.h"
#include "XPMLoader.h"  // For loading logo
#include "Station.h"
#include "Ship.h"
#include "Asteroid.h"
#include "Thing.h"    // For NO_DAMAGE
#include "World.h"    // For BAD_INDEX
#include <iostream>
#include <algorithm>  // For std::min
#include <cmath>      // For cos, sin
#include <iomanip>
#include <sstream>

ObserverSDL::ObserverSDL(const char* regFileName, int gfxFlag)
    : graphics(nullptr), spriteManager(nullptr), myWorld(nullptr),
      logoTexture(nullptr), attractor(0), useSpriteMode(gfxFlag == 1) {  // Enable sprite mode when -G is used

    drawnames = 1;

    if (gfxFlag == 1) {
        useXpm = true;
        useVelVectors = false;  // No tactical display
    } else {
        useXpm = false;
        useVelVectors = true;   // Tactical display
    }

    graphics = new SDL2Graphics();
}

ObserverSDL::~ObserverSDL() {
    if (logoTexture) {
        SDL_DestroyTexture(logoTexture);
    }
    if (graphics) {
        delete graphics;
    }
    if (spriteManager) {
        delete spriteManager;
    }
}

bool ObserverSDL::Initialize() {
    if (!graphics->Init()) {
        std::cerr << "Failed to initialize SDL2 graphics" << std::endl;
        return false;
    }

    // Initialize sprite manager
    spriteManager = new SpriteManager(graphics->GetRenderer());
    if (!spriteManager->LoadSprites("graphics.reg")) {
        std::cerr << "Warning: Failed to load sprites, sprite mode disabled" << std::endl;
        useSpriteMode = false;
    }

    // Load MM4 Logo for between-match display
    logoTexture = XPMLoader::LoadXPM(graphics->GetRenderer(), "gfx/MM4Logo.xpm");
    if (!logoTexture) {
        std::cerr << "Warning: Failed to load MM4Logo.xpm" << std::endl;
    }

    // Get dimensions from graphics
    int displayWidth = graphics->GetDisplayWidth();
    int displayHeight = graphics->GetDisplayHeight();

    // Calculate layout
    spaceWidth = graphics->GetSpaceWidth();
    spaceHeight = graphics->GetSpaceHeight();
    borderX = static_cast<int>(displayWidth * 0.015);
    borderY = static_cast<int>((displayHeight - spaceHeight) * 0.1);

    // Message area
    msgPosX = 2 * borderX + spaceWidth;
    msgPosY = borderY;
    msgWidth = displayWidth - msgPosX - borderX;
    msgHeight = spaceHeight / 3;

    // Time display
    timeX = msgPosX;
    timeY = msgPosY + msgHeight + borderY;
    timeWidth = msgWidth;
    timeHeight = 50;

    // Team info positions (in right panel)
    t1PosX = msgPosX;
    t1PosY = timeY + timeHeight + borderY;
    t2PosX = msgPosX;
    t2PosY = t1PosY + 200;  // More space for first team's info with ship details

    // Initialize message buffer
    messageBuffer.resize(MSG_ROWS);

    return true;
}

void ObserverSDL::Update() {
    // Update world state would happen here
    // For now, just placeholder
}

void ObserverSDL::Draw() {
    // Clear screen with X11-style gray background
    graphics->Clear(Color(160, 160, 160));  // Gray #A0A0A0

    // Draw main components
    DrawStarfield();
    DrawSpace();

    if (myWorld) {
        // Draw all things in the world
        // Iterate through all things using the linked list
        for (UINT i = myWorld->UFirstIndex; i != BAD_INDEX; i = myWorld->GetNextIndex(i)) {
            CThing* thing = myWorld->GetThing(i);
            if (thing) {
                DrawThing(thing);
            }
        }

        // Draw laser beams for all ships
        for (UINT t = 0; t < myWorld->GetNumTeams(); t++) {
            CTeam* team = myWorld->GetTeam(t);
            if (team) {
                for (UINT s = 0; s < team->GetShipCount(); s++) {
                    CShip* ship = team->GetShip(s);
                    if (ship && ship->IsAlive()) {
                        DrawLaserBeam(ship, t);
                    }
                }
            }
        }

        // Draw team info
        for (UINT t = 0; t < myWorld->GetNumTeams(); t++) {
            CTeam* team = myWorld->GetTeam(t);
            if (team) {
                int x = (t == 0) ? t1PosX : t2PosX;
                int y = (t == 0) ? t1PosY : t2PosY;
                DrawTeamInfo(team, x, y);
            }
        }
    }

    DrawMessages();
    DrawTimeDisplay();

    // Draw logo overlay if attractor mode is active
    if (attractor > 0) {
        DrawLogo();
    }

    DrawHelpFooter();

    // Present everything
    graphics->Present();
}

bool ObserverSDL::HandleEvents() {
    SDL_Event event;
    while (graphics->PollEvent(event)) {
        switch (event.type) {
            case SDL_QUIT:
                return false;

            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                    case SDLK_q:
                        return false;
                    case SDLK_n:
                        drawnames = !drawnames;
                        break;
                    case SDLK_v:
                        ToggleVelVectors();
                        break;
                    case SDLK_g:
                        ToggleSpriteMode();
                        std::cout << "Sprite mode: " << (useSpriteMode ? "ON" : "OFF") << std::endl;
                        break;
                    case SDLK_SPACE:
                        attractor = (attractor + 1) % 3;
                        std::cout << "Logo mode: " << (attractor ? "ON" : "OFF")
                                  << " (level " << attractor << ")" << std::endl;
                        break;
                }
                break;
        }
    }
    return true;
}

void ObserverSDL::DrawSpace() {
    // Draw space border
    graphics->DrawRect(borderX, borderY, spaceWidth, spaceHeight,
                      Color(100, 100, 100), false);

    // Draw grid if needed
    if (useVelVectors) {
        Color gridColor(60, 60, 60);
        int gridStep = spaceWidth / 8;  // 8x8 grid

        for (int i = 1; i < 8; i++) {
            int x = borderX + i * gridStep;
            graphics->DrawLine(x, borderY,
                             x, borderY + spaceHeight,
                             gridColor);
        }
        for (int i = 1; i < 8; i++) {
            int y = borderY + i * gridStep;
            graphics->DrawLine(borderX, y,
                             borderX + spaceWidth, y,
                             gridColor);
        }

        // Draw center lines slightly brighter
        Color centerColor(80, 80, 80);
        int centerX = borderX + spaceWidth / 2;
        int centerY = borderY + spaceHeight / 2;
        graphics->DrawLine(centerX, borderY,
                         centerX, borderY + spaceHeight,
                         centerColor);
        graphics->DrawLine(borderX, centerY,
                         borderX + spaceWidth, centerY,
                         centerColor);

        // Draw origin marker
        graphics->DrawCircle(centerX, centerY, 3, Color(100, 100, 100), false);
    }
}

void ObserverSDL::DrawStarfield() {
    // Simple starfield
    static bool starsInit = false;
    static std::vector<std::pair<int, int>> stars;

    if (!starsInit) {
        for (int i = 0; i < 200; i++) {
            int x = rand() % spaceWidth;
            int y = rand() % spaceHeight;
            stars.push_back({x, y});
        }
        starsInit = true;
    }

    Color starColor(180, 180, 180);
    for (const auto& star : stars) {
        graphics->DrawPixel(borderX + star.first, borderY + star.second, starColor);
    }
}

void ObserverSDL::DrawThing(CThing* thing) {
    if (!thing) return;

    const CCoord& pos = thing->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    // Determine what type of thing this is
    CShip* ship = dynamic_cast<CShip*>(thing);
    if (ship && ship->GetTeam()) {
        DrawShip(ship, ship->GetTeam()->GetTeamNumber());
        return;
    }

    CStation* station = dynamic_cast<CStation*>(thing);
    if (station && station->GetTeam()) {
        DrawStation(station, station->GetTeam()->GetTeamNumber());
        return;
    }

    CAsteroid* asteroid = dynamic_cast<CAsteroid*>(thing);
    if (asteroid) {
        DrawAsteroid(asteroid);
        return;
    }

    // Default thing drawing
    Color color = GetThingColor(thing);
    graphics->DrawCircle(x, y, static_cast<int>(thing->GetSize()), color, false);

    if (drawnames) {
        graphics->DrawText(thing->GetName(), x + 10, y - 10, color, true);
    }
}

void ObserverSDL::DrawShip(CShip* ship, int teamNum) {
    if (!ship) return;

    // Use sprite mode if enabled and sprites are loaded
    if (useSpriteMode && spriteManager && spriteManager->IsLoaded()) {
        DrawShipSprite(ship, teamNum);
        return;
    }

    const CCoord& pos = ship->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);
    double orient = ship->GetOrient();

    Color color = GetTeamColor(teamNum);

    // Draw ship as triangle
    int size = 8;
    int x1 = x + static_cast<int>(size * cos(orient));
    int y1 = y + static_cast<int>(size * sin(orient));
    int x2 = x + static_cast<int>(size * cos(orient + 2.4));
    int y2 = y + static_cast<int>(size * sin(orient + 2.4));
    int x3 = x + static_cast<int>(size * cos(orient - 2.4));
    int y3 = y + static_cast<int>(size * sin(orient - 2.4));

    int xPoints[] = {x1, x2, x3};
    int yPoints[] = {y1, y2, y3};
    graphics->DrawPolygon(xPoints, yPoints, 3, color, true);

    // Draw velocity vector if enabled
    if (useVelVectors) {
        const CTraj& vel = ship->GetVelocity();
        CCoord velCoord = vel.ConvertToCoord();
        int vx = x + static_cast<int>(velCoord.fX * 2);
        int vy = y + static_cast<int>(velCoord.fY * 2);
        graphics->DrawLine(x, y, vx, vy, Color(0, 255, 0));
    }

    // Draw name
    if (drawnames) {
        char info[64];
        snprintf(info, sizeof(info), "S%d",
                ship->GetWorldIndex());  // No health info available
        graphics->DrawText(info, x + 10, y - 10, color, true);
    }
}

void ObserverSDL::DrawLaserBeam(CShip* ship, int teamNum) {
    if (!ship) return;

    double laserRange = ship->GetLaserBeamDistance();
    if (laserRange <= 0.0) return;  // No laser active

    const CCoord& pos = ship->GetPos();
    double orient = ship->GetOrient();

    // Calculate laser end point
    double laserEndX = pos.fX + (laserRange * cos(orient));
    double laserEndY = pos.fY + (laserRange * sin(orient));

    // Convert to screen coordinates
    int startX = WorldToScreenX(pos.fX);
    int startY = WorldToScreenY(pos.fY);
    int endX = WorldToScreenX(laserEndX);
    int endY = WorldToScreenY(laserEndY);

    // Draw laser beam with team color but brighter
    Color laserColor = GetTeamColor(teamNum);
    // Make laser brighter by increasing all components
    laserColor.r = std::min(255, laserColor.r + 100);
    laserColor.g = std::min(255, laserColor.g + 100);
    laserColor.b = std::min(255, laserColor.b + 100);

    // Draw the laser beam (could make it thicker by drawing multiple lines)
    graphics->DrawLine(startX, startY, endX, endY, laserColor);

    // Optional: Draw a brighter center line
    Color brightCore(255, 255, 200);  // Yellow-white core
    graphics->DrawLine(startX, startY, endX, endY, brightCore);
}

void ObserverSDL::DrawStation(CStation* station, int teamNum) {
    if (!station) return;

    // Use sprite mode if enabled and sprites are loaded
    if (useSpriteMode && spriteManager && spriteManager->IsLoaded()) {
        DrawStationSprite(station, teamNum);
        return;
    }

    const CCoord& pos = station->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    Color color = GetTeamColor(teamNum);

    // Draw station as square
    int size = 15;
    graphics->DrawRect(x - size, y - size, size * 2, size * 2, color, true);
    graphics->DrawRect(x - size - 2, y - size - 2, size * 2 + 4, size * 2 + 4, color, false);

    if (drawnames) {
        const char* stationName = station->GetName();
        if (stationName && strlen(stationName) > 0) {
            graphics->DrawText(stationName, x + 20, y - 20, color, true);
        } else {
            graphics->DrawText("Station", x + 20, y - 20, color, true);
        }
    }
}

void ObserverSDL::DrawAsteroid(CAsteroid* asteroid) {
    if (!asteroid) return;

    // Use sprite mode if enabled and sprites are loaded
    if (useSpriteMode && spriteManager && spriteManager->IsLoaded()) {
        DrawAsteroidSprite(asteroid);
        return;
    }

    const CCoord& pos = asteroid->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    // Determine color based on type - match X11 colors
    Color color;
    if (asteroid->GetMaterial() == URANIUM) {
        color = Color(0, 255, 0);  // Green #00FF00
    } else if (asteroid->GetMaterial() == VINYL) {
        color = Color(255, 0, 255); // Magenta #FF00FF
    } else {
        color = Color(128, 128, 128);
    }

    int radius = static_cast<int>(asteroid->GetSize());
    graphics->DrawCircle(x, y, radius, color, true);

    if (drawnames) {
        const char* typeName = (asteroid->GetMaterial() == URANIUM) ? "U" : "V";
        graphics->DrawText(typeName, x - 5, y - 5, Color(255, 255, 255), false);
    }
}

void ObserverSDL::DrawTeamInfo(CTeam* team, int x, int y) {
    if (!team) return;

    Color color = GetTeamColor(team->GetTeamNumber());
    int lineHeight = 15;
    int currentY = y;

    // Draw team name
    graphics->DrawText(team->GetName(), x, currentY, color, false);
    currentY += lineHeight + 5;

    // Draw score and vinyl at station
    char info[256];
    CStation* station = team->GetStation();
    if (station) {
        snprintf(info, sizeof(info), "Score: %.0f  Vinyl: %.1f",
                team->GetScore(), station->GetVinylStore());
        graphics->DrawText(info, x, currentY, color, true);
        currentY += lineHeight;
    } else {
        snprintf(info, sizeof(info), "Score: %.0f", team->GetScore());
        graphics->DrawText(info, x, currentY, color, true);
        currentY += lineHeight;
    }

    // Draw ship count
    snprintf(info, sizeof(info), "Ships: %d", team->GetShipCount());
    graphics->DrawText(info, x, currentY, color, true);
    currentY += lineHeight + 3;

    // Draw detailed info for each ship
    for (UINT i = 0; i < team->GetShipCount() && i < 4; i++) {
        CShip* ship = team->GetShip(i);
        if (ship && ship->IsAlive()) {
            // Ship name and position
            const CCoord& pos = ship->GetPos();
            const char* shipName = ship->GetName();
            if (shipName && strlen(shipName) > 0) {
                snprintf(info, sizeof(info), "%s: (%.0f,%.0f)",
                        shipName, pos.fX, pos.fY);
            } else {
                snprintf(info, sizeof(info), "Ship %d: (%.0f,%.0f)",
                        i+1, pos.fX, pos.fY);
            }
            graphics->DrawText(info, x, currentY, color, true);
            currentY += lineHeight;

            // Ship resources
            double fuel = ship->GetAmount(S_FUEL);
            double fuelMax = ship->GetCapacity(S_FUEL);
            double cargo = ship->GetAmount(S_CARGO);
            double cargoMax = ship->GetCapacity(S_CARGO);
            double shield = ship->GetAmount(S_SHIELD);

            // Format fuel with color based on threshold
            char fuelStr[32];
            double fuelPercent = (fuelMax > 0) ? (fuel / fuelMax) * 100.0 : 0;
            Color fuelColor;
            if (fuelPercent > 50.0) {
                fuelColor = Color(0, 255, 0);  // Green
            } else if (fuelPercent >= 20.0) {
                fuelColor = Color(255, 255, 0);  // Yellow
            } else {
                fuelColor = Color(255, 0, 0);  // Red
            }
            snprintf(fuelStr, sizeof(fuelStr), "F:%.1f/%.0f", fuel, fuelMax);

            // Format shield with color based on threshold
            char shieldStr[32];
            Color shieldColor;
            if (shield > 12.5) {
                shieldColor = Color(0, 255, 0);  // Green
            } else if (shield >= 5.0) {
                shieldColor = Color(255, 255, 0);  // Yellow
            } else {
                shieldColor = Color(255, 0, 0);  // Red
            }
            snprintf(shieldStr, sizeof(shieldStr), "S:%.1f", shield);

            // Draw fuel, cargo, and shield separately with appropriate colors
            graphics->DrawText("  ", x, currentY, color, true);
            graphics->DrawText(fuelStr, x + 20, currentY, fuelColor, true);

            char cargoStr[32];
            snprintf(cargoStr, sizeof(cargoStr), " V:%.1f/%.0f ", cargo, cargoMax);
            graphics->DrawText(cargoStr, x + 100, currentY, color, true);

            graphics->DrawText(shieldStr, x + 180, currentY, shieldColor, true);
            currentY += lineHeight + 2;
        }
    }

    // Display team messages if any
    if (team->MsgText[0] != '\0') {
        // Process and add messages to the message buffer
        char* msg = team->MsgText;
        char line[256];
        int linePos = 0;

        for (int i = 0; i < maxTextLen && msg[i] != '\0'; i++) {
            if (msg[i] == '\n' || linePos >= 255) {
                line[linePos] = '\0';
                if (linePos > 0) {
                    AddMessage(std::string(team->GetName()) + ": " + line);
                }
                linePos = 0;
            } else {
                line[linePos++] = msg[i];
            }
        }
        // Add any remaining text
        if (linePos > 0) {
            line[linePos] = '\0';
            AddMessage(std::string(team->GetName()) + ": " + line);
        }

        // Clear the team's message buffer after displaying
        team->MsgText[0] = '\0';
    }
}

void ObserverSDL::DrawMessages() {
    int y = msgPosY;
    Color msgColor(200, 200, 200);

    // Calculate max characters per line based on available width
    int charWidth = 7;  // Approximate character width for small font
    int maxCharsPerLine = msgWidth / charWidth;

    for (const auto& msg : messageBuffer) {
        if (!msg.empty()) {
            // Word wrap long messages
            if (msg.length() > maxCharsPerLine) {
                std::string line = msg;
                size_t pos = 0;

                while (pos < line.length() && y < (msgPosY + msgHeight - 15)) {
                    size_t lineEnd = pos + maxCharsPerLine;

                    // If we're not at the end, try to break at a word boundary
                    if (lineEnd < line.length()) {
                        size_t lastSpace = line.find_last_of(" ", lineEnd);
                        if (lastSpace != std::string::npos && lastSpace > pos) {
                            lineEnd = lastSpace;
                        }
                    } else {
                        lineEnd = line.length();
                    }

                    std::string segment = line.substr(pos, lineEnd - pos);
                    graphics->DrawText(segment, msgPosX, y, msgColor, true);
                    y += 15;

                    pos = lineEnd;
                    // Skip the space if we broke at one
                    if (pos < line.length() && line[pos] == ' ') {
                        pos++;
                    }
                }
            } else {
                graphics->DrawText(msg, msgPosX, y, msgColor, true);
                y += 15;
            }
        }
    }
}

void ObserverSDL::DrawTimeDisplay() {
    if (!myWorld) return;

    char timeStr[64];
    double gameTime = myWorld->GetGameTime();
    int minutes = static_cast<int>(gameTime / 60);
    int seconds = static_cast<int>(gameTime) % 60;

    snprintf(timeStr, sizeof(timeStr), "Time: %02d:%02d", minutes, seconds);
    graphics->DrawText(timeStr, timeX, timeY, Color(0, 0, 0), false);  // Black text
}

int ObserverSDL::WorldToScreenX(double wx) {
    // World coordinates are from -512 to 512
    // Map to screen coordinates (0 to spaceWidth)
    double normalized = (wx + 512.0) / 1024.0;  // Normalize to 0-1
    return borderX + static_cast<int>(normalized * spaceWidth);
}

int ObserverSDL::WorldToScreenY(double wy) {
    // World coordinates are from -512 to 512
    // Map to screen coordinates (0 to spaceHeight)
    double normalized = (wy + 512.0) / 1024.0;  // Normalize to 0-1
    return borderY + static_cast<int>(normalized * spaceHeight);
}

double ObserverSDL::ScreenToWorldX(int sx) {
    double normalized = (sx - borderX) / static_cast<double>(spaceWidth);
    return (normalized * 1024.0) - 512.0;
}

double ObserverSDL::ScreenToWorldY(int sy) {
    double normalized = (sy - borderY) / static_cast<double>(spaceHeight);
    return (normalized * 1024.0) - 512.0;
}

Color ObserverSDL::GetTeamColor(int teamNum) {
    switch (teamNum % 6) {
        case 0: return Color(0xFF, 0xB5, 0x73);  // Orange #FFB573
        case 1: return Color(0x00, 0xC6, 0x8C);  // Teal #00C68C
        case 2: return Color(0, 255, 0);         // Green
        case 3: return Color(255, 255, 0);       // Yellow
        case 4: return Color(255, 0, 255);       // Magenta
        case 5: return Color(0, 255, 255);       // Cyan
        default: return Color(255, 255, 255);    // White
    }
}

Color ObserverSDL::GetThingColor(CThing* thing) {
    if (!thing) return Color(128, 128, 128);

    // Could check thing type or properties here
    return Color(200, 200, 200);
}

void ObserverSDL::AddMessage(const std::string& msg) {
    // Shift messages up
    for (size_t i = 0; i < messageBuffer.size() - 1; i++) {
        messageBuffer[i] = messageBuffer[i + 1];
    }
    messageBuffer.back() = msg;
}

void ObserverSDL::ClearMessages() {
    for (auto& msg : messageBuffer) {
        msg.clear();
    }
}

void ObserverSDL::DrawLogo() {
    if (!logoTexture) return;

    // Get logo dimensions
    int logoW, logoH;
    SDL_QueryTexture(logoTexture, nullptr, nullptr, &logoW, &logoH);

    if (attractor == 2) {
        // Mode 2: Opaque logo covering entire window
        int displayWidth = graphics->GetDisplayWidth();
        int displayHeight = graphics->GetDisplayHeight();

        // Scale logo to cover entire window while maintaining aspect ratio
        float scaleX = (float)displayWidth / logoW;
        float scaleY = (float)displayHeight / logoH;
        float scale = std::max(scaleX, scaleY);  // Use max to ensure full coverage

        int scaledW = (int)(logoW * scale);
        int scaledH = (int)(logoH * scale);

        // Center the scaled logo
        int x = (displayWidth - scaledW) / 2;
        int y = (displayHeight - scaledH) / 2;

        // Set to fully opaque
        SDL_SetTextureAlphaMod(logoTexture, 255);

        // Draw the logo covering the whole window
        SDL_Rect dest = {x, y, scaledW, scaledH};
        SDL_RenderCopy(graphics->GetRenderer(), logoTexture, nullptr, &dest);
    } else if (attractor == 1) {
        // Mode 1: Semi-transparent overlay on space canvas
        int x = borderX + (spaceWidth - logoW) / 2;
        int y = borderY + (spaceHeight - logoH) / 2;

        // Set to semi-transparent
        SDL_SetTextureAlphaMod(logoTexture, 128);

        // Draw the logo
        SDL_Rect dest = {x, y, logoW, logoH};
        SDL_RenderCopy(graphics->GetRenderer(), logoTexture, nullptr, &dest);
    }

    // Reset alpha mod
    SDL_SetTextureAlphaMod(logoTexture, 255);
}

void ObserverSDL::DrawHelpFooter() {
    // Draw a semi-transparent background for the footer
    int footerHeight = 25;
    int displayWidth = graphics->GetDisplayWidth();
    int displayHeight = graphics->GetDisplayHeight();
    int footerY = displayHeight - footerHeight;

    // Draw background bar
    SDL_Rect footerRect = {0, footerY, displayWidth, footerHeight};
    SDL_SetRenderDrawColor(graphics->GetRenderer(), 30, 30, 30, 200);
    SDL_SetRenderDrawBlendMode(graphics->GetRenderer(), SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(graphics->GetRenderer(), &footerRect);

    // Draw help text
    Color helpColor(200, 200, 200);
    int textY = footerY + 5;
    int spacing = 140;
    int x = 10;

    // Key instructions
    graphics->DrawText("[G] Sprite Mode", x, textY, helpColor, true);
    x += spacing;
    graphics->DrawText("[N] Toggle Names", x, textY, helpColor, true);
    x += spacing;
    graphics->DrawText("[V] Velocity Vectors", x, textY, helpColor, true);
    x += spacing + 20;
    graphics->DrawText("[Space] Logo Toggle", x, textY, helpColor, true);
    x += spacing + 30;
    graphics->DrawText("[ESC/Q] Quit", x, textY, helpColor, true);

    // Show current sprite mode status
    x = displayWidth - 150;
    if (useSpriteMode) {
        graphics->DrawText("Sprites: ON", x, textY, Color(0, 255, 0), true);
    } else {
        graphics->DrawText("Sprites: OFF", x, textY, Color(150, 150, 150), true);
    }
}

void ObserverSDL::DrawShipSprite(CShip* ship, int teamNum) {
    if (!ship || !spriteManager) return;

    const CCoord& pos = ship->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);
    double orient = ship->GetOrient();

    // Get image set from ship (0=normal, 1=thrust, 2=brake, 3=left, 4=right)
    int imageSet = ship->GetImage();

    // Get the appropriate sprite - use actual team number, not passed teamNum
    int actualTeamNum = ship->GetTeam()->GetTeamNumber();
    SDL_Texture* sprite = spriteManager->GetShipSprite(
        actualTeamNum, imageSet, orient);

    if (sprite) {
        // Draw sprite centered at ship position
        SDL_Rect dest = {x - 16, y - 16, 32, 32};  // Assuming 32x32 sprites
        SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
    }

    // Draw damage overlays if being hit
    if (ship->bIsColliding != NO_DAMAGE) {
        // Draw collision impact overlay
        int frame = spriteManager->AngleToFrame(ship->bIsColliding);
        SDL_Texture* impact = spriteManager->GetSprite(SPRITE_SHIP_IMPACT, frame);
        if (impact) {
            SDL_Rect dest = {x - 16, y - 16, 32, 32};
            SDL_RenderCopy(graphics->GetRenderer(), impact, nullptr, &dest);
        }
    }
    if (ship->bIsGettingShot != NO_DAMAGE) {
        // Draw laser hit overlay
        int frame = spriteManager->AngleToFrame(ship->bIsGettingShot);
        SDL_Texture* laser = spriteManager->GetSprite(SPRITE_SHIP_LASER, frame);
        if (laser) {
            SDL_Rect dest = {x - 16, y - 16, 32, 32};
            SDL_RenderCopy(graphics->GetRenderer(), laser, nullptr, &dest);
        }
    }

    // Draw velocity vector if enabled
    if (useVelVectors) {
        const CTraj& vel = ship->GetVelocity();
        CCoord velCoord = vel.ConvertToCoord();
        int vx = x + static_cast<int>(velCoord.fX * 2);
        int vy = y + static_cast<int>(velCoord.fY * 2);
        graphics->DrawLine(x, y, vx, vy, Color(0, 255, 0));
    }

    // Draw name
    if (drawnames) {
        char info[64];
        snprintf(info, sizeof(info), "%s", ship->GetName());
        Color color = GetTeamColor(teamNum);
        graphics->DrawText(info, x + 18, y - 18, color, true);
    }

    // Draw laser beam
    DrawLaserBeam(ship, teamNum);
}

void ObserverSDL::DrawStationSprite(CStation* station, int teamNum) {
    if (!station || !spriteManager) return;

    const CCoord& pos = station->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    // Use station's actual orientation (omega = 0.9 rad/s)
    int frame = spriteManager->AngleToFrame(station->GetOrient());

    // Use actual team number from station
    int actualTeamNum = station->GetTeam()->GetTeamNumber();
    SDL_Texture* sprite = spriteManager->GetStationSprite(
        actualTeamNum, frame);

    if (sprite) {
        SDL_Rect dest = {x - 24, y - 24, 48, 48};  // Stations are bigger
        SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
    }

    // Draw damage overlays if being hit
    if (station->bIsColliding != NO_DAMAGE) {
        // Draw collision impact overlay
        int impactFrame = spriteManager->AngleToFrame(station->bIsColliding);
        SDL_Texture* impact = spriteManager->GetSprite(SPRITE_STATION_IMPACT, impactFrame);
        if (impact) {
            SDL_Rect dest = {x - 24, y - 24, 48, 48};
            SDL_RenderCopy(graphics->GetRenderer(), impact, nullptr, &dest);
        }
    }
    if (station->bIsGettingShot != NO_DAMAGE) {
        // Draw laser hit overlay
        int laserFrame = spriteManager->AngleToFrame(station->bIsGettingShot);
        SDL_Texture* laser = spriteManager->GetSprite(SPRITE_STATION_LASER, laserFrame);
        if (laser) {
            SDL_Rect dest = {x - 24, y - 24, 48, 48};
            SDL_RenderCopy(graphics->GetRenderer(), laser, nullptr, &dest);
        }
    }

    if (drawnames) {
        Color color = GetTeamColor(teamNum);
        const char* stationName = station->GetName();
        if (stationName && strlen(stationName) > 0) {
            graphics->DrawText(stationName, x + 26, y - 26, color, true);
        } else {
            graphics->DrawText("Station", x + 26, y - 26, color, true);
        }
    }
}

void ObserverSDL::DrawAsteroidSprite(CAsteroid* asteroid) {
    if (!asteroid || !spriteManager) return;

    const CCoord& pos = asteroid->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    // Use asteroid's actual orientation (omega = 1.0 rad/s)
    int frame = spriteManager->AngleToFrame(asteroid->GetOrient());

    bool isVinyl = (asteroid->GetMaterial() == VINYL);
    SDL_Texture* sprite = spriteManager->GetAsteroidSprite(
        isVinyl, asteroid->GetMass(), frame);

    if (sprite) {
        int size = (asteroid->GetMass() > 200.0) ? 32 : 24;
        SDL_Rect dest = {x - size/2, y - size/2, size, size};
        SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
    }

    if (drawnames) {
        const char* typeName = isVinyl ? "V" : "U";
        graphics->DrawText(typeName, x - 5, y - 5, Color(255, 255, 255), false);
    }
}

void ObserverSDL::Run() {
    if (!Initialize()) {
        std::cerr << "Failed to initialize Observer" << std::endl;
        return;
    }

    // Main loop
    bool running = true;
    // Uint32 lastTime = SDL_GetTicks();  // Unused for now
    const Uint32 frameDelay = 1000 / 60; // 60 FPS

    while (running) {
        Uint32 currentTime = SDL_GetTicks();

        running = HandleEvents();
        Update();
        Draw();

        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - currentTime;
        if (frameTime < frameDelay) {
            SDL_Delay(frameDelay - frameTime);
        }
    }
}