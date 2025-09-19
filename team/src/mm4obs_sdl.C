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
#include "Parser.h"

int main(int argc, char *argv[])
{
    CParser PCmdLn(argc, argv);
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

    // Connect to server
    bool connected = false;
    CClient* myClient = nullptr;

    do {
        try {
            myClient = new CClient(PCmdLn.port, PCmdLn.hostname, true);
            connected = true;
            printf("Connected to server successfully\n");
        }
        catch (...) {
            printf("Failed to connect to server. ");
            if (PCmdLn.reconnect) {
                printf("Retrying in 3 seconds...\n");
                sleep(3);
            } else {
                printf("Exiting.\n");
                exit(1);
            }
        }
    } while (!connected && PCmdLn.reconnect);

    // Main loop
    bool running = true;
    while (running) {
        if (myClient->IsOpen() == 0) {
            printf("Disconnected from MechMania IV server\n");
            if (PCmdLn.reconnect) {
                printf("Attempting to reconnect...\n");
                delete myClient;
                sleep(3);

                try {
                    myClient = new CClient(PCmdLn.port, PCmdLn.hostname, true);
                    printf("Reconnected successfully\n");
                }
                catch (...) {
                    printf("Reconnection failed\n");
                    continue;
                }
            } else {
                printf("Terminating application\n");
                break;
            }
        }

        // Receive world state from server
        UINT received = myClient->ReceiveWorld();

        if (received > 0) {
            // Send acknowledgment to server (critical for observer!)
            myClient->SendAck();

            CWorld* world = myClient->GetWorld();
            if (world) {
                myObs.SetWorld(world);
            }

            // Update and draw after receiving new world state
            myObs.Update();
            myObs.Draw();
        }

        // Handle events (non-blocking)
        running = myObs.HandleEvents();

        // Small delay to prevent CPU spinning
        SDL_Delay(10);
    }

    delete myClient;
    return 0;
}