/* ObserverSDL.C
 * SDL2-based Observer implementation
 */

#include <algorithm>  // For std::min
#include <cmath>      // For cos, sin
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include "Asteroid.h"
#include "GameConstants.h"
#include "ObserverSDL.h"
#include "Ship.h"
#include "Station.h"
#include "Thing.h"      // Provides ship/thing state and sentinel constants
#include "World.h"      // For BAD_INDEX
#include "XPMLoader.h"  // For loading logo
#include "audio/AudioSystem.h"

ObserverSDL::ObserverSDL(const char* regFileName, int gfxFlag,
                         const std::string& assetsRoot, bool verboseAudio)
    : graphics(nullptr),
      spriteManager(nullptr),
      myWorld(nullptr),
      logoTexture(nullptr),
      useSpriteMode(gfxFlag == 1),
      attractor(0),
      audioInitialized(false),
      lastAudioTurnProcessed(std::numeric_limits<unsigned int>::max()),
      assetRootOverride_(assetsRoot),
      verboseAudio_(verboseAudio) {  // Enable sprite mode when -G is used
  (void)regFileName;

  drawnames = 1;
  isPaused = false;
  showStarfield = true;
  useVelVectors = false;
  audioControlsY = 0;
  audioControlsHeight = 0;
  audioControlsGap = 0;

  if (gfxFlag == 1) {
    useXpm = true;
    useVelVectors = false;  // No tactical display
  } else {
    useXpm = false;
    useVelVectors = true;  // Tactical display
  }

  audioEventTracker.Reset();
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
  if (audioInitialized) {
    mm4::audio::AudioSystem::Instance().Shutdown();
    audioInitialized = false;
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
    std::cerr << "Warning: Failed to load sprites, sprite mode disabled"
              << std::endl;
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

  // Compute character metrics for layout spacing
  int charW = 7, charH = 13;
  graphics->GetTextSize("W", charW, charH, true);
  int lineHeight = (charH > 0 ? charH : 13) + 1;

  // Time display area (3 lines tall)
  timeX = rightPanelX;
  timeY = borderY;
  timeWidth = rightPanelWidth;
  timeHeight = 3 * lineHeight;  // ~3 lines of text

  // Team 1 info area (7 lines tall + ~1/2 line)
  t1PosX = rightPanelX;
  t1PosY = timeY + timeHeight + lineHeight;  // ~1 line gap
  int teamInfoHeight =
      7 * lineHeight + (lineHeight / 2);  // add ~1/2 line for breathing room
  tHeight = teamInfoHeight;               // store for drawing

  // Team 2 info area (7 lines tall)
  t2PosX = rightPanelX;
  t2PosY = t1PosY + teamInfoHeight + lineHeight;  // ~1 line gap

  // Message area (remaining space at bottom)
  msgPosX = rightPanelX;
  msgPosY = t2PosY + teamInfoHeight + lineHeight;  // ~1 line gap
  msgWidth = rightPanelWidth;
  msgHeight = spaceHeight - (msgPosY - borderY);

  const int footerHeight = 25;  // Matches DrawHelpFooter layout
  audioControlsHeight = 0;
  audioControlsGap = 0;
  audioControlsY = displayHeight - footerHeight;
  int availableGap = displayHeight - (msgPosY + msgHeight) - footerHeight;
  if (availableGap > 10) {
    audioControlsGap = std::max(6, charH / 2);
    int desiredHeight = std::max(2 * lineHeight + 8, 36);
    audioControlsHeight = std::min(desiredHeight, availableGap);
    if (availableGap - audioControlsHeight < audioControlsGap) {
      audioControlsHeight = std::max(20, availableGap - audioControlsGap);
    }
    audioControlsHeight = std::max(0, audioControlsHeight);
    int footerGap = std::max(lineHeight, 14) + lineHeight;
    int minY = msgPosY + msgHeight + audioControlsGap;
    int desiredY = displayHeight - footerHeight - audioControlsHeight - footerGap;
    audioControlsY = std::max(minY, desiredY);
  }

  // Initialize message buffer (start empty; we keep full history)
  messageBuffer.clear();

  std::string soundConfigPath = "sound/defaults.txt";
  if (!std::filesystem::exists(soundConfigPath)) {
    const char* candidates[] = {"../sound/defaults.txt",
                                "../../sound/defaults.txt",
                                "../../../sound/defaults.txt"};
    for (const char* candidate : candidates) {
      if (std::filesystem::exists(candidate)) {
        soundConfigPath = candidate;
        break;
      }
    }
  }

  audioInitialized =
      mm4::audio::AudioSystem::Instance().Initialize(soundConfigPath,
                                                     assetRootOverride_,
                                                     verboseAudio_);
  if (!audioInitialized) {
    std::cerr << "Warning: Audio system failed to initialize" << std::endl;
  } else {
    audioEventTracker.Reset();
    lastAudioTurnProcessed = std::numeric_limits<unsigned int>::max();
  }

  return true;
}

void ObserverSDL::Update() {
  // Only update world state if not paused
  if (!isPaused && myWorld) {
    if (audioInitialized) {
      unsigned int currentTurn = myWorld->GetCurrentTurn();
      if (currentTurn != lastAudioTurnProcessed) {
        auto& audioSystem = mm4::audio::AudioSystem::Instance();
        audioSystem.BeginSubtick();
        auto events = audioEventTracker.GatherEvents(*myWorld);
        for (const auto& event : events) {
          audioSystem.QueueEffect(event);
        }
        audioSystem.EndSubtick();
        audioSystem.FlushPending(static_cast<int>(currentTurn));
        lastAudioTurnProcessed = currentTurn;
      }
    }
  }
}

void ObserverSDL::Draw() {
  // Clear screen with gray background
  graphics->Clear(Color(160, 160, 160));  // Gray #A0A0A0

  // Draw black background for space area
  graphics->DrawRect(borderX, borderY, spaceWidth, spaceHeight, Color(0, 0, 0),
                     true);

  // Right panel is already gray from Clear()

  // Draw black backgrounds for UI panels
  // Time area background
  graphics->DrawRect(timeX, timeY, timeWidth, timeHeight, Color(0, 0, 0), true);

  // Team 1 info area background (7 lines tall)
  graphics->DrawRect(t1PosX, t1PosY, msgWidth, tHeight, Color(0, 0, 0), true);

  // Team 2 info area background (7 lines tall)
  graphics->DrawRect(t2PosX, t2PosY, msgWidth, tHeight, Color(0, 0, 0), true);

  // Message area background
  graphics->DrawRect(msgPosX, msgPosY, msgWidth, msgHeight, Color(0, 0, 0),
                     true);

  // Draw main components
  if (showStarfield) {
    DrawStarfield();
  }
  DrawSpace();

  if (myWorld) {
    // Draw all things in the world
    // Iterate through all things using the linked list
    for (unsigned int i = myWorld->UFirstIndex; i != BAD_INDEX;
         i = myWorld->GetNextIndex(i)) {
      CThing* thing = myWorld->GetThing(i);
      if (thing) {
        DrawThing(thing);
      }
    }

    // Draw laser beams for all ships
    for (unsigned int t = 0; t < myWorld->GetNumTeams(); ++t) {
      CTeam* team = myWorld->GetTeam(t);
      if (team) {
        for (unsigned int s = 0; s < team->GetShipCount(); ++s) {
          CShip* ship = team->GetShip(s);
          if (ship && ship->IsAlive()) {
            DrawLaserBeam(ship, t);
          }
        }
      }
    }

    // Draw team info
    for (unsigned int t = 0; t < myWorld->GetNumTeams(); ++t) {
      CTeam* team = myWorld->GetTeam(t);
      if (team) {
        int x = (t == 0) ? t1PosX : t2PosX;
        int y = (t == 0) ? t1PosY : t2PosY;
        DrawTeamInfo(team, x, y);
      }
    }

    // Draw announcer messages
    DrawAnnouncerMessages();
  }

  DrawMessages();
  DrawAudioControlsPanel();
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
            // Cycle drawnames: 0 = off, 1 = human names, 2 = numeric/status
            // labels
            drawnames = (drawnames + 1) % 3;
            break;
          case SDLK_s:
            showStarfield = !showStarfield;
            std::cout << "Starfield: " << (showStarfield ? "ON" : "OFF")
                      << std::endl;
            break;
          case SDLK_m: {
            if (audioInitialized &&
                mm4::audio::AudioSystem::Instance().IsInitialized()) {
              auto& audioSystem = mm4::audio::AudioSystem::Instance();
              bool mute = !audioSystem.MusicMuted();
              audioSystem.SetMusicMuted(mute);
              std::cout << "Soundtrack: " << (mute ? "MUTED" : "ON")
                        << std::endl;
              AddMessage(mute ? "Soundtrack muted" : "Soundtrack unmuted",
                         -1);
            }
            break;
          }
          case SDLK_e: {
            if (audioInitialized &&
                mm4::audio::AudioSystem::Instance().IsInitialized()) {
              auto& audioSystem = mm4::audio::AudioSystem::Instance();
              bool mute = !audioSystem.EffectsMuted();
              audioSystem.SetEffectsMuted(mute);
              std::cout << "Effects: " << (mute ? "MUTED" : "ON")
                        << std::endl;
              AddMessage(mute ? "Sound effects muted"
                              : "Sound effects unmuted",
                         -1);
            }
            break;
          }
          case SDLK_v:
            ToggleVelVectors();
            break;
          case SDLK_g:
            ToggleSpriteMode();
            std::cout << "Sprite mode: " << (useSpriteMode ? "ON" : "OFF")
                      << std::endl;
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

void ObserverSDL::TogglePause() {
  isPaused = !isPaused;
  if (!audioInitialized) {
    return;
  }

  auto& audioSystem = mm4::audio::AudioSystem::Instance();
  if (isPaused) {
    audioSystem.PauseEffects();
  } else {
    audioSystem.ResumeEffects();
    lastAudioTurnProcessed = std::numeric_limits<unsigned int>::max();
  }
}

void ObserverSDL::DrawSpace() {
  // Draw space border only - starfield is already drawn
  graphics->DrawRect(borderX, borderY, spaceWidth, spaceHeight,
                     Color(100, 100, 100), false);

  // Draw grid if needed
  if (useVelVectors) {
    Color gridColor(60, 60, 60);
    int gridStep = spaceWidth / 8;  // 8x8 grid

    for (int i = 1; i < 8; ++i) {
      int x = borderX + i * gridStep;
      graphics->DrawLine(x, borderY, x, borderY + spaceHeight, gridColor);
    }
    for (int i = 1; i < 8; ++i) {
      int y = borderY + i * gridStep;
      graphics->DrawLine(borderX, y, borderX + spaceWidth, y, gridColor);
    }

    // Draw center lines slightly brighter
    Color centerColor(80, 80, 80);
    int centerX = borderX + spaceWidth / 2;
    int centerY = borderY + spaceHeight / 2;
    graphics->DrawLine(centerX, borderY, centerX, borderY + spaceHeight,
                       centerColor);
    graphics->DrawLine(borderX, centerY, borderX + spaceWidth, centerY,
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
    for (int i = 0; i < 2048; ++i) {
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
  if (!thing) {
    return;
  }

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
  // Velocity vector for generic things
  DrawVelocityVector(thing);
}

void ObserverSDL::DrawShip(CShip* ship, int teamNum) {
  if (!ship) {
    return;
  }

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

  // Velocity vector for ship (legacy style)
  DrawVelocityVector(ship);

  // Draw name/status below ship
  if (drawnames == 1) {
    const char* shipName = ship->GetName();
    if (shipName && strlen(shipName) > 0) {
      int textWidth = 0, textHeight = 0;
      graphics->GetTextSize(shipName, textWidth, textHeight, true);
      graphics->DrawText(shipName, x - textWidth / 2, y + 15, color, true,
                         true);
    }
  } else if (drawnames == 2) {
    // ship_number:shield:fuel:cargo (no decimals)
    int shipNum = 0;
    if (ship->GetTeam()) {
      CTeam* team = ship->GetTeam();
      for (unsigned int i = 0; i < team->GetShipCount(); ++i) {
        if (team->GetShip(i) == ship) {
          shipNum = static_cast<int>(i);
          break;
        }
      }
    }
    double sh = ship->GetAmount(S_SHIELD);
    double fu = ship->GetAmount(S_FUEL);
    double ca = ship->GetAmount(S_CARGO);
    char label[64];
    snprintf(label, sizeof(label), "%d:%.0f:%.0f:%.0f", shipNum, sh, fu, ca);
    int tw = 0, th = 0;
    graphics->GetTextSize(label, tw, th, true);
    graphics->DrawText(label, x - tw / 2, y + 15, color, true, true);
  }
}

void ObserverSDL::DrawLaserBeam(CShip* ship, int teamNum) {
  if (!ship) {
    return;
  }

  (void)teamNum;

  double laserRange = ship->GetLaserBeamDistance();
  if (laserRange <= 0.0) {
    return;  // No laser active
  }

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
  if (!station) {
    return;
  }

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
  double stationSize = 30.0;           // Default station size
  double worldSize = stationSize * 2;  // Square side length in world units

  // Convert world size to screen pixels
  int pixelWidth = static_cast<int>(worldSize * spaceWidth / 1024.0);
  int pixelHeight = static_cast<int>(worldSize * spaceHeight / 1024.0);

  // Draw station as empty square with 2 pixel stroke
  graphics->DrawRect(x - pixelWidth / 2, y - pixelHeight / 2, pixelWidth,
                     pixelHeight, color, false);
  graphics->DrawRect(x - pixelWidth / 2 + 1, y - pixelHeight / 2 + 1,
                     pixelWidth - 2, pixelHeight - 2, color, false);

  // Velocity vector for station
  DrawVelocityVector(station);

  if (drawnames == 1) {
    const char* stationName = station->GetName();
    const char* text =
        (stationName && strlen(stationName) > 0) ? stationName : "Station";
    int textWidth = 0, textHeight = 0;
    graphics->GetTextSize(text, textWidth, textHeight, true);
    graphics->DrawText(text, x - textWidth / 2, y + pixelHeight / 2 + 5, color,
                       true);
  } else if (drawnames == 2) {
    // team_id: score (3 decimals)
    int teamId = station->GetTeam()
                     ? static_cast<int>(station->GetTeam()->GetWorldIndex())
                     : 0;
    double score = station->GetVinylStore();
    char label[64];
    snprintf(label, sizeof(label), "%d: %.3f", teamId, score);
    int tw = 0, th = 0;
    graphics->GetTextSize(label, tw, th, true);
    graphics->DrawText(label, x - tw / 2, y + pixelHeight / 2 + 5, color, true,
                       true);
  }
}

void ObserverSDL::DrawAsteroid(CAsteroid* asteroid) {
  if (!asteroid) {
    return;
  }

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
    color = Color(255, 0, 255);  // Magenta #FF00FF
  } else {
    color = Color(128, 128, 128);
  }

  int radius = static_cast<int>(asteroid->GetSize());
  // Draw as empty circle with 2 pixel stroke
  graphics->DrawCircle(x, y, radius, color, false);
  graphics->DrawCircle(x, y, radius - 1, color, false);
  // Never show asteroid names per X11 style
  // Velocity vector for asteroid
  DrawVelocityVector(asteroid);
}

void ObserverSDL::DrawTeamInfo(CTeam* team, int x, int y) {
  if (!team) {
    return;
  }

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

  // Team header aligned with ship column start
  snprintf(info, sizeof(info), "%02d: %s", team->GetTeamNumber(),
           team->GetName());
  graphics->DrawText(info, x + 5, currentY, teamColor, false, true);
  currentY += lineHeight;

  // Station info line (gray Time:, team color station name)
  CStation* station = team->GetStation();
  if (station) {
    // Draw "Time: " in gray, bold with actual wait time
    char timeStr[32];
    snprintf(timeStr, sizeof(timeStr), "Time: %.2f", team->GetWallClock());
    graphics->DrawText(timeStr, x + 5, currentY, grayText, true, true);

    // Draw station name and vinyl in team color, bold, aligned with Fuel/Cap
    // column below
    int stationX = x + 5 + (23 * charW);  // align with Fuel/Cap column
    snprintf(info, sizeof(info), "%s: %.3f", station->GetName(),
             station->GetVinylStore());
    graphics->DrawText(info, stationX, currentY, teamColor, true, true);
  } else {
    char timeStr[64];
    snprintf(timeStr, sizeof(timeStr), "Time: %.2f         No Station",
             team->GetWallClock());
    graphics->DrawText(timeStr, x + 5, currentY, grayText, true, true);
  }
  currentY += lineHeight;

  // Compute column positions (monospace cell-based)
  int col0 = x + 5;  // Ship name
  int colSHD =
      x + 5 +
      (16 * charW);  // SHD column shifted 3 chars to the right from 13 -> 16
  int colFuel =
      x + 5 + (23 * charW);  // Fuel/Cap shifted 1 additional char (19 -> 23)
  int colVinyl = x + 5 + (34 * charW);  // Vinyl/Cap shifted right by ~5 chars

  // Column headers aligned to columns (no leading spaces)
  graphics->DrawText("Ship", col0, currentY, grayText, true, true);
  graphics->DrawText("SHD", colSHD, currentY, grayText, true, true);
  graphics->DrawText("Fuel/Cap", colFuel, currentY, grayText, true, true);
  graphics->DrawText("Vinyl/Cap", colVinyl, currentY, grayText, true, true);
  currentY += lineHeight;

  // Draw ships in tabular format
  for (unsigned int i = 0; i < team->GetShipCount() && i < 4; ++i) {
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

    for (int i = 0; i < maxTextLen && msg[i] != '\0'; ++i) {
      if (msg[i] == '\n' || linePos >= 255) {
        line[linePos] = '\0';
        if (linePos > 0) {
          // Message color is determined by world index; team name prefix is
          // redundant
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

void ObserverSDL::DrawAnnouncerMessages() {
  // Display announcer messages in white (worldIndex = -1)
  if (myWorld && strlen(myWorld->AnnouncerText) > 0) {
    // Process announcer messages line by line
    char* msg = myWorld->AnnouncerText;
    char line[256];
    int linePos = 0;

    for (int i = 0; i < myWorld->maxAnnouncerTextLen && msg[i] != '\0'; ++i) {
      if (msg[i] == '\n' || linePos >= 255) {
        line[linePos] = '\0';
        if (linePos > 0) {
          // Use worldIndex = -1 to display in white
          AddMessage(line, -1);
        }
        linePos = 0;
      } else {
        line[linePos++] = msg[i];
      }
    }
    // Add any remaining text
    if (linePos > 0) {
      line[linePos] = '\0';
      AddMessage(line, -1);
    }
  }
}

void ObserverSDL::DrawMessages() {
  // No header; render newest messages at the bottom and fill upward
  int charWidth = 7, charHeight = 13;
  graphics->GetTextSizeEx("W", charWidth, charHeight, true, true);
  if (charHeight <= 0) {
    charHeight = 13;
  }

  const int padX =
      3;  // slightly larger horizontal padding to avoid border bleed
  const int lineGap = 2;
  int usableHeight = msgHeight - 4;  // top/bottom padding
  int linesFit = usableHeight / (charHeight + lineGap);
  if (linesFit <= 0) {
    return;
  }

  // Prepare a list of wrapped display lines from newest to oldest
  struct WrappedLine {
    std::string text;
    Color color;
    int seconds;
    bool showTime;
    std::string timeStr;
    int timeW;
  };
  std::vector<WrappedLine> lines;
  lines.reserve(linesFit + 8);

  // Set a clip rect so drawing cannot spill into gray UI borders
  // Define an inner clip region that matches where we actually draw text
  const int clipLeft = msgPosX + padX + 1;
  const int iconReserve = 48;
  int clipRight = msgPosX + msgWidth - padX - 1 - iconReserve;
  if (clipRight <= clipLeft) {
    clipRight = clipLeft + 1;
  }
  const int clipTop = msgPosY + 1;
  const int clipBottom = msgPosY + msgHeight - 1;
  SDL_Rect clipRect = {clipLeft, clipTop, clipRight - clipLeft,
                       clipBottom - clipTop};
  SDL_RenderSetClipRect(graphics->GetRenderer(), &clipRect);

  // Walk messages newest → oldest and wrap by measured pixel width until we
  // have enough
  for (int i = (int)messageBuffer.size() - 1;
       i >= 0 && (int)lines.size() < linesFit; --i) {
    const auto& msg = messageBuffer[(size_t)i];
    if (msg.text.empty()) {
      continue;
    }
    Color msgColor(200, 200, 200);
    if (msg.worldIndex >= 0) {
      msgColor = GetTeamColor(msg.worldIndex);
    } else if (msg.worldIndex == -1) {
      msgColor = Color(255, 255, 255);  // White for announcer messages
    }

    // Build timestamp string and measure its width
    char tbuf[16];
    snprintf(tbuf, sizeof(tbuf), "%3ds ", std::max(0, msg.seconds));
    int timeW = 0, timeH = 0;
    graphics->GetTextSizeEx(tbuf, timeW, timeH, true, true);
    if (timeW <= 0) {
      timeW = 4 * (charWidth > 0 ? charWidth : 7);
    }

    // Pixel-accurate word wrap. First line width accounts for timestamp;
    // subsequent lines indent by same width. Available pixel width from the
    // text start to the inner clip right edge
    const int clipWidth = clipRight - clipLeft;
    const int borderFudge = (charWidth > 0 ? charWidth : 7);  // ~1 char safety
    const int maxWFirst = std::max(0, clipWidth - timeW - borderFudge);
    const int maxWNext = std::max(0, clipWidth - timeW - borderFudge);

    std::vector<std::string> wrapped;
    wrapped.reserve(4);
    std::istringstream iss(msg.text);
    std::string word;
    std::string accum;
    auto fits = [&](const std::string& s, int maxW) {
      int w = 0, h = 0;
      graphics->GetTextSizeEx(s, w, h, true, true);
      return w <= maxW || maxW == 0;
    };
    bool firstLine = true;
    int curMax = maxWFirst;
    while (iss >> word) {
      std::string candidate = accum.empty() ? word : (accum + " " + word);
      if (fits(candidate, curMax)) {
        accum = candidate;
      } else {
        if (!accum.empty()) {
          wrapped.push_back(accum);
        }
        // If a single word is too long, hard clip it into chunks that fit
        int w = 0, h = 0;
        graphics->GetTextSizeEx(word, w, h, true, true);
        if (w > curMax && curMax > 0) {
          std::string chunk;
          chunk.reserve(word.size());
          for (char c : word) {
            std::string tmp = chunk;
            tmp.push_back(c);
            int tw = 0, th = 0;
            graphics->GetTextSizeEx(tmp, tw, th, true, true);
            if (tw > curMax) {
              if (!chunk.empty()) {
                wrapped.push_back(chunk);
                chunk.clear();
              }
              // Switch to next line width after first wrap
              if (firstLine) {
                firstLine = false;
                curMax = maxWNext;
              }
            }
            chunk.push_back(c);
          }
          accum = chunk;
        } else {
          accum = word;
        }
        // Switch to next line width after first wrap
        if (firstLine) {
          firstLine = false;
          curMax = maxWNext;
        }
      }
    }
    if (!accum.empty()) {
      wrapped.push_back(accum);
    }

    // Push wrapped lines in reverse so the last segment is at bottom
    for (int w = (int)wrapped.size() - 1;
         w >= 0 && (int)lines.size() < linesFit; --w) {
      bool showTime = (w == 0);
      lines.push_back({wrapped[(size_t)w], msgColor, msg.seconds, showTime,
                       std::string(tbuf), timeW});
    }
  }

  // Draw from bottom upward
  int y = msgPosY + msgHeight - 2 - charHeight;  // bottom line position
  int leftX = clipLeft;
  Color grayText(160, 160, 160);
  for (size_t i = 0; i < lines.size(); ++i) {
    const auto& L = lines[i];
    int x = leftX;
    if (L.showTime) {
      graphics->DrawText(L.timeStr, x, y, grayText, true, true);
      x += L.timeW;
    } else {
      // indent to line up with wrapped content using measured timestamp width
      x += L.timeW;
    }
    graphics->DrawText(L.text, x, y, L.color, true, true);
    y -= (charHeight + lineGap);
    if (y < msgPosY + 2) {
      break;
    }
  }

  // Reset clip
  SDL_RenderSetClipRect(graphics->GetRenderer(), nullptr);
}

void ObserverSDL::DrawAudioControlsPanel() {
  if (audioControlsHeight <= 0 || audioControlsY <= 0) {
    return;
  }

  int panelX = msgPosX;
  int panelWidth = msgWidth;
  int panelY = audioControlsY;
  int panelHeight = audioControlsHeight;
  graphics->DrawRect(panelX, panelY, panelWidth, panelHeight, Color(0, 0, 0),
                     true);
  graphics->DrawRect(panelX, panelY, panelWidth, panelHeight,
                     Color(70, 70, 70), false);

  bool audioReady = audioInitialized &&
                    mm4::audio::AudioSystem::Instance().IsInitialized();
  bool musicMuted = true;
  bool effectsMuted = true;
  if (audioReady) {
    auto& audioSystem = mm4::audio::AudioSystem::Instance();
    musicMuted = audioSystem.MusicMuted();
    effectsMuted = audioSystem.EffectsMuted();
  }

  std::string musicLabel = "[M]ute soundtrack:";
  std::string effectsLabel = "Mute Sound [E]ffects:";

  int labelW = 0, labelH = 0;
  graphics->GetTextSize(musicLabel, labelW, labelH, true);
  int charW = 0, charH = 0;
  graphics->GetTextSize("W", charW, charH, true);
  int rowHeight = std::max(labelH + 4, 18);
  int verticalPadding = std::max(6, (panelHeight - 2 * rowHeight) / 3);
  int row1Y = panelY + verticalPadding;
  int shiftUp = std::min(rowHeight / 2, row1Y - panelY - 2);
  row1Y -= shiftUp;
  int row2Y = row1Y + rowHeight;
  if (row2Y + labelH > panelY + panelHeight - 2) {
    row2Y = panelY + panelHeight - labelH - 2;
    if (row2Y <= row1Y) {
      row2Y = row1Y + std::max(12, rowHeight / 2);
    }
  }

  const int iconReserve = 48;
  int iconWidth = 22;
  int spacing = 6;
  int iconAreaLeft = panelX + panelWidth - iconReserve;
  if (iconAreaLeft < panelX) {
    iconAreaLeft = panelX;
  }
  int iconX = iconAreaLeft + std::max(0, (iconReserve - iconWidth) / 2);
  Color activeColor(0, 220, 0);
  Color inactiveColor(130, 130, 130);

  Color musicColor = (!audioReady || musicMuted) ? inactiveColor : activeColor;
  int musicLabelW = 0, musicLabelH = 0;
  graphics->GetTextSize(musicLabel, musicLabelW, musicLabelH, true);
  int musicLabelX = iconAreaLeft - spacing - musicLabelW;
  const int labelPadding = 12;
  if (musicLabelX < panelX + labelPadding) {
    musicLabelX = panelX + labelPadding;
  }
  graphics->DrawText(musicLabel, musicLabelX, row1Y, musicColor, true, true);
  DrawSpeakerIcon(iconX, row1Y - 2, musicMuted || !audioReady, musicColor);

  Color fxColor = (!audioReady || effectsMuted) ? inactiveColor : activeColor;
  int effectsLabelW = 0, effectsLabelH = 0;
  graphics->GetTextSize(effectsLabel, effectsLabelW, effectsLabelH, true);
  int effectsLabelX = iconAreaLeft - spacing - effectsLabelW - (charW > 0 ? charW : 8);
  if (effectsLabelX < panelX + labelPadding) {
    effectsLabelX = panelX + labelPadding;
  }
  graphics->DrawText(effectsLabel, effectsLabelX, row2Y, fxColor, true, true);
  DrawSpeakerIcon(iconX, row2Y - 2, effectsMuted || !audioReady, fxColor);
}

void ObserverSDL::DrawSpeakerIcon(int x, int y, bool muted,
                                  const Color& accent) {
  (void)accent;
  int boxSize = 16;
  graphics->DrawRect(x, y, boxSize, boxSize, Color(25, 25, 25), true);
  graphics->DrawRect(x, y, boxSize, boxSize, Color(140, 140, 140), false);

  if (muted) {
    Color xColor(220, 0, 0);
    graphics->DrawLine(x + 3, y + 3, x + boxSize - 3, y + boxSize - 3, xColor);
    graphics->DrawLine(x + boxSize - 3, y + 3, x + 3, y + boxSize - 3, xColor);
  } else {
    Color checkColor(0, 200, 0);
    graphics->DrawLine(x + 3, y + boxSize - 5, x + 7, y + boxSize - 3,
                       checkColor);
    graphics->DrawLine(x + 7, y + boxSize - 3, x + boxSize - 3, y + 3,
                       checkColor);
  }
}

void ObserverSDL::DrawTimeDisplay() {
  if (!myWorld) {
    return;
  }

  char timeStr[64];
  double gameTime = myWorld->GetGameTime();

  // Display to tenth of second as per X11 style
  snprintf(timeStr, sizeof(timeStr), "Game Time: %.1f", gameTime);

  // Center text horizontally, align to top of time area
  int textWidth = 0, textHeight = 0;
  graphics->GetTextSize(timeStr, textWidth, textHeight, false);
  int centerX = timeX + (timeWidth / 2) - (textWidth / 2);

  // Draw at top of time area with bold white text
  graphics->DrawText(timeStr, centerX, timeY + 5, Color(255, 255, 255), false,
                     true);
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
    case 0:
      return Color(0xFF, 0xB5, 0x73);  // Orange #FFB573 (top-left spawn)
    case 1:
      return Color(0x00, 0xC6, 0x8C);  // Teal #00C68C (bottom-right spawn)
    case 2:
      return Color(0xFF, 0x11, 0xAC);  // Pink #FF11AC (bottom-left spawn)
    case 3:
      return Color(0xFF, 0xFF, 0x22);  // Yellow #FFFF22 (top-right spawn)
    case 4:
      return Color(255, 0, 255);  // Magenta (extra)
    case 5:
      return Color(0, 255, 255);  // Cyan (extra)
    default:
      return Color(255, 255, 255);  // White
  }
}

Color ObserverSDL::GetThingColor(CThing* thing) {
  if (!thing) {
    return Color(128, 128, 128);
  }

  // Could check thing type or properties here
  return Color(200, 200, 200);
}

void ObserverSDL::AddMessage(const std::string& msg, int worldIndex) {
  int secs = 0;
  if (myWorld) {
    double gt = myWorld->GetGameTime();
    if (gt >= 0.0) {
      secs = (int)gt;  // truncate to whole seconds
    }
  }
  messageBuffer.push_back({msg, worldIndex, secs});
}

void ObserverSDL::ClearMessages() {
  for (auto& msg : messageBuffer) {
    msg.text.clear();
    msg.worldIndex = -1;
  }
}

void ObserverSDL::DrawLogo() {
  if (!logoTexture) {
    return;
  }

  // Get logo dimensions
  int logoW, logoH;
  SDL_QueryTexture(logoTexture, nullptr, nullptr, &logoW, &logoH);

  if (attractor == 2) {
    // Mode 2: Opaque logo maximized within window (fit, keep aspect)
    int displayWidth = graphics->GetDisplayWidth();
    int displayHeight = graphics->GetDisplayHeight();

    // Scale logo to fit inside window while maintaining aspect ratio
    float scaleX = (float)displayWidth / logoW;
    float scaleY = (float)displayHeight / logoH;
    float scale =
        std::min(scaleX, scaleY);  // Use min to ensure it stays within bounds

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

  // Draw help text with better spacing
  Color helpColor(200, 200, 200);
  int textY = footerY + 5;
  int x = 10;
  int charW = 7, charH = 13;
  graphics->GetTextSize("W", charW, charH, true);

  // Title first at left
  const char* title = "MechMania IV: The Vinyl Frontier";
  int titleW = 0, titleH = 0;
  graphics->GetTextSize(title, titleW, titleH, true);
  graphics->DrawText(title, x, textY, Color(255, 255, 255), true);

  // Controls start ~15 characters to the right of title end
  int gapAfterTitleChars = 15;
  x = 10 + titleW + gapAfterTitleChars * (charW > 0 ? charW : 7);

  const char* controls[] = {
      "[S] Stars",        "[N] Names",     "[V] Velocities", "[G] Graphics",
      "[P] Pause/Resume", "[Spc] Credits", "[ESC/Q] Quit",   nullptr};

  int gapChars = 7;  // spread commands out more
  int gapPixels = gapChars * (charW > 0 ? charW : 7);
  for (int i = 0; controls[i] != nullptr; ++i) {
    int w = 0, h = 0;
    graphics->GetTextSize(controls[i], w, h, true);
    graphics->DrawText(controls[i], x, textY, helpColor, true);
    x += w + gapPixels;
  }

  // Right-side status (align to right using measured widths)
  const char* spriteStr = useSpriteMode ? "Sprites: ON" : "Sprites: OFF";
  const char* stateStr = isPaused ? "PAUSED" : "RUNNING";

  int w1 = 0, h1 = 0, w2 = 0, h2 = 0;
  graphics->GetTextSize(spriteStr, w1, h1, true);
  graphics->GetTextSize(stateStr, w2, h2, true);

  int rightGroupWidth = w1 + gapPixels + w2;
  int rightX = displayWidth - 10 - rightGroupWidth;

  graphics->DrawText(spriteStr, rightX, textY,
                     useSpriteMode ? Color(0, 255, 0) : Color(150, 150, 150),
                     true);
  graphics->DrawText(stateStr, rightX + w1 + gapPixels, textY,
                     isPaused ? Color(255, 255, 0) : Color(0, 255, 0), true,
                     true);
}

void ObserverSDL::DrawShipSprite(CShip* ship, int teamNum) {
  if (!ship || !spriteManager) {
    return;
  }

  const CCoord& pos = ship->GetPos();
  int x = WorldToScreenX(pos.fX);
  int y = WorldToScreenY(pos.fY);
  double orient = ship->GetOrient();

  // Get image set from ship (0=normal, 1=thrust, 2=brake, 3=left, 4=right)
  int imageSet = ship->GetImage();

  // Get the appropriate sprite - use world index for sprite selection
  int worldIndex = ship->GetTeam()->GetWorldIndex();
  SDL_Texture* sprite =
      spriteManager->GetShipSprite(worldIndex, imageSet, orient);

  if (sprite) {
    // Draw at native texture size for pixel-perfect crispness
    int tw = 0, th = 0;
    SDL_QueryTexture(sprite, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) {
      tw = 32;
      th = 32;
    }
    SDL_Rect dest = {x - tw / 2, y - th / 2, tw, th};
    SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
  }

  // Draw damage overlays if being hit
  if (ship->bIsColliding != g_no_damage_sentinel) {
    // Draw collision impact overlay
    int frame = spriteManager->AngleToFrame(ship->bIsColliding);
    SDL_Texture* impact = spriteManager->GetSprite(SPRITE_SHIP_IMPACT, frame);
    if (impact) {
      SDL_Rect dest = {x - 16, y - 16, 32, 32};
      SDL_RenderCopy(graphics->GetRenderer(), impact, nullptr, &dest);
    }
  }
  if (ship->bIsGettingShot != g_no_damage_sentinel) {
    // Draw laser hit overlay
    int frame = spriteManager->AngleToFrame(ship->bIsGettingShot);
    SDL_Texture* laser = spriteManager->GetSprite(SPRITE_SHIP_LASER, frame);
    if (laser) {
      SDL_Rect dest = {x - 16, y - 16, 32, 32};
      SDL_RenderCopy(graphics->GetRenderer(), laser, nullptr, &dest);
    }
  }

  // Velocity vector for ship
  DrawVelocityVector(ship);

  // Draw name centered below ship
  if (drawnames == 1) {
    const char* shipName = ship->GetName();
    if (shipName && strlen(shipName) > 0) {
      Color color = GetTeamColor(teamNum);
      int textWidth = 0, textHeight = 0;
      graphics->GetTextSize(shipName, textWidth, textHeight, true);
      graphics->DrawText(shipName, x - textWidth / 2, y + 20, color, true,
                         true);
    }
  } else if (drawnames == 2) {
    // Numeric label: ship_number:shield:fuel:cargo (no decimals)
    int shipNum = 0;
    if (ship->GetTeam()) {
      CTeam* team = ship->GetTeam();
      for (unsigned int i = 0; i < team->GetShipCount(); ++i) {
        if (team->GetShip(i) == ship) {
          shipNum = static_cast<int>(i);
          break;
        }
      }
    }
    double sh = ship->GetAmount(S_SHIELD);
    double fu = ship->GetAmount(S_FUEL);
    double ca = ship->GetAmount(S_CARGO);
    char label[64];
    snprintf(label, sizeof(label), "%d:%.0f:%.0f:%.0f", shipNum, sh, fu, ca);
    Color color = GetTeamColor(teamNum);
    int tw = 0, th = 0;
    graphics->GetTextSize(label, tw, th, true);
    graphics->DrawText(label, x - tw / 2, y + 20, color, true, true);
  }

  // Draw laser beam
  DrawLaserBeam(ship, teamNum);
}

void ObserverSDL::DrawStationSprite(CStation* station, int teamNum) {
  if (!station || !spriteManager) {
    return;
  }

  const CCoord& pos = station->GetPos();
  int x = WorldToScreenX(pos.fX);
  int y = WorldToScreenY(pos.fY);

  // Use station's actual orientation (omega = 0.9 rad/s)
  int frame = spriteManager->AngleToFrame(station->GetOrient());

  // Use world index for sprite selection
  int worldIndex = station->GetTeam()->GetWorldIndex();
  SDL_Texture* sprite = spriteManager->GetStationSprite(worldIndex, frame);

  if (sprite) {
    // Draw at native texture size
    int tw = 0, th = 0;
    SDL_QueryTexture(sprite, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) {
      tw = 48;
      th = 48;
    }
    SDL_Rect dest = {x - tw / 2, y - th / 2, tw, th};
    SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
  }

  // Draw damage overlays if being hit
  if (station->bIsColliding != g_no_damage_sentinel) {
    // Draw collision impact overlay
    int impactFrame = spriteManager->AngleToFrame(station->bIsColliding);
    SDL_Texture* impact =
        spriteManager->GetSprite(SPRITE_STATION_IMPACT, impactFrame);
    if (impact) {
      SDL_Rect dest = {x - 24, y - 24, 48, 48};
      SDL_RenderCopy(graphics->GetRenderer(), impact, nullptr, &dest);
    }
  }
  if (station->bIsGettingShot != g_no_damage_sentinel) {
    // Draw laser hit overlay
    int laserFrame = spriteManager->AngleToFrame(station->bIsGettingShot);
    SDL_Texture* laser =
        spriteManager->GetSprite(SPRITE_STATION_LASER, laserFrame);
    if (laser) {
      SDL_Rect dest = {x - 24, y - 24, 48, 48};
      SDL_RenderCopy(graphics->GetRenderer(), laser, nullptr, &dest);
    }
  }

  if (drawnames) {
    Color color = GetTeamColor(teamNum);
    if (drawnames == 1) {
      const char* stationName = station->GetName();
      const char* text =
          (stationName && strlen(stationName) > 0) ? stationName : "Station";
      int textWidth = 0, textHeight = 0;
      graphics->GetTextSize(text, textWidth, textHeight, true);
      graphics->DrawText(text, x - textWidth / 2, y + 30, color, true);
    } else if (drawnames == 2) {
      int teamId = station->GetTeam()
                       ? static_cast<int>(station->GetTeam()->GetWorldIndex())
                       : 0;
      double score = station->GetVinylStore();
      char label[64];
      snprintf(label, sizeof(label), "%d: %.3f", teamId, score);
      int tw = 0, th = 0;
      graphics->GetTextSize(label, tw, th, true);
      graphics->DrawText(label, x - tw / 2, y + 30, color, true, true);
    }
  }
}

void ObserverSDL::DrawAsteroidSprite(CAsteroid* asteroid) {
  if (!asteroid || !spriteManager) {
    return;
  }

  const CCoord& pos = asteroid->GetPos();
  int x = WorldToScreenX(pos.fX);
  int y = WorldToScreenY(pos.fY);

  // Use asteroid's actual orientation (omega = 1.0 rad/s)
  int frame = spriteManager->AngleToFrame(asteroid->GetOrient());

  bool isVinyl = (asteroid->GetMaterial() == VINYL);
  SDL_Texture* sprite =
      spriteManager->GetAsteroidSprite(isVinyl, asteroid->GetMass(), frame);

  if (sprite) {
    // Draw at native texture size to avoid any scaling blur
    int tw = 0, th = 0;
    SDL_QueryTexture(sprite, nullptr, nullptr, &tw, &th);
    if (tw <= 0 || th <= 0) {
      // Fallback: infer by mass if query fails
      int size = (asteroid->GetMass() > 200.0) ? 32 : 24;
      tw = th = size;
    }
    SDL_Rect dest = {x - tw / 2, y - th / 2, tw, th};
    SDL_RenderCopy(graphics->GetRenderer(), sprite, nullptr, &dest);
  }

  // Never show asteroid names in sprite mode
  // Velocity vector for asteroid (sprite mode)
  DrawVelocityVector(asteroid);
}

void ObserverSDL::DrawVelocityVector(CThing* thing) {
  if (!useVelVectors || !thing) {
    return;
  }

  const CTraj& vel = thing->GetVelocity();
  double speed = vel.rho;
  if (speed <= 0.0) {
    return;
  }
  // Clamp speed to world max (legacy behaviour)
  if (speed > g_game_max_speed) {
    speed = g_game_max_speed;
  }

  double theta = vel.theta;  // direction of motion
  double rad = thing->GetSize();
  const CCoord& pos = thing->GetPos();

  // Compute world-space endpoints from leading edge along velocity vector
  double ux = cos(theta);
  double uy = sin(theta);
  double srcX = pos.fX + rad * ux;
  double srcY = pos.fY + rad * uy;
  double dstX = pos.fX + (rad + speed) * ux;
  double dstY = pos.fY + (rad + speed) * uy;

  // Convert to screen pixels
  int x1 = WorldToScreenX(srcX);
  int y1 = WorldToScreenY(srcY);
  int x2 = WorldToScreenX(dstX);
  int y2 = WorldToScreenY(dstY);

  // Draw in white, 1px stroke
  graphics->DrawLine(x1, y1, x2, y2, Color(255, 255, 255));
}

void ObserverSDL::Run() {
  if (!Initialize()) {
    std::cerr << "Failed to initialize Observer" << std::endl;
    return;
  }

  // Main loop
  bool running = true;
  // Uint32 lastTime = SDL_GetTicks();  // Unused for now
  const Uint32 frameDelay = 1000 / 60;  // 60 FPS

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
