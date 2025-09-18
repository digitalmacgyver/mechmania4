/* ObserverSDL.C
 * SDL2-based Observer implementation
 */

#include "ObserverSDL.h"
#include "Station.h"
#include "Ship.h"
#include "Asteroid.h"
#include "World.h"  // For BAD_INDEX
#include <iostream>
#include <iomanip>
#include <sstream>

ObserverSDL::ObserverSDL(const char* regFileName, int gfxFlag)
    : graphics(nullptr), myWorld(nullptr), attractor(0) {

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
    if (graphics) {
        delete graphics;
    }
}

bool ObserverSDL::Initialize() {
    if (!graphics->Init()) {
        std::cerr << "Failed to initialize SDL2 graphics" << std::endl;
        return false;
    }

    // Get dimensions from graphics
    int displayWidth = graphics->GetDisplayWidth();
    int displayHeight = graphics->GetDisplayHeight();

    // Calculate layout
    spaceWidth = graphics->GetSpaceWidth();
    spaceHeight = graphics->GetSpaceHeight();
    borderX = static_cast<int>(displayWidth * 0.015);
    borderY = static_cast<int>((displayHeight - spaceHeight) * 0.1);

    // Team info positions
    t1PosX = borderX;
    t1PosY = spaceHeight + 2 * borderY;
    t2PosX = spaceWidth / 2;
    t2PosY = t1PosY;

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

    // Initialize message buffer
    messageBuffer.resize(MSG_ROWS);

    return true;
}

void ObserverSDL::Update() {
    // Update world state would happen here
    // For now, just placeholder
}

void ObserverSDL::Draw() {
    // Clear screen
    graphics->Clear(Color(40, 40, 40));

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
                    case SDLK_SPACE:
                        attractor = (attractor + 1) % 3;
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
        int gridStep = 50;

        for (int x = gridStep; x < spaceWidth; x += gridStep) {
            graphics->DrawLine(borderX + x, borderY,
                             borderX + x, borderY + spaceHeight,
                             gridColor);
        }
        for (int y = gridStep; y < spaceHeight; y += gridStep) {
            graphics->DrawLine(borderX, borderY + y,
                             borderX + spaceWidth, borderY + y,
                             gridColor);
        }
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
    if (ship) {
        DrawShip(ship, ship->GetWorldIndex()); // Assuming team number
        return;
    }

    CStation* station = dynamic_cast<CStation*>(thing);
    if (station) {
        DrawStation(station, 0); // Need to determine team
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

void ObserverSDL::DrawStation(CStation* station, int teamNum) {
    if (!station) return;

    const CCoord& pos = station->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    Color color = GetTeamColor(teamNum);

    // Draw station as square
    int size = 15;
    graphics->DrawRect(x - size, y - size, size * 2, size * 2, color, true);
    graphics->DrawRect(x - size - 2, y - size - 2, size * 2 + 4, size * 2 + 4, color, false);

    if (drawnames) {
        graphics->DrawText("Station", x + 20, y - 20, color, true);
    }
}

void ObserverSDL::DrawAsteroid(CAsteroid* asteroid) {
    if (!asteroid) return;

    const CCoord& pos = asteroid->GetPos();
    int x = WorldToScreenX(pos.fX);
    int y = WorldToScreenY(pos.fY);

    // Determine color based on type
    Color color;
    if (asteroid->GetMaterial() == URANIUM) {
        color = Color(0, 255, 100);
    } else if (asteroid->GetMaterial() == VINYL) {
        color = Color(255, 255, 0);
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

    // Draw team name
    graphics->DrawText(team->GetName(), x, y, color, false);

    // Draw score and ship info
    char info[256];
    snprintf(info, sizeof(info), "Score: %.0f  Ships: %d",
            team->GetScore(), team->GetShipCount());
    graphics->DrawText(info, x, y + 20, color, true);
}

void ObserverSDL::DrawMessages() {
    int y = msgPosY;
    Color msgColor(200, 200, 200);

    for (const auto& msg : messageBuffer) {
        if (!msg.empty()) {
            graphics->DrawText(msg, msgPosX, y, msgColor, true);
            y += 15;
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
    graphics->DrawText(timeStr, timeX, timeY, Color(255, 255, 255), false);
}

int ObserverSDL::WorldToScreenX(double wx) {
    // Assuming world coordinates are 0-1000
    return borderX + static_cast<int>((wx / 1000.0) * spaceWidth);
}

int ObserverSDL::WorldToScreenY(double wy) {
    return borderY + static_cast<int>((wy / 1000.0) * spaceHeight);
}

double ObserverSDL::ScreenToWorldX(int sx) {
    return ((sx - borderX) / static_cast<double>(spaceWidth)) * 1000.0;
}

double ObserverSDL::ScreenToWorldY(int sy) {
    return ((sy - borderY) / static_cast<double>(spaceHeight)) * 1000.0;
}

Color ObserverSDL::GetTeamColor(int teamNum) {
    switch (teamNum % 6) {
        case 0: return Color(255, 0, 0);     // Red
        case 1: return Color(0, 0, 255);     // Blue
        case 2: return Color(0, 255, 0);     // Green
        case 3: return Color(255, 255, 0);   // Yellow
        case 4: return Color(255, 0, 255);   // Magenta
        case 5: return Color(0, 255, 255);   // Cyan
        default: return Color(255, 255, 255); // White
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