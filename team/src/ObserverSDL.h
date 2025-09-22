/* ObserverSDL.h
 * SDL2-based Observer for MechMania IV
 * Replaces X11-based Observer
 */

#ifndef _OBSERVERSDL_H_
#define _OBSERVERSDL_H_

#include "SDL2Graphics.h"
#include "SpriteManager.h"
#include "World.h"
#include "Team.h"
#include "Thing.h"
#include "Ship.h"
#include <string>
#include <vector>

class ObserverSDL {
private:
    SDL2Graphics* graphics;
    SpriteManager* spriteManager;
    CWorld* myWorld;
    SDL_Texture* logoTexture;

    // Display settings
    bool useXpm;
    bool useSpriteMode;
    bool useVelVectors;
    int drawnames;
    int attractor;

    // Layout dimensions
    int spaceWidth, spaceHeight;
    int msgWidth, msgHeight;
    int tWidth, tHeight;
    int borderX, borderY;
    int t1PosX, t1PosY;
    int t2PosX, t2PosY;
    int msgPosX, msgPosY;
    int timeX, timeY, timeWidth, timeHeight;

    // Message display
    static const int MSG_ROWS = 20;
    static const int MSG_COLS = 80;
    std::vector<std::string> messageBuffer;

    // Drawing helpers
    void DrawSpace();
    void DrawShip(CShip* ship, int teamNum);
    void DrawShipSprite(CShip* ship, int teamNum);
    void DrawLaserBeam(CShip* ship, int teamNum);
    void DrawStation(CStation* station, int teamNum);
    void DrawStationSprite(CStation* station, int teamNum);
    void DrawAsteroid(CAsteroid* asteroid);
    void DrawAsteroidSprite(CAsteroid* asteroid);
    void DrawThing(CThing* thing);
    void DrawTeamInfo(CTeam* team, int x, int y);
    void DrawMessages();
    void DrawTimeDisplay();
    void DrawStarfield();
    void DrawHelpFooter();
    void DrawLogo();

    // Coordinate transformation
    int WorldToScreenX(double wx);
    int WorldToScreenY(double wy);
    double ScreenToWorldX(int sx);
    double ScreenToWorldY(int sy);

    // Colors
    Color GetTeamColor(int teamIndex);  // Based on world index (connection order)
    Color GetThingColor(CThing* thing);

public:
    ObserverSDL(const char* regFileName, int gfxFlag);
    ~ObserverSDL();

    // Main methods
    bool Initialize();
    void Update();
    void Draw();
    bool HandleEvents();

    // World management
    void SetWorld(CWorld* world) { myWorld = world; }
    CWorld* GetWorld() { return myWorld; }

    // Settings
    void SetAttractor(int val) { attractor = val; }
    void SetDrawNames(int val) { drawnames = val; }
    void ToggleVelVectors() { useVelVectors = !useVelVectors; }
    void ToggleSpriteMode() { useSpriteMode = !useSpriteMode; }

    // Message system
    void AddMessage(const std::string& msg);
    void ClearMessages();

    // Main loop
    void Run();
};

#endif // _OBSERVERSDL_H_