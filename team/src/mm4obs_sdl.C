/* mm4obs_sdl.C
 * MechMania IV Observer executable using SDL2
 * Modernized version
 */

#include <unistd.h>   // for sleep

#ifdef USE_SDL2
    #include "ObserverSDL.h"
    #include <SDL2/SDL.h>  // for SDL_Delay
#else
    #include "Observer.h"
#endif

#include "Client.h"
#include "ParserModern.h"

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char *argv[])
{
    CParser PCmdLn(argc, argv);
    g_pParser = &PCmdLn;  // Set global parser instance
    if (PCmdLn.needhelp == 1) {
        printf("mm4obs [-R] [-G] [-pport] [-hhostname] [-ggfxreg]\n");
        printf("  -R:  Attempt reconnect after server disconnect\n");
        printf("  -G:  Activate full graphics mode\n");
        printf("  port defaults to 2323\n  hostname defaults to localhost\n");
        printf("  gfxreg defaults to graphics.reg\n");
        printf("MechMania IV: The Vinyl Frontier - SDL2 Edition\n");
        exit(1);
    }

    printf("Initializing graphics...\n");

#ifdef USE_SDL2
    ObserverSDL myObs(PCmdLn.gfxreg, PCmdLn.gfxflag);
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
    Uint32 lastConnectAttempt = 0;
    const Uint32 reconnectDelay = 3000; // 3 seconds in milliseconds

    // Initial connection attempt
    try {
        myClient = new CClient(PCmdLn.port, PCmdLn.hostname, true);
        connected = true;
        printf("Connected to server successfully\n");
    }
    catch (...) {
        printf("Failed to connect to server. ");
        if (PCmdLn.reconnect) {
            printf("Will retry in 3 seconds...\n");
            lastConnectAttempt = SDL_GetTicks();
        } else {
            printf("Running in standalone mode.\n");
        }
    }

    // Main loop - always responsive
    bool running = true;
    Uint32 lastReconnectAttempt = 0;

    bool prevPaused = false;
    while (running) {
        Uint32 currentTime = SDL_GetTicks();

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
                }
                catch (...) {
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
                printf("No reconnect flag, continuing in standalone mode\n");
            }
        }

        // Receive world state from server if connected
        if (connected && myClient) {
            UINT received = myClient->ReceiveWorld();

            if (received > 0) {
                // Send acknowledgment to server (critical for observer!)
                myClient->SendAck();

                CWorld* world = myClient->GetWorld();
                if (world) {
                    myObs.SetWorld(world);
                }
            }
        }

        // Send pause/resume control if toggled
        if (connected && myClient) {
            bool nowPaused = myObs.IsPaused();
            if (nowPaused != prevPaused) {
                if (nowPaused) myClient->SendPause(); else myClient->SendResume();
                prevPaused = nowPaused;
            }
        }

        // ALWAYS update and draw, even when disconnected
        myObs.Update();
        myObs.Draw();

        // ALWAYS handle events (non-blocking)
        running = myObs.HandleEvents();

        // Small delay to prevent CPU spinning (16ms = ~60 FPS)
        SDL_Delay(16);
    }

    if (myClient) {
        delete myClient;
    }
    return 0;
}
