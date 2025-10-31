/* mm4obs_sdl.C
 * MechMania IV Observer executable using SDL2
 * Modernized version
 */

#include <unistd.h>  // for sleep
#include <cstdlib>   // for getenv
#include <cstring>   // for strcmp

#ifdef USE_SDL2
#include <SDL2/SDL.h>  // for SDL_Delay

#include "ObserverSDL.h"
#else
#include "Observer.h"
#endif

#include "Client.h"
#include "ParserModern.h"

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char* argv[]) {
  CParser PCmdLn(argc, argv);
  g_pParser = &PCmdLn;  // Set global parser instance
  if (PCmdLn.needhelp == 1) {
    printf("mm4obs [-R] [-G] [--verbose] [--mute] [-pport] [-hhostname] [-ggfxreg] [--assets-root path]\n");
    printf("  -R:  Attempt reconnect after server disconnect\n");
    printf("  -G:  Activate full graphics mode\n");
    printf("  --verbose: Show game time progress\n");
    printf("  --mute: Start observer with soundtrack and effects muted\n");
    printf("  --audio-lead-ms ms: Delay video draw to let audio lead (default 40, 0 when headless)\n");
    printf("  port defaults to 2323\n  hostname defaults to localhost\n");
    printf("  gfxreg defaults to graphics.reg\n");
    printf("MechMania IV: The Vinyl Frontier - SDL2 Edition\n");
    exit(1);
  }

  // Check if running in headless mode
  bool headlessMode = false;
  const char* sdlVideoDriver = getenv("SDL_VIDEODRIVER");
  if (sdlVideoDriver && strcmp(sdlVideoDriver, "dummy") == 0) {
    headlessMode = true;
    if (PCmdLn.verbose) {
      printf("Running in headless mode (SDL_VIDEODRIVER=dummy)\n");
    }
  }

  const char* dummyVideoDriver = getenv("DUMMY_VIDEO_DRIVER");
  bool suppressUiByEnv = dummyVideoDriver && dummyVideoDriver[0] != '\0';

  int audioLeadDelayMs = 40;
  bool audioLeadOverridden = false;
  if ((headlessMode || suppressUiByEnv) &&
      !PCmdLn.GetAudioLeadMilliseconds().has_value()) {
    audioLeadDelayMs = 0;
  }
  if (auto overrideLead = PCmdLn.GetAudioLeadMilliseconds()) {
    audioLeadDelayMs = *overrideLead;
    audioLeadOverridden = true;
  }
  if (audioLeadDelayMs < 0) {
    audioLeadDelayMs = 0;
  }

  int postDrawDelayMs = 16;
  if (audioLeadDelayMs >= postDrawDelayMs) {
    postDrawDelayMs = 0;
  } else {
    postDrawDelayMs -= audioLeadDelayMs;
  }

  if (PCmdLn.verbose) {
    const char* latencyReason = audioLeadOverridden
                                    ? "command-line override"
                                    : ((headlessMode || suppressUiByEnv)
                                           ? "headless default"
                                           : "default");
    printf("Audio lead latency: %d ms (%s)\n", audioLeadDelayMs,
           latencyReason);
  }

  printf("Initializing graphics...\n");

#ifdef USE_SDL2
  ObserverSDL myObs(PCmdLn.gfxreg, PCmdLn.gfxflag,
                    PCmdLn.GetAssetsRoot(), PCmdLn.verbose,
                    PCmdLn.enableAudioTestPing,
                    PCmdLn.startAudioMuted != 0,
                    PCmdLn.GetPlaylistSeed());
  printf("SDL2 Graphics initialized\n");
#else
  Observer myObs(PCmdLn.gfxreg, PCmdLn.gfxflag);
  printf("X11 Graphics initialized\n");
#endif

  myObs.SetAttractor(0);

  // Initialize the observer graphics
  if (!myObs.Initialize()) {
    printf("Failed to initialize observer graphics\n");
    exit(1);
  }

  printf("Connecting to server at %s:%d...\n", PCmdLn.hostname, PCmdLn.port);

  // Connect to server (non-blocking approach)
  CClient* myClient = nullptr;
  bool connected = false;
  const Uint32 reconnectDelay = 3000;  // 3 seconds in milliseconds
  Uint32 lastReconnectAttempt = 0;

  // Initial connection attempt
  try {
    myClient = new CClient(PCmdLn.port, PCmdLn.hostname, true);
    connected = true;
    printf("Connected to server successfully\n");
  } catch (...) {
    printf("Failed to connect to server. ");
    if (PCmdLn.reconnect) {
      printf("Will retry in 3 seconds...\n");
      lastReconnectAttempt = SDL_GetTicks();
    } else {
      printf("Exiting. Run with -R to wait for reconnect.\n");
      return 1;
    }
  }

  // Main loop - always responsive
  bool running = true;

  bool prevPaused = false;
  double lastGameTime = -1.0;

  while (running) {
    Uint32 currentTime = SDL_GetTicks();

    if (!headlessMode) {
      if (!myObs.HandleEvents()) {
        running = false;
        break;
      }
    }

    // Handle server connection/reconnection
    if (!connected && PCmdLn.reconnect) {
      // Try to reconnect periodically (non-blocking)
      if (currentTime - lastReconnectAttempt >= reconnectDelay) {
        printf("Attempting to reconnect...\n");
        lastReconnectAttempt = currentTime;

        if (myClient) {
          delete myClient;
          myClient = nullptr;
        }

        try {
          myClient = new CClient(PCmdLn.port, PCmdLn.hostname, true);
          connected = true;
          printf("Reconnected successfully\n");
        } catch (...) {
          printf("Reconnection failed, will retry...\n");
        }
      }
    }

    // Check if connected client is still alive
    if (connected && myClient && myClient->IsOpen() == 0) {
      printf("Disconnected from MechMania IV server\n");
      connected = false;

      if (PCmdLn.reconnect) {
        printf("Will attempt reconnection...\n");
        lastReconnectAttempt = currentTime;
      } else {
        printf("No reconnect flag, exiting observer.\n");
        break;
      }
    }

    // Receive world state from server if connected
    if (connected && myClient) {
      unsigned int received = 0;
      while ((received = myClient->ReceiveWorldNonBlocking()) > 0) {
        // Send acknowledgment to server (critical for observer!)
        myClient->SendAck();

        CWorld* world = myClient->GetWorld();
        if (world) {
          myObs.SetWorld(world);

          // Verbose output: print game time
          if (PCmdLn.verbose) {
            double currentGameTime = world->GetGameTime();
            if (currentGameTime != lastGameTime) {
              printf("t=%.1f\n", currentGameTime);
              fflush(stdout);
              lastGameTime = currentGameTime;
            }
          }
        }
      }
    }

    // Send pause/resume control if toggled
    if (connected && myClient) {
      bool nowPaused = myObs.IsPaused();
      if (nowPaused != prevPaused) {
        if (nowPaused) {
          myClient->SendPause();
        } else {
          myClient->SendResume();
        }
        prevPaused = nowPaused;
      }
    }

    myObs.Update();

    if (!headlessMode) {
      if (audioLeadDelayMs > 0) {
        SDL_Delay(static_cast<Uint32>(audioLeadDelayMs));
      }
      myObs.Draw();
    } else {
      // Headless mode: still run update so audio diagnostics can fire.
      // Keep running unless we lose connection
      running = connected || PCmdLn.reconnect;
      SDL_Delay(1);
    }

    if (!headlessMode && postDrawDelayMs > 0) {
      SDL_Delay(static_cast<Uint32>(postDrawDelayMs));
    }
  }

  if (myClient) {
    delete myClient;
  }
  return 0;
}
