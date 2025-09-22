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
    isPaused = false;
    showStarfield = true;
    useVelVectors = false;

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

    // Right panel dimensions
    int rightPanelX = 2 * borderX + spaceWidth;
    int rightPanelWidth = displayWidth - rightPanelX - borderX;

    // Time display area (3 lines tall)
    timeX = rightPanelX;
    timeY = borderY;
    timeWidth = rightPanelWidth;
    timeHeight = 45;  // ~3 lines of text

    // Team 1 info area (7 lines tall)
    t1PosX = rightPanelX;
    t1PosY = timeY + timeHeight + 5;
    int teamInfoHeight = 105;  // ~7 lines of text

    // Team 2 info area (7 lines tall)
    t2PosX = rightPanelX;
    t2PosY = t1PosY + teamInfoHeight + 5;

    // Message area (remaining space at bottom)
    msgPosX = rightPanelX;
    msgPosY = t2PosY + teamInfoHeight + 5;
    msgWidth = rightPanelWidth;
    msgHeight = spaceHeight - (msgPosY - borderY);

    // Initialize message buffer
    messageBuffer.resize(MSG_ROWS);

    return true;
}

void ObserverSDL::Update() {
    // Only update world state if not paused
    if (!isPaused && myWorld) {
        // World update would happen here
    }
}

void ObserverSDL::Draw() {
    // Clear screen with gray background
    graphics->Clear(Color(160, 160, 160));  // Gray #A0A0A0

    // Draw black background for space area
    graphics->DrawRect(borderX, borderY, spaceWidth, spaceHeight,
                      Color(0, 0, 0), true);

    // Right panel is already gray from Clear()

    // Draw black backgrounds for UI panels
    // Time area background
    graphics->DrawRect(timeX, timeY, timeWidth, timeHeight,
                      Color(0, 0, 0), true);

    // Team 1 info area background
    graphics->DrawRect(t1PosX, t1PosY, msgWidth, 105,
                      Color(0, 0, 0), true);

    // Team 2 info area background
    graphics->DrawRect(t2PosX, t2PosY, msgWidth, 105,
                      Color(0, 0, 0), true);

    // Message area background
    graphics->DrawRect(msgPosX, msgPosY, msgWidth, msgHeight,
                      Color(0, 0, 0), true);

    // Draw main components
    if (showStarfield) {
        DrawStarfield();
    }
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
                    case SDLK_s:
                        showStarfield = !showStarfield;
                        std::cout << "Starfield: " << (showStarfield ? "ON" : "OFF") << std::endl;
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
                    case SDLK_p:
                        // Toggle pause
                        TogglePause();
                        std::cout << (isPaused ? "PAUSED" : "RESUMED") << std::endl;
                        if (isPaused) {
                            AddMessage("Game PAUSED - Press P to resume", -1);
                        } else {
                            AddMessage("Game RESUMED", -1);
                        }
                        break;
                }
                break;
        }
    }
    return true;
}

