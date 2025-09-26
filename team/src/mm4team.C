/* mm4team.C
 * MechMania IV Observer executable
 * 9/15/1998 Misha Voloshin
 */

#include "Client.h"
#include "ParserModern.h"

// Global parser instance for feature flag access
CParser* g_pParser = nullptr;

int main(int argc, char* argv[]) {
  CParser PCmdLn(argc, argv);
  g_pParser = &PCmdLn;  // Set global parser instance
  if (PCmdLn.needhelp == 1) {
    printf("mm4team -pport -hhostname\n");
    printf("  port defaults to 2323\n  hostname defaults to localhost\n");
    printf("MechMania IV: The Vinyl Frontier   10/2/98\n");
    exit(1);
  }

  CClient myClient(PCmdLn.port, PCmdLn.hostname, false);

  while (1) {
    if (myClient.IsOpen() == 0) {
      printf("Disconnected from MechMania IV server\n");
      printf("Terminating application\n");
      break;
    }

    myClient.ReceiveWorld();
    myClient.DoTurn();
  }

  return 0;
}
