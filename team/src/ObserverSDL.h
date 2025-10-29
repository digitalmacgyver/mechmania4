/* ObserverSDL.h
 * SDL2-based Observer for MechMania IV
 * Replaces X11-based Observer
 */

#ifndef _OBSERVERSDL_H_
#define _OBSERVERSDL_H_

#include <limits>
#include <string>
#include <vector>

#include "SDL2Graphics.h"
#include "Ship.h"
#include "SpriteManager.h"
#include "audio/AudioEventTracker.h"
#include "Team.h"
#include "Thing.h"
#include "World.h"

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
  bool isPaused;
  bool showStarfield;
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

  // Message display (unbounded history; render last lines that fit)
  struct Message {
    std::string text;
    int worldIndex;  // -1 for system messages, team world index for team
                     // messages
    int seconds;     // game time (whole seconds) when captured
  };
  std::vector<Message> messageBuffer;
  int audioControlsY = 0;
  int audioControlsHeight = 0;
  int audioControlsGap = 0;

  mm4::audio::AudioEventTracker audioEventTracker;
  bool audioInitialized = false;
  unsigned int lastAudioTurnProcessed = 0;
  std::string assetRootOverride_;
  bool verboseAudio_ = false;

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
  void DrawAnnouncerMessages();
  void DrawMessages();
  void DrawAudioControlsPanel();
  void DrawSpeakerIcon(int x, int y, bool muted, const Color& accent);
  void DrawTimeDisplay();
  void DrawStarfield();
  void DrawHelpFooter();
  void DrawLogo();
  void DrawVelocityVector(CThing* thing);

  // Coordinate transformation
  int WorldToScreenX(double wx);
  int WorldToScreenY(double wy);
  double ScreenToWorldX(int sx);
  double ScreenToWorldY(int sy);

  // Colors
  Color GetTeamColor(int teamIndex);  // Based on world index (connection order)
  Color GetThingColor(CThing* thing);

 public:
  ObserverSDL(const char* regFileName, int gfxFlag,
              const std::string& assetsRoot = std::string(),
              bool verboseAudio = false);
  ~ObserverSDL();

  // Main methods
  bool Initialize();
  void Update();
  void Draw();
  bool HandleEvents();

  // World management
  void SetWorld(CWorld* world) {
    if (myWorld != world) {
      myWorld = world;
      audioEventTracker.Reset();
      lastAudioTurnProcessed = std::numeric_limits<unsigned int>::max();
    }
  }
  CWorld* GetWorld() { return myWorld; }

  // Settings
  void SetAttractor(int val) { attractor = val; }
  void SetDrawNames(int val) { drawnames = val; }
  void ToggleVelVectors() { useVelVectors = !useVelVectors; }
  void ToggleSpriteMode() { useSpriteMode = !useSpriteMode; }
  void TogglePause();
  bool IsPaused() const { return isPaused; }

  // Message system
  void AddMessage(const std::string& msg, int worldIndex = -1);
  void ClearMessages();

  // Main loop
  void Run();
};

#endif  // _OBSERVERSDL_H_