void ObserverSDL::DrawSpace() {
    // Draw space border only - starfield is already drawn
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
        // Use world index for colors (connection order), not team number
        DrawShip(ship, ship->GetTeam()->GetWorldIndex());
        return;
    }

    CStation* station = dynamic_cast<CStation*>(thing);
    if (station && station->GetTeam()) {
        // Use world index for colors (connection order), not team number
        DrawStation(station, station->GetTeam()->GetWorldIndex());
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

    // Draw ship as "V" glyph (two lines from tip to rear points)
    double shipSize = 12.0;  // Default ship size in world units
    double factor = 0.7071;  // sqrt(2)/2 to keep within circle

    // World scaling factors
    double sclx = spaceWidth / 1024.0;
    double scly = spaceHeight / 1024.0;

    // Calculate ship points in world units, then scale to screen
    // Tip point at orientation angle
    int tipX = x + static_cast<int>(factor * shipSize * cos(orient) * sclx);
    int tipY = y + static_cast<int>(factor * shipSize * sin(orient) * scly);

    // Rear-left point (orient + 120 degrees)
    double angle1 = orient + (2.0 * 3.14159265359 / 3.0);
    int x1 = x + static_cast<int>(factor * shipSize * cos(angle1) * sclx);
    int y1 = y + static_cast<int>(factor * shipSize * sin(angle1) * scly);

    // Rear-right point (orient + 240 degrees)
    double angle2 = orient + (4.0 * 3.14159265359 / 3.0);
    int x2 = x + static_cast<int>(factor * shipSize * cos(angle2) * sclx);
    int y2 = y + static_cast<int>(factor * shipSize * sin(angle2) * scly);

    // Draw two lines from tip to rear points (V shape) - weight 2
    graphics->DrawLine(tipX, tipY, x1, y1, color);
    graphics->DrawLine(tipX + 1, tipY, x1 + 1, y1, color);  // 2nd line for weight
    graphics->DrawLine(tipX, tipY, x2, y2, color);
    graphics->DrawLine(tipX + 1, tipY, x2 + 1, y2, color);  // 2nd line for weight

    // Draw velocity vector if enabled
    if (useVelVectors) {
        const CTraj& vel = ship->GetVelocity();
        CCoord velCoord = vel.ConvertToCoord();
        int vx = x + static_cast<int>(velCoord.fX * 2);
        int vy = y + static_cast<int>(velCoord.fY * 2);
        graphics->DrawLine(x, y, vx, vy, Color(0, 255, 0));
    }

    // Draw name centered below ship
    if (drawnames) {
        const char* shipName = ship->GetName();
        if (shipName && strlen(shipName) > 0) {
            int textWidth = 0, textHeight = 0;
            graphics->GetTextSize(shipName, textWidth, textHeight, true);
            graphics->DrawText(shipName, x - textWidth/2, y + 15, color, true, true);
        }
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

    // Draw laser beam in red (#FF0000), weight 2
    Color laserColor(255, 0, 0);  // Red laser #FF0000

    // Draw weight 2 laser beam
    graphics->DrawLine(startX, startY, endX, endY, laserColor);
    graphics->DrawLine(startX + 1, startY, endX + 1, endY, laserColor);
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

    // Station size in world units (default is 30, so square is 60x60 world units)
    double stationSize = 30.0;  // Default station size
    double worldSize = stationSize * 2;  // Square side length in world units

    // Convert world size to screen pixels
    int pixelWidth = static_cast<int>(worldSize * spaceWidth / 1024.0);
    int pixelHeight = static_cast<int>(worldSize * spaceHeight / 1024.0);

    // Draw station as empty square with 2 pixel stroke
    graphics->DrawRect(x - pixelWidth/2, y - pixelHeight/2, pixelWidth, pixelHeight, color, false);
    graphics->DrawRect(x - pixelWidth/2 + 1, y - pixelHeight/2 + 1, pixelWidth - 2, pixelHeight - 2, color, false);

    if (drawnames) {
        const char* stationName = station->GetName();
        if (stationName && strlen(stationName) > 0) {
            // Center name below station
            int textWidth = 0, textHeight = 0;
            graphics->GetTextSize(stationName, textWidth, textHeight, true);
            graphics->DrawText(stationName, x - textWidth/2, y + pixelHeight/2 + 5, color, true);
        } else {
            graphics->DrawText("Station", x - 24, y + pixelHeight/2 + 5, color, true);
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
    // Draw as empty circle with 2 pixel stroke
    graphics->DrawCircle(x, y, radius, color, false);
    graphics->DrawCircle(x, y, radius - 1, color, false);
    // Never show asteroid names per X11 style
}

void ObserverSDL::DrawTeamInfo(CTeam* team, int x, int y) {
    if (!team) return;

    // Use world index for color (connection order)
    Color teamColor = GetTeamColor(team->GetWorldIndex());
    Color whiteText(255, 255, 255);
    Color grayText(160, 160, 160);  // #A0A0A0
    // Use actual small-font height + 1px for spacing
    int charW = 7, charH = 13;
    graphics->GetTextSize("W", charW, charH, true);
    int lineHeight = charH + 1;
    int currentY = y + 2;  // Start slightly down from top
    char info[256];

    // Team header: "DD: TEAM_NAME" (display team's chosen number, not world index)
    snprintf(info, sizeof(info), "%02d: %s", team->GetTeamNumber(), team->GetName());
    graphics->DrawText(info, x, currentY, teamColor, false, true);
    currentY += lineHeight;

    // Station info line (gray Time:, team color station name)
    CStation* station = team->GetStation();
    if (station) {
        // Draw "Time: " in gray, bold with actual wait time
        char timeStr[32];
        snprintf(timeStr, sizeof(timeStr), "Time: %.2f", team->GetWallClock());
        graphics->DrawText(timeStr, x, currentY, grayText, true, true);

        // Draw station name and vinyl in team color, bold
        snprintf(info, sizeof(info), "         %s: %.3f",
                station->GetName(), station->GetVinylStore());
        graphics->DrawText(info, x + 70, currentY, teamColor, true, true);
    } else {
        char timeStr[64];
        snprintf(timeStr, sizeof(timeStr), "Time: %.2f         No Station", team->GetWallClock());
        graphics->DrawText(timeStr, x, currentY, grayText, true, true);
    }
    currentY += lineHeight;

    // Compute column positions (monospace cell-based)
    int col0  = x + 5;                    // Ship name
    int colSHD = x + 5 + (16 * charW);    // SHD column shifted 3 chars to the right from 13 -> 16
    int colFuel = x + 5 + (23 * charW);   // Fuel/Cap shifted 1 additional char (19 -> 23)
    int colVinyl = x + 5 + (29 * charW);  // Vinyl/Cap unchanged

    // Column headers aligned to columns (no leading spaces)
    graphics->DrawText("Ship",      col0,    currentY, grayText, true, true);
    graphics->DrawText("SHD",       colSHD,  currentY, grayText, true, true);
    graphics->DrawText("Fuel/Cap",  colFuel, currentY, grayText, true, true);
    graphics->DrawText("Vinyl/Cap", colVinyl,currentY, grayText, true, true);
    currentY += lineHeight;

    // Draw ships in tabular format
    for (UINT i = 0; i < team->GetShipCount() && i < 4; i++) {
        CShip* ship = team->GetShip(i);
        if (ship && ship->IsAlive()) {
            const char* shipName = ship->GetName();
            if (!shipName || strlen(shipName) == 0) {
                shipName = "Ship";
            }

            // Get ship resources
            double fuel = ship->GetAmount(S_FUEL);
            double fuelMax = ship->GetCapacity(S_FUEL);
            double cargo = ship->GetAmount(S_CARGO);
            double cargoMax = ship->GetCapacity(S_CARGO);
            double shield = ship->GetAmount(S_SHIELD);

            // Determine shield color
            Color shieldColor;
            if (shield > 12.5) {
                shieldColor = Color(0, 255, 0);  // Green
            } else if (shield >= 5.0) {
                shieldColor = Color(255, 255, 0);  // Yellow
            } else {
                shieldColor = Color(255, 0, 0);  // Red
            }

            // Determine fuel color
            double fuelPercent = (fuelMax > 0) ? (fuel / fuelMax) * 100.0 : 0;
            Color fuelColor;
            if (fuelPercent > 50.0) {
                fuelColor = Color(0, 255, 0);  // Green
            } else if (fuelPercent >= 20.0) {
                fuelColor = Color(255, 255, 0);  // Yellow
            } else {
                fuelColor = Color(255, 0, 0);  // Red
            }

            // Format ship name (team color)
            graphics->DrawText(shipName, col0, currentY, teamColor, true, true);

            // Format shield
            char shieldStr[16];
            snprintf(shieldStr, sizeof(shieldStr), "%.1f", shield);
            graphics->DrawText(shieldStr, colSHD, currentY, shieldColor, true, true);

            // Format fuel
            char fuelStr[32];
            snprintf(fuelStr, sizeof(fuelStr), "%.1f/%.1f", fuel, fuelMax);
            graphics->DrawText(fuelStr, colFuel, currentY, fuelColor, true, true);

            // Format cargo
            char cargoStr[32];
            snprintf(cargoStr, sizeof(cargoStr), "%.1f/%.1f", cargo, cargoMax);
            graphics->DrawText(cargoStr, colVinyl, currentY, whiteText, true, true);

            currentY += lineHeight;
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
                    // Message color is determined by world index; team name prefix is redundant
                    AddMessage(line, team->GetWorldIndex());
                }
                linePos = 0;
            } else {
                line[linePos++] = msg[i];
            }
        }
        // Add any remaining text
        if (linePos > 0) {
            line[linePos] = '\0';
            AddMessage(line, team->GetWorldIndex());
        }

        // Clear the team's message buffer after displaying
        team->MsgText[0] = '\0';
    }
}

void ObserverSDL::DrawMessages() {
    // Draw message header
    graphics->DrawText("Messages:", msgPosX + 2, msgPosY + 2, Color(200, 200, 200), true, true);

    int y = msgPosY + 18;  // Start below header

    // Measure font height once
    int charWidth = 7, charHeight = 13;
    graphics->GetTextSize("W", charWidth, charHeight, true);
    if (charHeight <= 0) charHeight = 13;

    // Padding within the message box
    const int padX = 2;
    int leftX = msgPosX + padX;
    int rightX = msgPosX + msgWidth - padX;
    int maxPixelWidth = rightX - leftX;

    for (const auto& msg : messageBuffer) {
        if (!msg.text.empty()) {
            // Determine color based on stored world index
            Color msgColor(200, 200, 200);  // Default gray for system messages
            if (msg.worldIndex >= 0) {
                msgColor = GetTeamColor(msg.worldIndex);
            }

            // Pixel-accurate word wrap within the message box
            std::istringstream iss(msg.text);
            std::string word;
            std::string lineAccum;
            while (iss >> word) {
                std::string candidate = lineAccum.empty() ? word : (lineAccum + " " + word);
                int w = 0, h = 0;
                graphics->GetTextSize(candidate, w, h, true);
                if (w <= maxPixelWidth) {
                    lineAccum = candidate;
                } else {
                    if (!lineAccum.empty()) {
                        graphics->DrawText(lineAccum, leftX, y, msgColor, true, true);
                        y += (charHeight + 2);
                        if (y >= (msgPosY + msgHeight - 2)) break; // Stop drawing if no space
                    }
                    // If single word too long, hard clip by characters to fit
                    int ww = 0, hh = 0;
                    graphics->GetTextSize(word, ww, hh, true);
                    if (ww > maxPixelWidth) {
                        // Find max characters that fit
                        std::string clipped;
                        clipped.reserve(word.size());
                        for (size_t i = 0; i < word.size(); ++i) {
                            std::string tmp = clipped + word[i];
                            int tw = 0, th = 0;
                            graphics->GetTextSize(tmp, tw, th, true);
                            if (tw > maxPixelWidth) break;
                            clipped.push_back(word[i]);
                        }
                        lineAccum = clipped;
                    } else {
                        lineAccum = word;
                    }
                }
            }
            if (!lineAccum.empty() && y < (msgPosY + msgHeight - 2)) {
                graphics->DrawText(lineAccum, leftX, y, msgColor, true, true);
                y += (charHeight + 2);
            }
        }
    }
}

void ObserverSDL::DrawTimeDisplay() {
    if (!myWorld) return;

    char timeStr[64];
    double gameTime = myWorld->GetGameTime();

    // Display to tenth of second as per X11 style
    snprintf(timeStr, sizeof(timeStr), "Game Time: %.1f", gameTime);

    // Center text horizontally, align to top of time area
    int textWidth = 0, textHeight = 0;
    graphics->GetTextSize(timeStr, textWidth, textHeight, false);
    int centerX = timeX + (timeWidth / 2) - (textWidth / 2);

    // Draw at top of time area with bold white text
    graphics->DrawText(timeStr, centerX, timeY + 5, Color(255, 255, 255), false, true);
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

Color ObserverSDL::GetTeamColor(int teamIndex) {
    // Colors based on world index (connection order)
    // Index 0: first team to connect (spawns top-left)
    // Index 1: second team to connect (spawns bottom-right)
    // Index 2: third team to connect (spawns bottom-left)
    // Index 3: fourth team to connect (spawns top-right)
    switch (teamIndex % 6) {
        case 0: return Color(0xFF, 0xB5, 0x73);  // Orange #FFB573 (top-left spawn)
        case 1: return Color(0x00, 0xC6, 0x8C);  // Teal #00C68C (bottom-right spawn)
        case 2: return Color(0xFF, 0x11, 0xAC);  // Pink #FF11AC (bottom-left spawn)
        case 3: return Color(0xFF, 0xFF, 0x22);  // Yellow #FFFF22 (top-right spawn)
        case 4: return Color(255, 0, 255);       // Magenta (extra)
        case 5: return Color(0, 255, 255);       // Cyan (extra)
        default: return Color(255, 255, 255);    // White
    }
}

Color ObserverSDL::GetThingColor(CThing* thing) {
    if (!thing) return Color(128, 128, 128);

    // Could check thing type or properties here
    return Color(200, 200, 200);
}

void ObserverSDL::AddMessage(const std::string& msg, int worldIndex) {
    // Shift messages up
    for (size_t i = 0; i < messageBuffer.size() - 1; i++) {
        messageBuffer[i] = messageBuffer[i + 1];
    }
    messageBuffer.back() = {msg, worldIndex};
}

void ObserverSDL::ClearMessages() {
    for (auto& msg : messageBuffer) {
        msg.text.clear();
        msg.worldIndex = -1;
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

    // Draw help text (measured widths for exact spacing)
    Color helpColor(200, 200, 200);
    int textY = footerY + 5;
    int x = 10;
    const int gap = 20;  // padding between items

    // Left-side items
    const char* items[] = {
        "MechMania IV: The Vinyl Frontier",
        "[S] Stars",
        "[N] Names",
        "[V] Velocities",
        "[G] Graphics",
        "[P] Pause/Resume",
        "[Spc] Credits",
        "[ESC/Q] Quit",
        nullptr
    };

    // Colors per item (title is white, others help gray)
    for (int i = 0; items[i] != nullptr; i++) {
        int w = 0, h = 0;
        graphics->GetTextSize(items[i], w, h, true);
        Color c = (i == 0) ? Color(255, 255, 255) : helpColor;
        graphics->DrawText(items[i], x, textY, c, true);
        x += w + gap;
    }

    // Right-side status (align to right using measured widths)
    const char* spriteStr = useSpriteMode ? "Sprites: ON" : "Sprites: OFF";
    const char* stateStr = isPaused ? "PAUSED" : "RUNNING";

    int w1 = 0, h1 = 0, w2 = 0, h2 = 0;
    graphics->GetTextSize(spriteStr, w1, h1, true);
    graphics->GetTextSize(stateStr, w2, h2, true);

    int rightGroupWidth = w1 + gap + w2;
    int rightX = displayWidth - 10 - rightGroupWidth;

    graphics->DrawText(spriteStr, rightX, textY, useSpriteMode ? Color(0, 255, 0) : Color(150, 150, 150), true);
    graphics->DrawText(stateStr, rightX + w1 + gap, textY, isPaused ? Color(255, 255, 0) : Color(0, 255, 0), true, true);
}

void ObserverSDL::DrawShipSprite(CShip* ship, int teamNum) {
    if (!ship || !spriteManager) return;

    const CCoord& pos = ship->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);
    double orient = ship->GetOrient();

    // Get image set from ship (0=normal, 1=thrust, 2=brake, 3=left, 4=right)
    int imageSet = ship->GetImage();

    // Get the appropriate sprite - use world index for sprite selection
    int worldIndex = ship->GetTeam()->GetWorldIndex();
    SDL_Texture* sprite = spriteManager->GetShipSprite(
        worldIndex, imageSet, orient);

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

    // Draw name centered below ship
    if (drawnames) {
        const char* shipName = ship->GetName();
        if (shipName && strlen(shipName) > 0) {
            Color color = GetTeamColor(teamNum);
            int textWidth = 0, textHeight = 0;
            graphics->GetTextSize(shipName, textWidth, textHeight, true);
            graphics->DrawText(shipName, x - textWidth/2, y + 20, color, true, true);
        }
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

    // Use world index for sprite selection
    int worldIndex = station->GetTeam()->GetWorldIndex();
    SDL_Texture* sprite = spriteManager->GetStationSprite(
        worldIndex, frame);

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
            // Center the text below the station
            int textWidth = 0, textHeight = 0;
            graphics->GetTextSize(stationName, textWidth, textHeight, true);
            graphics->DrawText(stationName, x - textWidth/2, y + 30, color, true);
        } else {
            // Center "Station" below the station
            graphics->DrawText("Station", x - 24, y + 30, color, true);
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

    // Never show asteroid names in sprite mode
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
